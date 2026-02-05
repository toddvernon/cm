//-------------------------------------------------------------------------------------------------
//
//  MarkUpParsing.cpp
//  cmacs
//
//  Parsing utility methods for MarkUp class.
//  Extracted from MarkUp.cpp to reduce file size.
//
//-------------------------------------------------------------------------------------------------

#include "MarkUp.h"
#include <string.h>


//-------------------------------------------------------------------------------------------------
// MarkUp::findExclusionRegions
//
// Find all string literals and inline comments in the line. These regions should be excluded
// from keyword and number colorization to prevent colorizing content inside strings/comments.
//
//-------------------------------------------------------------------------------------------------
void
MarkUp::findExclusionRegions( CxString line, const char* commentMarker, ColorRegions* regions )
{
    regions->count = 0;
    char *data = line.data();
    int len = line.length();
    int i = 0;

    while (i < len && regions->count < MAX_COLOR_REGIONS) {
        // Check for string literal (double quotes)
        if (data[i] == '"') {
            int startPos = i;
            i++;  // skip opening quote
            while (i < len) {
                if (data[i] == '\\' && i + 1 < len) {
                    i += 2;  // skip escaped character
                } else if (data[i] == '"') {
                    i++;  // skip closing quote
                    break;
                } else {
                    i++;
                }
            }
            regions->regions[regions->count].start = startPos;
            regions->regions[regions->count].end = i;
            regions->count++;
        }
        // Check for character literal (single quotes)
        else if (data[i] == '\'') {
            int startPos = i;
            i++;  // skip opening quote
            while (i < len) {
                if (data[i] == '\\' && i + 1 < len) {
                    i += 2;  // skip escaped character
                } else if (data[i] == '\'') {
                    i++;  // skip closing quote
                    break;
                } else {
                    i++;
                }
            }
            regions->regions[regions->count].start = startPos;
            regions->regions[regions->count].end = i;
            regions->count++;
        }
        // Check for inline comment
        else if (commentMarker != NULL && data[i] == commentMarker[0]) {
            // Check if this is the start of a comment marker
            int markerLen = strlen(commentMarker);
            int isComment = 1;
            for (int j = 0; j < markerLen && i + j < len; j++) {
                if (data[i + j] != commentMarker[j]) {
                    isComment = 0;
                    break;
                }
            }
            if (isComment) {
                // Comment goes to end of line
                regions->regions[regions->count].start = i;
                regions->regions[regions->count].end = len;
                regions->count++;
                break;  // comment consumes rest of line
            } else {
                i++;
            }
        }
        else {
            i++;
        }
    }
}


//-------------------------------------------------------------------------------------------------
// MarkUp::isInsideRegion
//
// Check if a position falls inside any exclusion region.
// Returns 1 if inside a region, 0 otherwise.
//
//-------------------------------------------------------------------------------------------------
int
MarkUp::isInsideRegion( int pos, ColorRegions* regions )
{
    for (int i = 0; i < regions->count; i++) {
        if (pos >= regions->regions[i].start && pos < regions->regions[i].end) {
            return 1;
        }
    }
    return 0;
}


//-------------------------------------------------------------------------------------------------
// MarkUp::parseNumber
//
// Parse a numeric literal starting at startPos. Returns 1 if a valid number was found,
// 0 otherwise. Sets endPos to the position after the number.
// Handles: hex (0x...), decimal, float, exponent notation, and type suffixes.
//
//-------------------------------------------------------------------------------------------------
int
MarkUp::parseNumber( char* data, int len, int startPos, int* endPos )
{
    int i = startPos;

    // Make sure it's not part of an identifier (check char before)
    if (startPos > 0) {
        char prev = data[startPos - 1];
        if ((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z') || prev == '_') {
            return 0;
        }
    }

    // Check for hex number (0x...)
    if (i + 1 < len && data[i] == '0' && (data[i+1] == 'x' || data[i+1] == 'X')) {
        i += 2;
        int hexStart = i;
        while (i < len && ((data[i] >= '0' && data[i] <= '9') ||
                           (data[i] >= 'a' && data[i] <= 'f') ||
                           (data[i] >= 'A' && data[i] <= 'F'))) {
            i++;
        }
        if (i > hexStart) {
            *endPos = i;
            return 1;
        }
        return 0;
    }

    // Check for decimal/float number
    if (data[i] >= '0' && data[i] <= '9') {
        while (i < len && data[i] >= '0' && data[i] <= '9') i++;

        // Check for decimal point
        if (i < len && data[i] == '.') {
            i++;
            while (i < len && data[i] >= '0' && data[i] <= '9') i++;
        }

        // Check for exponent
        if (i < len && (data[i] == 'e' || data[i] == 'E')) {
            i++;
            if (i < len && (data[i] == '+' || data[i] == '-')) i++;
            while (i < len && data[i] >= '0' && data[i] <= '9') i++;
        }

        // Check for suffix (f, F, l, L, u, U, etc.)
        while (i < len && (data[i] == 'f' || data[i] == 'F' ||
                           data[i] == 'l' || data[i] == 'L' ||
                           data[i] == 'u' || data[i] == 'U')) {
            i++;
        }

        // Make sure number isn't followed by identifier char
        if (i < len) {
            char next = data[i];
            if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || next == '_') {
                return 0;
            }
        }

        *endPos = i;
        return 1;
    }

    return 0;
}


//-------------------------------------------------------------------------------------------------
// MarkUp::colorizeNumbersWithExclusions
//
// Colorize numeric literals, but skip any numbers that fall inside exclusion regions
// (strings or comments).
//
//-------------------------------------------------------------------------------------------------
CxString
MarkUp::colorizeNumbersWithExclusions( CxString line, CxString colorStart, CxString colorEnd, ColorRegions* regions )
{
    if (colorStart.length() == 0) return line;

    CxString result;
    char *data = line.data();
    int len = line.length();
    int lastPos = 0;
    int i = 0;

    while (i < len) {
        // Skip if inside an exclusion region
        if (isInsideRegion(i, regions)) {
            i++;
            continue;
        }

        // Try to parse a number at this position
        int endPos = 0;
        if (parseNumber(data, len, i, &endPos)) {
            // Append text before the number
            if (i > lastPos) {
                result = result + line.subString(lastPos, i - lastPos);
            }
            // Append colorized number
            result = result + colorStart;
            result = result + line.subString(i, endPos - i);
            result = result + colorEnd;
            lastPos = endPos;
            i = endPos;
        } else {
            i++;
        }
    }

    // Append remaining text
    if (lastPos < len) {
        result = result + line.subString(lastPos, len - lastPos);
    }

    return (lastPos > 0) ? result : line;
}


//-------------------------------------------------------------------------------------------------
// MarkUp::colorizeKeywordsWithExclusions
//
// Colorize keywords, but skip any that fall inside exclusion regions (strings or comments).
//
//-------------------------------------------------------------------------------------------------
CxString
MarkUp::colorizeKeywordsWithExclusions( CxString line, const char* keywords, CxString colorStart, CxString colorEnd, ColorRegions* regions )
{
    if (keywords == NULL) return line;

    // Parse comma-separated keywords and colorize each
    const char* p = keywords;
    while (*p) {
        // Find end of current keyword
        const char* start = p;
        while (*p && *p != ',') p++;

        int keywordLen = p - start;
        if (keywordLen > 0 && keywordLen < 64) {
            char keyword[64];
            strncpy(keyword, start, keywordLen);
            keyword[keywordLen] = '\0';

            // Find and colorize this keyword, skipping exclusion regions
            unsigned long initial = 0;
            unsigned long matchStart = 0;
            unsigned long matchEnd = 0;
            CxString result;
            unsigned long lastPos = 0;
            CxString item(keyword);

            while (parseTextConstant(line, item, initial, &matchStart, &matchEnd)) {
                // Check if this match is inside an exclusion region
                if (!isInsideRegion((int)matchStart, regions)) {
                    // Append text before this match
                    if (matchStart > lastPos) {
                        result = result + line.subString(lastPos, matchStart - lastPos);
                    }
                    // Append colorized match
                    result = result + colorStart;
                    result = result + line.subString(matchStart, matchEnd - matchStart);
                    result = result + colorEnd;
                    lastPos = matchEnd;
                }
                initial = matchEnd;
            }

            // Append remaining text after last match
            if (lastPos > 0) {
                if (lastPos < (unsigned long)line.length()) {
                    result = result + line.subString(lastPos, line.length() - lastPos);
                }
                line = result;
            }
        }

        if (*p == ',') p++;
    }

    return line;
}

