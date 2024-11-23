/**
 * @file        trace.cpp
 * @brief       Definitions and prototypes for trace and error logging
 * @author      Copyright (C) Peter Ivanov, 2023, 2024
 *
 * Created      2023-12-26 11:48:53
 * Last modify: 2024-11-23 12:50:26 ivanovp {Time-stamp}
 * Licence:     GPL
 */

#ifndef INCLUDE_TRACE_H
#define INCLUDE_TRACE_H

#include <stdint.h>

#include <Arduino.h>

#include "common.h"
#include "config.h"

#define TRACE_FLAG_NORMAL   0x00u
#define TRACE_FLAG_ERROR    0x01u
#define TRACE_FLAG_FILE     0x02u

#define TRACE(...)          trace_printf(TRACE_FLAG_NORMAL, __VA_ARGS__)
#if DISABLE_SENSOR_TRACE
#define TRACES(...)
#else
#define TRACES(...)         trace_printf(TRACE_FLAG_NORMAL, __VA_ARGS__)
#endif
#define ERROR(...)          trace_printf(TRACE_FLAG_ERROR, __VA_ARGS__)
#define FILE_TRACE(...)     trace_printf(TRACE_FLAG_FILE, __VA_ARGS__)

extern void trace_init();
extern void trace_printf(uint8_t trace_flags, const char *fmt, ...);
extern bool trace_enable();
extern bool trace_disable();
extern bool trace_file_enable_exists();
extern bool trace_to_file_is_working();
extern void trace_task();

#endif /* INCLUDE_TRACE_H */

