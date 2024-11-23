/**
 * @file        http_server.h
 * @brief       Definitions of http_server.cpp
 * @author      Copyright (C) Peter Ivanov, 2011, 2012, 2013, 2014, 2021, 2023, 2024
 *
 * Created      2011-01-19 11:48:53
 * Last modify: 2024-11-23 12:49:32 ivanovp {Time-stamp}
 * Licence:     GPL
 */

#ifndef INCLUDE_HTTP_SERVER_H
#define INCLUDE_HTTP_SERVER_H

#include <stdint.h>

#include <Arduino.h>
#include <ESP8266WebServer.h>

#include "common.h"
#include "config.h"
#include "secrets.h"  // add WLAN Credentials in here.
#include "trace.h"

#include <FS.h>        // File System for Web Server Files
#include <LittleFS.h>  // This file system is used.

#define INDEX_HTM       "/index.htm"
#define ADMIN_HTM       "/admin.htm"
#define UPLOAD_HTM      "/upload.htm"
#define FILE_LIST_JSON  "/file_list.json"
#define SYSINFO_JSON    "/sysinfo.json"
#define SENSOR_JSON     "/sensor.json"

#if ENABLE_DOORBELL
#define DOORBELL_HTM "/doorbell.htm"
#endif
#if ENABLE_FILE_TRACE
#define FILE_TRACE_HTM "/file_trace.htm"
#endif
#if ENABLE_HTTP_AUTH
#define LOGIN_HTM   "/login.htm"
#endif
#if ENABLE_FIRMWARE_UPDATE
#define UPDATE_HTM  "/update.htm"
#endif
#if ENABLE_RESET
#define RESET_HTM   "/reset.htm"
#endif

#define TITLE_STR "Doorbell"

#if ENABLE_HTTP_SERVER
extern ESP8266WebServer httpServer;
extern String homepageTitleStr;   /* First line of homepage_texts.txt */

extern const String& html_link_to_index();
#if ENABLE_FILE_TRACE
extern const String& html_link_to_trace_log();
#endif
extern String& html_begin(bool a_scalable=false, String a_title=homepageTitleStr, String a_heading=homepageTitleStr,
    int a_homepage_refresh_interval_sec = -1, String a_homepage_redirect="");
extern String& html_footer(bool enableTraceInfo=true);
extern const String& html_end();
extern void http_redirect_to_index();
#if ENABLE_HTTP_AUTH
extern bool http_is_authenticated(String htmPage="");
extern void http_server_handle_login_htm();
extern void request_http_auth();
#endif
extern void http_server_init(void);
extern void http_server_task(void);
#endif

#endif /* INCLUDE_HTTP_SERVER_H */

