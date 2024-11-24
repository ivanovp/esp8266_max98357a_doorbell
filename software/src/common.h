/**
 * @file        common.h
 * @brief       Common definitions
 * @author      Copyright (C) Peter Ivanov, 2011, 2012, 2013, 2014, 2021, 2023, 2024
 *
 * Created      2011-01-19 11:48:53
 * Last modify: 2024-11-23 12:49:06 ivanovp {Time-stamp}
 * Licence:     GPL
 */

#ifndef INCLUDE_COMMON_H
#define INCLUDE_COMMON_H

#include <stdint.h>
#include "config.h"

#if !DISABLE_SERIAL_TRACE && ENABLE_FILE_TRACE
#include <FS.h>        // File System for Web Server Files
#include <LittleFS.h>  // This file system is used.
#endif

#define VERSION_MAJOR    1
#define VERSION_MINOR    1
#define VERSION_REVISION 1

#define MAX(x, y) (x > y ? x : y)
#define MIN(x, y) (x < y ? x : y)

#ifndef FALSE
#define FALSE   (0)
#endif
#ifndef TRUE
#define TRUE    (1)
#endif

#define CHR_EOS                     '\0'    /* End of string */
#define CHR_SPACE                   ' '
#define CHR_CR                      '\r'    /* Carriage return */
#define CHR_LF                      '\n'    /* Line feed */
#define CHR_TAB                     '\t'    /* Tabulator */
#define IS_WHITESPACE(c)    ((c) == CHR_SPACE || (c) == CHR_CR || (c) == CHR_LF || (c) == CHR_TAB)

// mark parameters not used
#define UNUSED                      __attribute__((unused))

/* One minute in milliseconds unit */
#define SEC_TO_MS(seconds)          ((seconds) * 1000u)
#define ONE_MIN_IN_MS               SEC_TO_MS(60)

#define XSTR(x)                     #x
#define TOSTR(x)                    XSTR(x)

typedef char bool_t;


#ifndef ENABLE_HTTP_SERVER
#define ENABLE_HTTP_SERVER      1
#endif

#ifndef ENABLE_NTP_CLIENT
#define ENABLE_NTP_CLIENT       1
#endif

#ifndef ENABLE_MQTT_CLIENT
#define ENABLE_MQTT_CLIENT      1
#endif

#if ENABLE_MQTT_CLIENT
#ifndef MQTT_SWITCHES_TOPIC_PREFIX
#define MQTT_SWITCHES_TOPIC_PREFIX  "/switches/"
#endif
#endif /* ENABLE_MQTT_CLIENT */

#endif /* INCLUDE_COMMON_H */

