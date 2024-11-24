/**
 * @file        main.cpp
 * @brief       Main program
 * @author      Copyright (C) Peter Ivanov, 2011, 2012, 2013, 2014, 2021, 2023, 2024
 *
 * Created      2011-01-19 11:48:53
 * Last modify: 2024-11-23 12:49:57 ivanovp {Time-stamp}
 * Licence:     GPL
 */

#include <Arduino.h>
#include <FS.h>       // File System for Web Server Files
#include <LittleFS.h> // This file system is used.
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
// Example firmware upload via curl:
// curl -F "image=@.pio/build/esp07/firmware.bin" http://sensor10c488.home/update.htm
#include <ESP8266HTTPUpdateServer.h>
#include "PubSubClient.h" /* MQTT */

#include "main.h"
#include "common.h"
#include "config.h"
#include "secrets.h" // add WLAN Credentials in here.
#include "http_server.h"
#include "fileutils.h"
#include "trace.h"
#include "doorbell.h"

const char *ssid = STASSID;
const char *passPhrase = STAPSK;

String hostname;
WiFiClient wifiClient;

#if ENABLE_MQTT_CLIENT
PubSubClient mqttClient(wifiClient);

uint32_t mqtt_connect_start_time = 0;
String mqttTopic;

String mqttSwitchesTopicPrefix;
#endif /* ENABLE_MQTT_CLIENT */

#if ENABLE_RESET
bool board_reset = false;
uint32_t board_reset_timestamp_ms = 0;
#endif

// The text of builtin files are in this header file
#include "builtinfiles.h"

void setLed(bool on)
{
#if LED_PIN != 0
    if (
#if LED_INVERTED
        !on // Inverse logic on Wemos D1 Mini
#else
        on
#endif
    )
    {
        digitalWrite(LED_PIN, HIGH);
    }
    else
    {
        digitalWrite(LED_PIN, LOW);
    }
#endif
}

void setLed2(bool on)
{
#if LED2_PIN != 0
    if (
#if LED2_INVERTED
        !on
#else
        on
#endif
    )
    {
        digitalWrite(LED2_PIN, HIGH);
    }
    else
    {
        digitalWrite(LED2_PIN, LOW);
    }
#endif
}

String sec2str(uint32_t a_seconds)
{
    String buf;

    uint8_t sec = a_seconds % 60;
    uint8_t min = (a_seconds / 60) % 60;
    uint8_t hour = (a_seconds / (60 * 60)) % 24;
    uint32_t day = (a_seconds / (60 * 60 * 24));

    if (day == 1)
    {
        buf += "1 day ";
    }
    else if (day > 1)
    {
        buf += String(day) + " days ";
    }
    if (hour == 1)
    {
        buf += "1 hour ";
    }
    else if (hour > 1)
    {
        buf += String(hour) + " hours ";
    }
    if (min == 1)
    {
        buf += "1 minute ";
    }
    else if (min > 1)
    {
        buf += String(min) + " minutes ";
    }
    if (sec == 1)
    {
        buf += "1 second";
    }
    else if (sec > 1)
    {
        buf += String(sec) + " seconds";
    }
    else if (!buf.length()) /* check if string is empty (in this case sec == 0)*/
    {
        buf = "0 second";
    }
    /* Check if last character is a space character */
    if (buf[buf.length() - 1] == ' ')
    {
        /* Remove trailing space */
        buf.remove(buf.length() - 1);
    }

    return buf;
}

String sec2str_short(uint32_t a_seconds)
{
    String buf;

    uint8_t sec = a_seconds % 60;
    uint8_t min = (a_seconds / 60) % 60;
    uint8_t hour = (a_seconds / (60 * 60)) % 24;
    uint32_t day = (a_seconds / (60 * 60 * 24));

    if (day > 0)
    {
        buf += String(day) + "d ";
    }
    if (hour > 0)
    {
        buf += String(hour) + "h ";
    }
    if (min > 0)
    {
        buf += String(min) + "m ";
    }
    if (sec > 0)
    {
        buf += String(sec) + "s";
    }
    else if (!buf.length()) /* check if string is empty (in this case sec == 0)*/
    {
        buf = "0s";
    }
    /* Check if last character is a space character */
    if (buf[buf.length() - 1] == ' ')
    {
        /* Remove trailing space */
        buf.remove(buf.length() - 1);
    }

    return buf;
}

#if ENABLE_MQTT_CLIENT
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    String topicStr = topic;
    String payloadStr;
    uint16_t i;

    for (i = 0; i < length; i++)
    {
        payloadStr += static_cast<char>(payload[i]);
    }
    TRACE("MQTT callback, topic: '%s', payload: '%s'\n", topic, payloadStr.c_str());

    doorbell_mqtt_callback(topicStr, payloadStr, length);
}
#endif

// Setup everything to make the webserver work.
void setup(void)
{
#if LED_PIN != 0
    pinMode(LED_PIN, OUTPUT);
#endif
#if LED2_PIN != 0
    pinMode(LED2_PIN, OUTPUT);
#endif
    setLed(true);
    setLed2(true);

    delay(250); // wait for serial monitor to start completely.

    // Use Serial port for some trace information from the example
    Serial.begin(115200);
    Serial.setDebugOutput(false);

#if DISABLE_SERIAL_TRACE
    /* Be silent! */
#else
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.println();
    Serial.println("Doorbell firmware started");
    Serial.printf("Compiled on " __DATE__ " " __TIME__ "\n");
#endif
    setLed(false);
    setLed2(false);

    TRACE("\n");
    TRACE("Mounting the filesystem... ");
    if (LittleFS.begin())
    {
        TRACE("Done.\n");
    }
    else
    {
        ERROR("Could not mount the filesystem!\n");
        delay(2000);
        ESP.restart();
    }
    /* Initialize trace after file system as log file might be created */
    trace_init();

    // start WiFI
    WiFi.mode(WIFI_STA);
    if (strlen(ssid) == 0)
    {
        WiFi.begin();
    }
    else
    {
        WiFi.begin(ssid, passPhrase);
    }

    hostname = readStringFromFile("/hostname.txt");
    if (hostname.isEmpty())
    {
        // allow to address the device by the given name e.g. http://doorbell<MAC>
        hostname = DEFAULT_HOSTNAME;
        String mac = WiFi.macAddress();
        TRACE("MAC: %s\n", mac.c_str());
        hostname += mac[9];
        hostname += mac[10];
        hostname += mac[12];
        hostname += mac[13];
        hostname += mac[15];
        hostname += mac[16];
    }
#if ENABLE_MQTT_CLIENT
    mqttTopic = readStringFromFile("/mqtt_topic.txt");
    if (mqttTopic.isEmpty())
    {
        mqttTopic = hostname;
        TRACE("Using hostname '%s' as MQTT topic\n", hostname.c_str());
    }
    mqttSwitchesTopicPrefix = readStringFromFile("/mqtt_topic.txt", 2);
    if (mqttSwitchesTopicPrefix.isEmpty())
    {
        mqttSwitchesTopicPrefix = MQTT_SWITCHES_TOPIC_PREFIX + mqttTopic + "/";
    }
#endif

    TRACE("Setting WiFi hostname: %s\n", hostname.c_str());
    WiFi.setHostname(hostname.c_str());
    TRACE("WiFi hostname: %s\n", WiFi.getHostname());

    String str;

    doorbell_init();

    TRACE("Connecting to WiFi...\n");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        TRACE(".");
    }
    TRACE("connected.\n");
    randomSeed(micros());
    TRACE("IP address: %s\n", WiFi.localIP().toString().c_str());

#if ENABLE_NTP_CLIENT
    // Ask for the current time using NTP request builtin into ESP firmware.
    TRACE("Setup NTP...\n");
    configTime(TIMEZONE, "pool.ntp.org");
#endif

    TRACE("Register service handlers...\n");

#if ENABLE_HTTP_SERVER
    http_server_init();
#endif

#if ENABLE_MQTT_CLIENT
    mqttClient.setKeepAlive(15);        /* default is 15 seconds */
    mqttClient.setSocketTimeout(15);    /* default is 15 seconds */
    mqttClient.setServer(MQTT_SERVER, MQTT_SERVERPORT);
    mqttClient.setCallback(mqtt_callback);
#endif /* ENABLE_MQTT_CLIENT */
} // setup

#if ENABLE_MQTT_CLIENT
// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care of connecting.
uint8_t mqtt_task()
{
    uint8_t mqtt_flags = 0;

    if (WiFi.status() == WL_CONNECTED)
    {
        if (!mqttClient.connected())
        {
            if ((mqtt_connect_start_time == 0) || (millis() >= mqtt_connect_start_time))
            {
                TRACE("Connecting to MQTT... ");

                if (mqttClient.connect(hostname.c_str()))
                {
                    TRACE("Connected to MQTT broker\n");
                    mqtt_flags |= MQTT_FLAG_CONNECTED;
                }
                else
                {
                    ERROR("Cannot connect to MQTT broker! Error: %i\n", mqttClient.state());
                    TRACE("Retrying MQTT connection in %i seconds...", MQTT_CONNECT_RETRY_SEC);
                    mqttClient.disconnect();
                    mqtt_connect_start_time = millis() + SEC_TO_MS(MQTT_CONNECT_RETRY_SEC);
                    mqtt_flags |= MQTT_FLAG_DISCONNECTED;
                }
            }
        }
    }
    else
    {
        if (mqttClient.connected())
        {
            TRACE("WiFi disconnected, disconnecting from MQTT broker... ");
            mqttClient.disconnect();
            mqtt_flags |= MQTT_FLAG_DISCONNECTED;
        }
    }
    mqttClient.loop();

    return mqtt_flags;
}
#endif /* ENABLE_MQTT_CLIENT */

// run the server...
void loop(void)
{
    uint8_t mqtt_flags = 0;
#if ENABLE_RESET
    uint32_t now;
#endif

#if ENABLE_HTTP_SERVER
    http_server_task();
#endif
#if ENABLE_MQTT_CLIENT
    mqtt_flags = mqtt_task();
#endif
    doorbell_task(mqtt_flags);
#if ENABLE_RESET
    now = millis();
    if (board_reset && now >= BOARD_RESET_TIME_MS && now - BOARD_RESET_TIME_MS >= board_reset_timestamp_ms)
    {
        TRACE("Restarting...");
        ESP.restart();
    }
#endif
#if ENABLE_FILE_TRACE
    trace_task();
#endif
}
