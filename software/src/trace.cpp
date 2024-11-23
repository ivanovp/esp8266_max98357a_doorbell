/**
 * @file        trace.cpp
 * @brief       Trace and error logging
 * @author      Copyright (C) Peter Ivanov, 2023, 2024
 *
 * Created      2023-12-26 11:48:53
 * Last modify: 2024-11-23 12:50:22 ivanovp {Time-stamp}
 * Licence:     GPL
 */

#include <Arduino.h>

#include "main.h"
#include "common.h"
#include "config.h"
#include "fileutils.h"

#include <FS.h>       // File System for Web Server Files
#include <LittleFS.h> // This file system is used.

#define TRACE_PRINTF_BUF_SIZE           256

#ifndef ERROR_FILE_NAME
/* Current error output is saved to this file too */
#define ERROR_FILE_NAME                 "error.txt"
#endif

#ifndef ERROR_PREV_FILE_NAME
/* Previous error output is renamed to this file */
#define ERROR_PREV_FILE_NAME            "error_prev.txt"
#endif

#ifndef TRACE_LINE_COUNT_TO_FLUSH
#define TRACE_LINE_COUNT_TO_FLUSH       100                 /* flush file after every 100th line */
#endif

#ifndef TRACE_ELAPSED_TIME_TO_FLUSH_MS
#define TRACE_ELAPSED_TIME_TO_FLUSH_MS  5000                /* flush file after 5 seconds */
#endif

#ifndef ENABLE_TRACE_MS_TIMESAMP
#define ENABLE_TRACE_MS_TIMESAMP        1
#endif

#if ENABLE_FILE_TRACE
bool traceToFileIsWorking = false;
File traceFile;
#if TRACE_LINE_COUNT_TO_FLUSH
static uint16_t traceFileLineCntr = 0;
#endif
#if TRACE_ELAPSED_TIME_TO_FLUSH_MS
bool traceFileFlushPending = false;
uint32_t traceFileLastFlushTimesamp_ms = 0;
#endif
#endif
bool errorFileIsOpened = false;
File errorFile;

#if ENABLE_FILE_TRACE
static bool trace_file_start()
{
    bool traceFileOk = false;

    if (LittleFS.exists(TRACE_FILE_NAME))
    {
        TRACE("Renaming previous trace file %s -> %s ...", TRACE_FILE_NAME,
              TRACE_PREV_FILE_NAME);
        if (LittleFS.rename(TRACE_FILE_NAME, TRACE_PREV_FILE_NAME))
        {
            TRACE("Done.\n");
        }
        else
        {
            ERROR("Error!\n");
        }
    }
    traceFile = LittleFS.open(TRACE_FILE_NAME, "w");
    if (traceFile)
    {
        traceFileOk = true;
        TRACE("Trace file %s opened.\n", TRACE_FILE_NAME);
    }
    else
    {
        ERROR("Cannot create trace file %s!\n", TRACE_FILE_NAME);
    }

    return traceFileOk;
}

static bool trace_file_stop()
{
    bool ok = false;

    if (traceFile)
    {
        traceFile.close();
        TRACE("Trace file %s closed.\n", TRACE_FILE_NAME);
        ok = true;
    }
    else
    {
        ERROR("Cannot close un-opened trace file %s!\n", TRACE_FILE_NAME);
        ok = false;
    }

    return ok;
}
#endif /* ENABLE_FILE_TRACE */

void trace_init()
{
    errorFileIsOpened = false;

#if ENABLE_FILE_TRACE
    if (LittleFS.exists(ENABLE_TRACE_FILE_NAME))
    {
        traceToFileIsWorking = trace_file_start();
    }
    else
    {
        TRACE("Trace file disabled.\n");
        traceToFileIsWorking = false;
    }
#endif
    if (fileSize(ERROR_FILE_NAME) > 100 * 1024)
    {
        TRACE("Renaming previous error file %s -> %s ...", ERROR_FILE_NAME,
              ERROR_PREV_FILE_NAME);
        if (LittleFS.rename(ERROR_FILE_NAME, ERROR_PREV_FILE_NAME))
        {
            TRACE("Done.\n");
        }
        else
        {
            ERROR("Error!\n");
        }
    }

    if (LittleFS.exists(ERROR_FILE_NAME))
    {
        errorFile = LittleFS.open(ERROR_FILE_NAME, "a");
    }
    else
    {
        errorFile = LittleFS.open(ERROR_FILE_NAME, "w");
    }
    if (errorFile)
    {
        errorFileIsOpened = true;
        TRACE("Error file %s opened.\n", ERROR_FILE_NAME);
    }
    else
    {
        ERROR("cannot create error file %s!\n", ERROR_FILE_NAME);
    }
}

static String trace_get_timestamp()
{
    String timestamp;
    char buffer[32];
    time_t rawtime;
    uint32_t ms = millis() % 1000;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    timestamp = buffer;
#if ENABLE_TRACE_MS_TIMESAMP
    snprintf(buffer, sizeof(buffer), ".%03d", ms);
    timestamp += buffer;
#endif
    return timestamp;
}

/**
 * Printf function which uses UART/file interface to send.
 * Example:
<pre>
trace_printf ("Number: %02i\r\n", number);
</pre>
 *
 * @param fmt Printf format string. Example: "Value: %02i\r\n"
 */
void trace_printf(uint8_t trace_flags, const char *fmt, ...)
{
    static va_list valist;
    static bool printTimeStamp = true;
    static char buf[TRACE_PRINTF_BUF_SIZE];
    static String timeStampStr;

    va_start (valist, fmt);
    vsnprintf (buf, sizeof (buf), fmt, valist);
    va_end (valist);

    if (printTimeStamp)
    {
        timeStampStr = trace_get_timestamp() + ' ';
    }
#if !DISABLE_SERIAL_TRACE
#if ENABLE_TIMESTAMP_ON_SERIAL_TRACE
    if (printTimeStamp)
    {
        Serial.print(timeStampStr);
        Serial.print(' ');
    }
#endif
    if (trace_flags & TRACE_FLAG_ERROR)
    {
        Serial.print("ERROR: ");
    }
    Serial.print(buf);
#endif

#if ENABLE_FILE_TRACE
    if (traceToFileIsWorking)
    {
        if (printTimeStamp)
        {
            traceFile.print(timeStampStr);
        }
        traceFile.print(buf);
        if (buf[0])
        {
            /* There was some data to be printed */
            traceFileFlushPending = true;
        }
    }
#endif

    if (errorFileIsOpened && (trace_flags & TRACE_FLAG_ERROR))
    {
        // if (printTimeStamp)
        // always print timestamp for errors!
        errorFile.print(timeStampStr);
        errorFile.print(buf);
    }

    size_t len = strnlen(buf, sizeof(buf));
    if (len > 0 && (buf[len - 1] == CHR_CR || buf[len - 1] == CHR_LF))
    {
        /* If end of printed string is new line character, put timestamp */
        /* at the beginning of line next time! */
        printTimeStamp = true;
#if ENABLE_FILE_TRACE
        if (traceToFileIsWorking)
        {
#if TRACE_LINE_COUNT_TO_FLUSH
            traceFileLineCntr++;
            if (traceFileFlushPending
                && traceFileLineCntr >= TRACE_LINE_COUNT_TO_FLUSH)
            {
                // Serial.print("#\n");
                traceFile.flush();
                traceFileLineCntr = 0;
                traceFileFlushPending = false;
                traceFileLastFlushTimesamp_ms = millis();
            }
#endif
#if TRACE_LINE_COUNT_TO_FLUSH && TRACE_ELAPSED_TIME_TO_FLUSH_MS
            else
#endif
#if TRACE_ELAPSED_TIME_TO_FLUSH_MS
            if (traceFileFlushPending
                && traceFileLastFlushTimesamp_ms + TRACE_ELAPSED_TIME_TO_FLUSH_MS <= millis())
            {
                // Serial.print("^\n");
                traceFile.flush();
                traceFileLineCntr = 0;
                traceFileFlushPending = false;
                traceFileLastFlushTimesamp_ms = millis();
            }
#endif

        }
#endif
        if (errorFileIsOpened && (trace_flags & TRACE_FLAG_ERROR))
        {
            errorFile.flush();
        }
    }
    else
    {
        printTimeStamp = false;
    }
}

bool trace_enable()
{
    bool ret = false;

#if ENABLE_FILE_TRACE
    File file;

    TRACE("Enabling file trace... ");
    file = LittleFS.open(ENABLE_TRACE_FILE_NAME, "w");
    if (file)
    {
        file.close();
        TRACE("Done.\n");
        traceToFileIsWorking = trace_file_start();
        ret = traceToFileIsWorking;
    }
    else
    {
        ERROR("Cannot enable file trace!\n");
        ret = false;
    }
#endif

    return ret;
}

bool trace_disable()
{
    bool ret = true;
#if ENABLE_FILE_TRACE
    File file;

    if (traceToFileIsWorking)
    {
        ret = trace_file_stop();
        traceToFileIsWorking = false;
    }
    TRACE("Disabling file trace... ");
    if (LittleFS.remove(ENABLE_TRACE_FILE_NAME))
    {
        TRACE("Done.\n");
    }
    else
    {
        ERROR("Cannot disable file trace!\n");
        ret = false;
    }
#endif

    return ret;
}

bool trace_file_enable_exists()
{
    bool exists = false;

#if ENABLE_FILE_TRACE
    if (LittleFS.exists(ENABLE_TRACE_FILE_NAME))
    {
        exists = true;
    }
#endif

    return exists;
}

bool trace_to_file_is_working()
{
    bool enabled = false;

#if ENABLE_FILE_TRACE
    if (traceToFileIsWorking)
    {
        enabled = true;
    }
#endif

    return enabled;
}

void trace_task()
{
#if ENABLE_FILE_TRACE && TRACE_ELAPSED_TIME_TO_FLUSH_MS
    if (traceToFileIsWorking
        && traceFileFlushPending
        && traceFileLastFlushTimesamp_ms + TRACE_ELAPSED_TIME_TO_FLUSH_MS <= millis())
    {
        // Serial.print("&\n");
        traceFile.flush();
        traceFileLineCntr = 0;
        traceFileLastFlushTimesamp_ms = millis();
        traceFileFlushPending = false;
    }
#endif
}
