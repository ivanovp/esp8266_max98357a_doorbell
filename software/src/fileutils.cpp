/**
 * @file        fileutils.cpp
 * @brief       File reading utilities
 * @author      Copyright (C) Peter Ivanov, 2023, 2024
 *
 * Created      2023-12-24 11:48:53
 * Last modify: 2024-11-23 12:49:22 ivanovp {Time-stamp}
 * Licence:     GPL
 */

#include <Arduino.h>

#include "common.h"
#include "config.h"
#include "trace.h"

#include <FS.h>       // File System for Web Server Files
#include <LittleFS.h> // This file system is used.

/*
 * Read given line of a file and return.
 *
 * @param[in] filename              File to open and read.
 * @param[in] lineIdx               Line index (nth) to read.
 * @param[in] error                 true: ERROR() is used if file not found, false: TRACE() is used if file not found.
 * @param[in] commentChar           Ignore this character and all data after this character
 * @param[in] removeTrailingSpace   true: Remove all trailing space characters
 *
 * @return nth line of file.
 */
String readStringFromFile(const String &filename, uint16_t lineIdx, bool error, uint8_t commentChar, bool removeTrailingSpaces)
{
    String content;
    uint16_t idx;
    int commentCharIdx;

    File file = LittleFS.open(filename, "r");
    if (file)
    {
        /* Read nth - 1 line from file and discard */
        idx = lineIdx;
        while (idx-- > 0)
        {
            file.readStringUntil('\n');
        }
        if (file.available())
        {
            content = file.readStringUntil('\n');
            if (commentChar != 0)
            {
                commentCharIdx = content.indexOf(commentChar);
                /* Check if comment character is in the string */
                if (commentCharIdx != -1)
                {
                    /* Remove comment character and evrything after that*/
                    content.remove(commentCharIdx);
                }
            }

            if (removeTrailingSpaces)
            {
                idx = content.length() - 1;
                /* Remove trailing spaces */
                while (content[idx] == ' ' || content[idx] == '\t')
                {
                    content.remove(idx);
                    idx--;
                }
            }
        }
        TRACE("Line #%i from file '%s': '%s'\n", lineIdx, filename.c_str(), content.c_str());
    }
    else
    {
        if (error)
        {
            ERROR("Failed to open file %s for reading\n", filename.c_str());
        }
        else
        {
            TRACE("Failed to open file %s for reading\n", filename.c_str());
        }
    }

    return content;
}


/*
 * Get line count of given file.
 *
 * @param[in] filename      File to open and read.
 * @param[in] error         true: ERROR() is used if file not found, false: TRACE() is used if file not found.
 *
 * @return Number of lines in the file.
 */
int32_t getLineCountOfFile(const String& filename, bool error=true)
{
    uint32_t lineCntr = -1;
    String content;

    File file = LittleFS.open(filename, "r");
    if (file)
    {
        if (file.available())
        {
            lineCntr = 0;
            do
            {
                file.readStringUntil('\n');
                lineCntr++;
            } while (file.available());
        }
    }
    else
    {
        if (error)
        {
            ERROR("Failed to open file %s for reading\n", filename.c_str());
        }
        else
        {
            TRACE("Failed to open file %s for reading\n", filename.c_str());
        }
    }

    return lineCntr;
}


/*
 * Read given line of a file and return.
 *
 * @param[in] filename              File to open and read.
 * @param[in] startLineIdx          Line index (nth) to read.
 * @param[in] error                 true: ERROR() is used if file not found, false: TRACE() is used if file not found.
 * @param[in] commentChar           Ignore this character and all data after this character
 * @param[in] removeTrailingSpace   true: Remove all trailing space characters
 *
 * @return Number of lines read from file.
 */
uint16_t readStringsFromFile(const String &filename, uint16_t startLineIdx,
                             String *a_lines, uint32_t a_max_lines, bool error,
                             uint8_t commentChar, bool removeTrailingSpaces)
{
    String content;
    uint16_t idx;
    int commentCharIdx;
    uint16_t lineCntr = 0;

    File file = LittleFS.open(filename, "r");
    if (file)
    {
        /* Read nth - 1 line from file and discard */
        idx = startLineIdx;
        while (idx-- > 0)
        {
            file.readStringUntil('\n');
        }
        while (file.available() && lineCntr < a_max_lines)
        {
            content = file.readStringUntil('\n');
            if (commentChar != 0)
            {
                commentCharIdx = content.indexOf(commentChar);
                /* Check if comment character is in the string */
                if (commentCharIdx != -1)
                {
                    /* Remove comment character and evrything after that*/
                    content.remove(commentCharIdx);
                }
            }

            if (removeTrailingSpaces)
            {
                idx = content.length() - 1;
                /* Remove trailing spaces */
                while (content[idx] == ' ' || content[idx] == '\t')
                {
                    content.remove(idx);
                    idx--;
                }
            }
            a_lines[lineCntr] = content;
            TRACE("Line #%i from file '%s': '%s'\n", lineCntr + startLineIdx, filename.c_str(), content.c_str());
            lineCntr++;
        }
    }
    else
    {
        if (error)
        {
            ERROR("Failed to open file %s for reading\n", filename.c_str());
        }
        else
        {
            TRACE("Failed to open file %s for reading\n", filename.c_str());
        }
    }

    return lineCntr;
}

uint32_t fileSize(const String &filename)
{
    uint32_t size = 0;
    File file;

    file = LittleFS.open(filename, "r");
    if (file)
    {
        size = file.size();
        file.close();
    }

    return size;
}
