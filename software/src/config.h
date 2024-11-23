/**
 * @file        config_doorbell.h
 * @brief       Configuration definitions for doorbell
 * @author      Copyright (C) Peter Ivanov, 2011, 2012, 2013, 2014, 2021, 2023, 2024
 *
 * Created      2011-01-19 11:48:53
 * Last modify: 2024-11-23 12:49:11 ivanovp {Time-stamp}
 * Licence:     GPL
 */

#ifndef INCLUDE_CONFIG_H_
#define INCLUDE_CONFIG_H_

/* do not change the following four defines! */
#define HW_TYPE_ESP01           0   /* ESP8266-01 a.k.a. ESP-01 */
#define HW_TYPE_ESP201          1   /* ESP201 */
#define HW_TYPE_WEMOS_D1_MINI   2   /* Wemos D1 Mini */
#define HW_TYPE_ESP12F          3   /* ESP12F */

#define HW_TYPE                 HW_TYPE_WEMOS_D1_MINI

#define ENABLE_HTTP_SERVER      1   /* 1: enable HTTP server, 0: disable HTTP server */
#define ENABLE_NTP_CLIENT       1   /* 1: enable NTP client, 0: disable NTP client */
#define ENABLE_MQTT_CLIENT      1   /* 1: enable MQTT client, 0: disable MQTT client */
#define ENABLE_FIRMWARE_UPDATE  1   /* 1: enable firmware update through HTTP, 0: disable firmware update */
#define ENABLE_RESET            1   /* 1: enable reset through HTTP, 0: disable reset */
#define ENABLE_HTTP_AUTH        1   /* 1: enable user authentication through HTTP, 0: disable authentication */

#define ENABLE_DOORBELL         1

#define DISABLE_SERIAL_TRACE    0

// name of the server. You reach it using http://doorbell<MAC>
#define DEFAULT_HOSTNAME "doorbell"

// TODO define time zone in file!
// local time zone definition (Berlin, Belgrade, Budapest, Oslo, Paris, etc.)
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

#define MQTT_CONNECT_RETRY_SEC                  5
#define MQTT_PUBLISH_INTERVAL_SEC               10
#define HTTP_SERVER_PORT                        80
#define DEFAULT_HOMEPAGE_REFRESH_INTERVAL_SEC   60

#if HW_TYPE == HW_TYPE_WEMOS_D1_MINI
#define LED_PIN                         2   /* GPIO2 */
#define LED_INVERTED                    1
#endif

#if ENABLE_DOORBELL
#define DOORBELL_AUDIO_FILE_NAME        "doorbell.wav"
#define DOORBELL_AUDIO_PLAY_COUNT       1               /* Play audio file multiple times */
#define DOORBELL_AUDIO_PLAY_DELAY_MS    1000            /* Play audio file multiple times with delay */
#define DOORBELL_AUDIO_GAIN             1.0f
/* Input 14 is broken */
#define DOORBELL_SWITCH_PIN             12              /* GPIO pin or -1 to disable switch input */
#define ENABLE_DOORBELL_I2S_DAC         1
#define DOORBELL_HISTORY_LENGTH         32             /* Last 32 button press will be stored and displayed on index page */
#endif

#if ENABLE_MQTT_CLIENT
#define MQTT_SWITCHES_TOPIC_PREFIX      "/switches/"
#endif

#define ENABLE_TIMESTAMP_ON_SERIAL_TRACE    0

#define ENABLE_FILE_TRACE               1
#if ENABLE_FILE_TRACE
/* If this file exists on file system, trace output will be saved to TRACE_FILE_NAME */
#define ENABLE_TRACE_FILE_NAME          "enable_trace.txt"
/* Current trace output is saved to this file too */
#define TRACE_FILE_NAME                 "trace.txt"
/* Previous trace output is renamed to this file */
#define TRACE_PREV_FILE_NAME            "trace_prev.txt"
#define TRACE_LINE_COUNT_TO_FLUSH       100                 /* flush file after every 100th line */
#define TRACE_ELAPSED_TIME_TO_FLUSH_MS  5000                /* flush file after 5 seconds */
#define ENABLE_TRACE_MS_TIMESAMP        1
#endif /* ENABLE_FILE_TRACE */
/* Current error output is saved to this file too */
#define ERROR_FILE_NAME                 "error.txt"
/* Previous error output is renamed to this file */
#define ERROR_PREV_FILE_NAME            "error_prev.txt"

#define COMMENT_CHAR                    ';'

#define BOARD_RESET_TIME_MS             1000

#if ENABLE_HTTP_AUTH
#define MAX_AUTH_PAGES                  16
#define HTTP_AUTH_USERNAME              "admin"
#define HTTP_AUTH_PASSWORD              "pass"
#endif

#endif /* INCLUDE_CONFIG_H_ */
