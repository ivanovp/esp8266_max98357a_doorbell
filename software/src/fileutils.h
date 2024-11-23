/**
 * @file        fileutils.h
 * @brief       Definitions of fileutils.cpp
 * @author      Copyright (C) Peter Ivanov, 2023, 2024
 *
 * Created      2023-12-24 11:48:53
 * Last modify: 2024-11-23 12:49:26 ivanovp {Time-stamp}
 * Licence:     GPL
 */

#ifndef INCLUDE_FILEUTILS_H
#define INCLUDE_FILEUTILS_H

#include <Arduino.h>
#include "common.h"
#include "config.h"

extern String readStringFromFile(const String &filename, uint16_t lineIdx = 0, bool error=false,
                                 uint8_t commentChar = COMMENT_CHAR, bool removeTrailingSpaces = true);
extern int32_t getLineCountOfFile(const String& filename, bool error=true);
extern uint16_t readStringsFromFile(const String &filename, uint16_t startLineIdx,
                                    String *a_lines, uint32_t a_max_lines, bool error=false,
                                    uint8_t commentChar = COMMENT_CHAR, bool removeTrailingSpaces = true);
extern uint32_t fileSize(const String &filename);
#endif /* INCLUDE_FILEUTILS_H */

