/**
 * @file        http_server.cpp
 * @brief       HTTP server
 * @author      Copyright (C) Peter Ivanov, 2011, 2012, 2013, 2014, 2021, 2023, 2024
 *
 * Created      2011-01-19 11:48:53
 * Last modify: 2024-11-23 12:49:30 ivanovp {Time-stamp}
 * Licence:     GPL
 */

#include <Arduino.h>
#include <FS.h>       // File System for Web Server Files
#include <LittleFS.h> // This file system is used.
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
// Example firmware upload via curl:
// curl -F "image=@.pio/build/esp07/firmware.bin" http://doorbell10c488.home/update.htm
#include <ESP8266HTTPUpdateServer.h>

#include "main.h"
#include "common.h"
#include "config.h"
#include "secrets.h" // add WLAN Credentials in here.
#include "http_server.h"
#include "fileutils.h"
#include "trace.h"
#include "doorbell.h"

#if ENABLE_HTTP_SERVER
// need a WebServer for http access on port 80.
ESP8266WebServer httpServer(HTTP_SERVER_PORT);
#if ENABLE_FIRMWARE_UPDATE
ESP8266HTTPUpdateServer httpUpdater;
#endif
uint32_t homepageRefreshInterval_sec = DEFAULT_HOMEPAGE_REFRESH_INTERVAL_SEC; /* First line of homepage_refresh_interval.txt */
String homepageTitleStr = TITLE_STR;   /* First line of homepage_texts.txt */
#if ENABLE_HTTP_AUTH
/* true: include HTTP pages of httpPages for authentication */
/* false: exclude HTTP pages of httpPages for authentication */
bool includeHttpAuthPages = true;
String httpAuthPages[MAX_AUTH_PAGES];
uint32_t httpAuthPageNumber = 0;
#endif /* ENABLE_HTTP_AUTH */
#endif /* ENABLE_HTTP_SERVER */


#if ENABLE_HTTP_SERVER
// The text of builtin files are in this header file
#include "builtinfiles.h"

const String& html_link_to_index()
{
    static const String str = String("<a href=\"" INDEX_HTM "\">Index</a>");
    return str;
}

#if ENABLE_FILE_TRACE
const String& html_link_to_trace_log()
{
    static const String str = String("Trace log: <a href=\"" TRACE_FILE_NAME "\">" TRACE_FILE_NAME "</a>");
    return str;
}
#endif

String& html_begin(bool a_scalable, String a_title, String a_heading,
    int a_homepage_refresh_interval_sec , String a_homepage_redirect)
{
    static String buf;
    int refresh_sec = 0;
    String redirectMsg = "";

    buf = "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"";
    buf += "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">";
    buf += "<html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"en\" xml:lang=\"en\">";
    buf += "<head><meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\"/>";
    if (a_homepage_refresh_interval_sec < 0 && homepageRefreshInterval_sec > 0)
    {
        refresh_sec = homepageRefreshInterval_sec;
    }
    else if (a_homepage_refresh_interval_sec > 0)
    {
        refresh_sec = a_homepage_refresh_interval_sec;
    }
    if (refresh_sec > 0 || a_homepage_redirect.length())
    {
        if (!a_homepage_redirect.length())
        {
            buf += "<meta http-equiv=\"refresh\" content=\"" + String(refresh_sec) + "\"/>"; // automatically reload in 60 seconds
        }
        else
        {
            buf += "<meta http-equiv=\"refresh\" content=\"" + String(refresh_sec) + "; URL=" + a_homepage_redirect + "\" />";
            redirectMsg = "You will be redirected to <a href=\"" + a_homepage_redirect + "\">";
            redirectMsg += a_homepage_redirect + "</a> in " + sec2str(refresh_sec) + ".";
        }
    }
    buf += "<meta name=\"mobile-web-app-capable\" content=\"yes\">";
    if (!a_scalable)
    {
        buf += "<meta name=\"viewport\" content=\"user-scalable=no, width=device-width, initial-scale=1.2, maximum-scale=1.2\"/>";
    }
    buf += "<title>" + homepageTitleStr + "</title>";
    buf += "<link Content-Type=\"text/css\" href=\"/style.css\" rel=\"stylesheet\" />";
    buf += "</head><body>";
    if (a_heading.length())
    {
        buf += "<h1>" + a_heading + "</h1>";
    }
    if (redirectMsg.length())
    {
        buf += "<p>" + redirectMsg + "</p>";
    }

    return buf;
}

String& html_footer(bool enableTraceInfo)
{
    static String buf;
    uint32_t uptime_sec = millis() / 1000;

    buf = "<hr><p><small>";
#if ENABLE_FILE_TRACE
    if (enableTraceInfo)
    {
        if (trace_file_enable_exists())
        {
            buf += "<b>Trace enabled";
            if (trace_to_file_is_working())
            {
                buf += " and trace to file is working.";
            }
            else
            {
                buf += ", but trace to file is not working currently!";
            }
            buf += "</b><br>";
            buf += html_link_to_trace_log() + "<br>";
        }
        else
        {
            if (trace_to_file_is_working())
            {
                buf += "<b>Trace disabled, but trace to file is still working!</b><br>";
                buf += html_link_to_trace_log() + "<br>";
            }
        }
    }
#endif
    buf += "<a href=\"" ADMIN_HTM "\">Admin</a> | ";
    buf += "Uptime: " + sec2str_short(uptime_sec) + " | ";
    buf += "Hostname: <a href=\"http://" + hostname + "\">" + hostname + "</a><br>";
    buf += "<br>";
    buf += "Copyright (C) Peter Ivanov &lt;<a href=\"mailto:ivanovp@gmail.com\">ivanovp@gmail.com</a>&gt;, 2023, 2024.<br>";
    buf += "</small></p>";

    return buf;
}

const String& html_end()
{
    static const String str = String("</body></html>");
    return str;
}

void http_redirect_to_index()
{
    httpServer.sendHeader("Location", INDEX_HTM);
    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.send(301);
}

#if ENABLE_HTTP_AUTH
// Check if header is present and correct
bool http_is_authenticated(String htmPage)
{
    bool ok = false;
    uint8_t i;

    // TRACE("is_authenticated %s\n", htmPage.c_str());
    if (htmPage.length())
    {
        // TRACE("Auth page number: %i\n", httpAuthPageNumber);
        ok = includeHttpAuthPages;
        for (i = 0u; i < httpAuthPageNumber && i < MAX_AUTH_PAGES; i++)
        {
            // TRACE("Checking %s ...\n", httpAuthPages[i].c_str());
            if (htmPage == httpAuthPages[i])
            {
                ok = !ok;
                // TRACE("%s found, ok: %i\n", htmPage.c_str(), ok);
                break;
            }
        }
    }
    if (!ok && httpServer.hasHeader("Cookie"))
    {
        String cookie = httpServer.header("Cookie");
        // TRACE("Found cookie: %s\n", cookie.c_str());
        if (cookie.indexOf("ESPSESSIONID=1") != -1)
        {
            // TRACE("Authentication successful\n");
            ok = true;
        }
    }

    if (!ok)
    {
        TRACE("Authentication failed!\n");
    }

    return ok;
}

// login page, also called for disconnect
void http_server_handle_login_htm()
{
    bool loginFailed = false;
    static String buf;
    if (httpServer.hasArg("DISCONNECT"))
    {
        TRACE("Disconnection\n");
        httpServer.sendHeader("Location", LOGIN_HTM);
        httpServer.sendHeader("Cache-Control", "no-cache");
        httpServer.sendHeader("Set-Cookie", "ESPSESSIONID=0");
        httpServer.send(301);
        return;
    }
    if (httpServer.hasArg("USERNAME") && httpServer.hasArg("PASSWORD"))
    {
        if (httpServer.arg("USERNAME") == HTTP_AUTH_USERNAME && httpServer.arg("PASSWORD") == HTTP_AUTH_PASSWORD)
        {
            httpServer.sendHeader("Location", "/");
            httpServer.sendHeader("Cache-Control", "no-cache");
            httpServer.sendHeader("Set-Cookie", "ESPSESSIONID=1");
            httpServer.send(301);
            TRACE("Log in successful\n");
            return;
        }
        loginFailed = true;
        ERROR("Log in failed!\n");
    }
    buf = html_begin(false, homepageTitleStr, "Login");
    if (loginFailed)
    {
        buf += "<p>Log in failed! Wrong username or password.</p>";
    }
    buf += "<form action='" + String(LOGIN_HTM) + "' method='POST'>";
    buf += "Username: <input type='text' name='USERNAME' placeholder='user name'><br>";
    buf += "Password: <input type='password' name='PASSWORD' placeholder='password'><br>";
    buf += "<input type='submit' name='SUBMIT' value='Submit'></form>";
    buf += html_footer();
    buf += html_end();

    httpServer.send(200, "text/html", buf);
}

void request_http_auth()
{
    httpServer.sendHeader("Location", LOGIN_HTM);
    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.send(301);
}
#endif

/*
 * It generates page /index.htm and /
 */
void http_server_handle_index_htm()
{
    static String buf;

#if ENABLE_HTTP_AUTH
    if (!http_is_authenticated(INDEX_HTM))
    {
        request_http_auth();
        return;
    }
#endif

    buf = html_begin();
    buf += doorbell_generate_index_htm();
    buf += html_footer();
    buf += html_end();

    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.send(200, "text/html; charset=utf-8", buf);
}

/*
 * It generates page /admin.htm
 */
void http_server_handle_admin_htm()
{
    static String buf;

#if ENABLE_HTTP_AUTH
    if (!http_is_authenticated(ADMIN_HTM))
    {
        request_http_auth();
        return;
    }
#endif

    buf = html_begin(false, "Admin", "Admin", 0);
    buf += R"==(
<p>The following pages are available:</p>
<ul>
  <li><a href="/index.htm">/index.htm</a> - Index page</li>
  <li><a href="/admin.htm">/admin.htm</a> - This page</li>
  <li><a href="/files.htm">/files.htm</a> - Manage files on the server</li>
  <li><a href="/upload.htm">/upload.htm</a> - Built-in upload utility</a></li>)==";
#if ENABLE_FIRMWARE_UPDATE
    buf += "<li><a href=\"/update.htm\">/update.htm</a> - Firmware update</li>";
#endif
#if ENABLE_RESET
    buf += "<li><a href=\"/reset.htm\">/reset.htm</a> - Board reset</li>";
#endif
#if ENABLE_FILE_TRACE
    buf += "<li><a href=\"/file_trace.htm\">/file_trace.htm</a> - Enable/disable file trace</li>";
#endif
    buf += R"==(
</ul>

<p>The following REST services are available:</p>
<ul>
  <li><a href="/sysinfo.json">/sysinfo.json</a> - Some system level information</a></li>
  <li><a href="/file_list.json">/file_list.json</a> - Array of all files</a></li>
</ul>)==";
    buf += html_footer();
    buf += html_end();

    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.send(200, "text/html; charset=utf-8", buf);
}

void http_server_handle_upload_htm()
{
#if ENABLE_HTTP_AUTH
    if (!http_is_authenticated(UPLOAD_HTM))
    {
        request_http_auth();
        return;
    }
#endif

    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.send(200, "text/html", FPSTR(uploadContent));
}

#if ENABLE_RESET
/*
 * It resets the board.
 */
void http_server_handle_reset_htm()
{
    String buf;
    String reset_confirmed;
    String yes = "Yes, reset the board!";
    int refresh_sec = BOARD_RESET_TIME_MS / 1000 + 5;

#if ENABLE_HTTP_AUTH
    if (!http_is_authenticated(RESET_HTM))
    {
        request_http_auth();
        return;
    }
#endif

    if (httpServer.hasArg("reset_confirmed"))
    {
        reset_confirmed = httpServer.arg("reset_confirmed");
    }

    if (reset_confirmed.length())
    {
        if (reset_confirmed == yes)
        {
            buf = html_begin(false, homepageTitleStr, "Board reset", refresh_sec, INDEX_HTM);
            buf += "<p>Board will be resetted in " + String(BOARD_RESET_TIME_MS) + " milliseconds...</p>";
            buf += html_footer();
            buf += html_end();

            httpServer.sendHeader("Cache-Control", "no-cache");
            httpServer.send(200, "text/html; charset=utf-8", buf);

            board_reset = true;
            board_reset_timestamp_ms = millis();
        }
        else
        {
            http_redirect_to_index();
        }
    }
    else
    {
        buf = html_begin(false, homepageTitleStr, "Board reset");
        buf += "<p>Are you sure you want to reset the board?</p>";
        buf += "<form><input type=\"submit\" name=\"reset_confirmed\" value=\"" + yes + "\">&nbsp;";
        buf += "<input type=\"submit\" name=\"reset_confirmed\" value=\"No\"></form>";
        buf += html_footer();
        buf += html_end();

        httpServer.sendHeader("Cache-Control", "no-cache");
        httpServer.send(200, "text/html; charset=utf-8", buf);
    }
}
#endif

#if ENABLE_FILE_TRACE
/*
 * It handles file trace enable/disable
 */
void http_server_handle_file_trace_htm(ESP8266WebServer &httpServer, String requestUri)
{
    String buf;
    String enableFileTrace;

#if ENABLE_HTTP_AUTH
    if (!http_is_authenticated(FILE_TRACE_HTM))
    {
        request_http_auth();
        return;
    }
#endif
    if (httpServer.hasArg("filetrace"))
    {
        enableFileTrace = httpServer.arg("filetrace");
    }

    if (enableFileTrace.length())
    {
        buf = html_begin(false, homepageTitleStr, "Enabling/disabling file trace", 5, FILE_TRACE_HTM);
    }
    else
    {
        buf = html_begin(false, homepageTitleStr, "Enabling/disabling file trace");
    }
    buf += "<p>";
    if (enableFileTrace.length())
    {
        buf += "<large><b>";
        if (enableFileTrace == "DISABLE")
        {
            if (trace_disable())
            {
                buf += "File trace has been disabled.";
            }
            else
            {
                buf += "ERROR: cannot disable file trace!";
            }
        }
        else
        {
            if (trace_enable())
            {
                buf += "File trace has been enabled.";
            }
            else
            {
                buf += "ERROR: cannot enable file trace!";
            }
        }
        buf += "</b></large>";
    }
    else
    {
        if (trace_file_enable_exists())
        {
            // buf += "<p>" ENABLE_TRACE_FILE_NAME " file exists in the file system, ";
            // buf += "trace to file is ENABLED.</p>";
            buf += "Trace enabled";
            if (trace_to_file_is_working())
            {
                buf += " and trace to file is working.";
            }
            else
            {
                buf += ", but trace to file is not working currently!";
            }
            buf += "<br>";
            buf += html_link_to_trace_log() + "<br>";
            buf += "<form action=\"" FILE_TRACE_HTM "\">File trace: ";
            buf += "<input type=\"submit\" name=\"filetrace\" value=\"DISABLE\"></form>";
        }
        else
        {
            // buf += "<p>" ENABLE_TRACE_FILE_NAME " file does not exist in the file system, ";
            // buf += "trace to file is DISABLED.</p>";
            buf += "Trace disabled";
            if (trace_to_file_is_working())
            {
                buf += ", <b>but trace to file is still working!</b>";
            }
            buf += "<br>";
            buf += html_link_to_trace_log() + "<br>";
            buf += "<form action=\"" FILE_TRACE_HTM "\">File trace: ";
            buf += "<input type=\"submit\" name=\"filetrace\" value=\"ENABLE\"></form>";
        }
    }
    buf += "</p>";
    buf += "<p>" + html_link_to_index() + "</p>";
    buf += html_footer(false);
    buf += html_end();

    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.send(200, "text/html; charset=utf-8", buf);
}
#endif

// This function is called when the WebServer was requested to list all existing files in the filesystem.
// a JSON array with file information is returned.
void http_server_handle_file_list_json()
{
    Dir dir = LittleFS.openDir("/");
    static String result;

#if ENABLE_HTTP_AUTH
    if (!http_is_authenticated(FILE_LIST_JSON))
    {
        request_http_auth();
        return;
    }
#endif

    result = "[\n";
    while (dir.next())
    {
        if (result.length() > 4)
        {
            result += ",";
        }
        result += "  {";
        result += " \"name\": \"" + dir.fileName() + "\", ";
        result += " \"size\": " + String(dir.fileSize()) + ", ";
        result += " \"time\": " + String(dir.fileTime());
        result += " }\n";
        // jc.addProperty("size", dir.fileSize());
    } // while
    result += "]";
    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.send(200, "text/javascript; charset=utf-8", result);
}

// This function is called when the sysInfo service was requested.
void http_server_handle_sysinfo_json()
{
    String result;

#if ENABLE_HTTP_AUTH
    if (!http_is_authenticated(SYSINFO_JSON))
    {
        request_http_auth();
        return;
    }
#endif

    FSInfo fs_info;
    LittleFS.info(fs_info);

    result = "{\n"
             "  \"firmwareVersionMajor\": " TOSTR(VERSION_MAJOR) "\n"
             "  , \"firmwareVersionMinor\": " TOSTR(VERSION_MINOR) "\n"
             "  , \"firmwareVersionRevision\": " TOSTR(VERSION_REVISION) "\n"
             "  , \"compileDate\": \"" __DATE__ "\"\n"
             "  , \"compileTime\": \"" __TIME__ "\"\n";
#if HW_TYPE == HW_TYPE_ESP01
    result += "  , \"hwType\": \"ESP01\"\n";
#elif HW_TYPE == HW_TYPE_ESP201
    result += "  , \"hwType\": \"ESP201\"\n";
#elif HW_TYPE == HW_TYPE_WEMOS_D1_MINI
    result += "  , \"hwType\": \"WEMOS_D1_MINI\"\n";
#elif HW_TYPE == HW_TYPE_ESP12F
    result += "  , \"hwType\": \"ESP12F\"\n";
#else
    result += "  , \"hwType\": \"unknown\"\n";
#endif
    result += "  , \"hostName\": \"" + String(WiFi.getHostname()) + "\"\n";
    result += "  , \"macAddress\": \"" + WiFi.macAddress() + "\"\n";
    result += "  , \"ipAddress\": \"" + WiFi.localIP().toString() + "\"\n";
    result += "  , \"ipMask\": \"" + WiFi.subnetMask().toString() + "\"\n";
    result += "  , \"dnsIp\": \"" + WiFi.dnsIP().toString() + "\"\n";
    result += "  , \"flashSize\": " + String(ESP.getFlashChipSize()) + "\n";
    result += "  , \"freeHeap\": " + String(ESP.getFreeHeap()) + "\n";
    result += "  , \"fsTotalBytes\": " + String(fs_info.totalBytes) + "\n";
    result += "  , \"fsUsedBytes\": " + String(fs_info.usedBytes) + "\n";
    result += "  , \"uptime_ms\": " + String(millis()) + "\n";
    result += "  , \"doorbell\": 1\n"
              "  , \"doorbellAudioFileName\": \"" DOORBELL_AUDIO_FILE_NAME "\"\n"
              "  , \"doorbellAudioPlayCount\": " TOSTR(DOORBELL_AUDIO_PLAY_COUNT) "\n"
              "  , \"doorbellAudioPlayDelay_ms\": " TOSTR(DOORBELL_AUDIO_PLAY_DELAY_MS) "\n"
              "  , \"doorbellSwitchPin\": " TOSTR(DOORBELL_SWITCH_PIN) "\n"
#if ENABLE_MQTT_CLIENT
              "  , \"mqttClient\": 1\n"
#endif
#if ENABLE_FIRMWARE_UPDATE
              "  , \"firmwareUpdate\": 1\n"
#endif
#if ENABLE_RESET
              "  , \"reset\": 1\n"
#endif
#if ENABLE_HTTP_AUTH
              "  , \"httpAuth\": 1\n"
#endif
#if DISABLE_SERIAL_TRACE
              "  , \"disableSerialTrace\": 1\n"
#endif
#if ENABLE_FILE_TRACE
              "  , \"fileTrace\": 1\n"
              "  , \"traceFileName\": \"" TRACE_FILE_NAME "\"\n"
#ifdef TRACE_LINE_COUNT_TO_FLUSH
              "  , \"traceLineCountToFlush\": " TOSTR(TRACE_LINE_COUNT_TO_FLUSH) "\n"
#endif
#ifdef TRACE_ELAPSED_TIME_TO_FLUSH_MS
              "  , \"traceElapsedTimeToFlush_ms\": " TOSTR(TRACE_ELAPSED_TIME_TO_FLUSH_MS) "\n"
#endif
#ifdef ENABLE_TRACE_MS_TIMESAMP
              "  , \"enableTraceMsTimestamp\": " TOSTR(ENABLE_TRACE_MS_TIMESAMP) "\n"
#endif
              ;
    result += "  , \"traceToFileIsWorking\": " + String(trace_to_file_is_working()) + "\n";
#endif /* ENABLE_FILE_TRACE*/
    result += "}";
    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.send(200, "text/javascript; charset=utf-8", result);
} // http_server_handle_sysinfo_json()


// ===== Request Handler class used to answer more complex requests =====

// The FileServerHandler is registered to the web server to support DELETE and UPLOAD of files into the filesystem.
class FileServerHandler : public RequestHandler
{
public:
    // @brief Construct a new File Server Handler object
    // @param fs The file system to be used.
    // @param path Path to the root folder in the file system that is used for serving static data down and upload.
    // @param cache_header Cache Header to be used in replies.
    FileServerHandler()
    {
        TRACE("FileServerHandler is registered\n");
    }

    // @brief check incoming request. Can handle POST for uploads and DELETE.
    // @param requestMethod method of the http request line.
    // @param requestUri request ressource from the http request line.
    // @return true when method can be handled.
    bool canHandle(HTTPMethod requestMethod, const String &requestUri) override
    {
        bool can = false;

        if (requestMethod == HTTP_POST)
        {
            can = true;
        }
        else if (requestMethod == HTTP_DELETE)
        {
            can = true;
        }
        else if ((requestMethod == HTTP_GET) && (requestUri == DOORBELL_HTM))
        {
            can = true;
        }
#if ENABLE_FILE_TRACE
        else if ((requestMethod == HTTP_GET) && (requestUri == FILE_TRACE_HTM))
        {
            can = true;
        }
#endif

        return can;
    } // canHandle()

    bool canUpload(const String &uri) override
    {
        // only allow upload on root fs level.
        return (uri == "/");
    } // canUpload()

    bool handle(ESP8266WebServer &httpServer, HTTPMethod requestMethod, const String &requestUri) override
    {
        // ensure that filename starts with '/'
        String fileName = requestUri;
        // TRACE("handle, requestMethod: %i, requestUri: %s\n", requestMethod, requestUri.c_str());
        if (!fileName.startsWith("/"))
        {
            fileName = "/" + fileName;
        }

        if (requestMethod == HTTP_POST)
        {
            // all done in upload. no other forms.
        }
        else if (requestMethod == HTTP_GET)
        {
            if (requestUri == DOORBELL_HTM)
            {
                doorbell_handle_doorbell_htm(httpServer, requestUri);
            }
            else
#if ENABLE_FILE_TRACE
            if (requestUri == FILE_TRACE_HTM)
            {
                http_server_handle_file_trace_htm(httpServer, requestUri);
            }
            else
#endif
            {
#if ENABLE_HTTP_AUTH
                if (!http_is_authenticated(requestUri))
                {
                    request_http_auth();
                    return true;
                }
#endif
                return RequestHandler::handle(httpServer, requestMethod, requestUri);
            }
        }
        else if (requestMethod == HTTP_DELETE)
        {
            TRACE("Deleting %s... ", fileName.c_str());
            if (LittleFS.exists(fileName))
            {
                if (LittleFS.remove(fileName))
                {
                    TRACE("Done.\n");
                }
                else
                {
                    ERROR("Cannot delete %s!\n", fileName.c_str());
                }
            }
            else
            {
                ERROR("Cannot delete %s as it does not exists!\n", fileName.c_str());
            }
        } // if

        // TRACE("URI: %s\n", requestUri.c_str());
        httpServer.send(200); // all done.
        return (true);
    } // handle()

    // uploading process
    void upload(ESP8266WebServer UNUSED &server, const String UNUSED &_requestUri, HTTPUpload &upload) override
    {
        // ensure that filename starts with '/'
        String fileName = upload.filename;
        if (!fileName.startsWith("/"))
        {
            fileName = "/" + fileName;
        }

        if (upload.status == UPLOAD_FILE_START)
        {
            // Open the file
            if (LittleFS.exists(fileName))
            {
                LittleFS.remove(fileName);
            }
            m_fsUploadFile = LittleFS.open(fileName, "w");
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
            // Write received bytes
            if (m_fsUploadFile)
            {
                m_fsUploadFile.write(upload.buf, upload.currentSize);
            }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
            // Close the file
            if (m_fsUploadFile)
            {
                m_fsUploadFile.close();
            }
        }
    } // upload()

protected:
    File m_fsUploadFile;
};

#if ENABLE_HTTP_AUTH
static void http_auth_init()
{
    if (LittleFS.exists("/include_http_auth_pages.txt"))
    {
        includeHttpAuthPages = true;
        httpAuthPageNumber = readStringsFromFile("/include_http_auth_pages.txt", 0, httpAuthPages, MAX_AUTH_PAGES);
    }
    else if (LittleFS.exists("/exclude_http_auth_pages.txt"))
    {
        includeHttpAuthPages = false;
        httpAuthPageNumber = readStringsFromFile("/include_http_auth_pages.txt", 0, httpAuthPages, MAX_AUTH_PAGES);
    }
    else
    {
        /* Apply default settings */
        /* Exclude only index.htm from authentication */
        includeHttpAuthPages = false;
        httpAuthPages[0] = INDEX_HTM;
        httpAuthPageNumber = 1;
    }
}
#endif

void http_server_init(void)
{
    String str;

#if ENABLE_FIRMWARE_UPDATE
    MDNS.begin(hostname);
    httpUpdater.setup(&httpServer, UPDATE_HTM
#if ENABLE_HTTP_AUTH
                      , HTTP_AUTH_USERNAME, HTTP_AUTH_PASSWORD
#endif
    );
#endif

    str = readStringFromFile("/homepage_refresh_interval.txt");
    if (!str.isEmpty())
    {
        homepageRefreshInterval_sec = str.toInt();
        TRACE("Setting homepage refresh interval to %i seconds...\n", homepageRefreshInterval_sec);
    }

    str = readStringFromFile("/homepage_texts.txt");
    if (!str.isEmpty())
    {
        homepageTitleStr = str;
        TRACE("Setting homepage title to '%s'...\n", homepageTitleStr.c_str());
    }

    // serve a built-in htm page
    httpServer.on(UPLOAD_HTM, http_server_handle_upload_htm);

    httpServer.on("/", HTTP_GET, http_server_handle_index_htm);
    httpServer.on(INDEX_HTM, HTTP_GET, http_server_handle_index_htm);
    httpServer.on(ADMIN_HTM, HTTP_GET, http_server_handle_admin_htm);
#if ENABLE_RESET
    httpServer.on(RESET_HTM, HTTP_GET, http_server_handle_reset_htm);
#endif
#if ENABLE_HTTP_AUTH
    httpServer.on(LOGIN_HTM, http_server_handle_login_htm);
#endif

    // register some REST services
    httpServer.on(FILE_LIST_JSON, HTTP_GET, http_server_handle_file_list_json);
    httpServer.on(SYSINFO_JSON, HTTP_GET, http_server_handle_sysinfo_json);

    // UPLOAD and DELETE of files in the file system using a request handler.
    httpServer.addHandler(new FileServerHandler());

    // enable CORS header in webserver results
    httpServer.enableCORS(true);

    // enable ETAG header in webserver results from serveStatic handler
    httpServer.enableETag(true);

#if ENABLE_HTTP_AUTH
    // ask server to track these headers
    httpServer.collectHeaders("User-Agent", "Cookie");
    http_auth_init();
#endif
    // serve all static files
    httpServer.serveStatic("/", LittleFS, "/");

    // handle cases when file is not found
    httpServer.onNotFound([]()
                          {
    // standard not found in browser.
    httpServer.send(404, "text/html", FPSTR(notFoundContent)); });

    httpServer.begin();
    TRACE("HTTP server started at http://%s:%i/\n", hostname.c_str(), HTTP_SERVER_PORT);

#if ENABLE_FIRMWARE_UPDATE
    MDNS.addService("http", "tcp", HTTP_SERVER_PORT);
    TRACE("HTTPUpdateServer ready at http://%s:%i%s\n", hostname.c_str(), HTTP_SERVER_PORT, UPDATE_HTM);
#endif
}

void http_server_task(void)
{
#if ENABLE_HTTP_SERVER
    httpServer.handleClient();
#if ENABLE_FIRMWARE_UPDATE
    MDNS.update();
#endif
#endif
}
#endif /* ENABLE_HTTP_SERVER */
