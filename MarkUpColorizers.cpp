//-------------------------------------------------------------------------------------------------
//
//  MarkUpColorizers.cpp
//  cmacs
//
//  Element colorizer methods for MarkUp class.
//  Extracted from MarkUp.cpp to reduce file size.
//
//-------------------------------------------------------------------------------------------------

#include "MarkUp.h"
#include <string.h>


//-------------------------------------------------------------------------------------------------
// MarkUp::colorizeStrings
//
// Colorize string literals that start and end on the same line.
// Handles double quotes and single quotes, with escaped quote support.
// Uses string concatenation to build result (more reliable than in-place insertion).
//
//-------------------------------------------------------------------------------------------------
CxString
MarkUp::colorizeStrings( CxString line, CxString colorStart, CxString colorEnd )
{
    if (colorStart.length() == 0) return line;

    CxString result;
    char *data = line.data();
    int len = line.length();
    int lastPos = 0;
    int i = 0;

    while (i < len) {
        // Look for start of string
        if (data[i] == '"' || data[i] == '\'') {
            char quoteChar = data[i];
            int startPos = i;
            i++;

            // Find matching end quote
            while (i < len) {
                if (data[i] == '\\' && i + 1 < len) {
                    // Skip escaped character
                    i += 2;
                } else if (data[i] == quoteChar) {
                    // Found end quote - build colorized segment
                    int endPos = i + 1;

                    // Append text before the string
                    if (startPos > lastPos) {
                        result = result + line.subString(lastPos, startPos - lastPos);
                    }

                    // Append colorized string
                    result = result + colorStart;
                    result = result + line.subString(startPos, endPos - startPos);
                    result = result + colorEnd;

                    lastPos = endPos;
                    i = endPos;
                    break;
                } else {
                    i++;
                }
            }
        } else {
            i++;
        }
    }

    // Append remaining text
    if (lastPos < len) {
        result = result + line.subString(lastPos, len - lastPos);
    }

    return result.length() > 0 ? result : line;
}


//-------------------------------------------------------------------------------------------------
// MarkUp::colorizeNumbers
//
// Colorize numeric literals: integers, floats, hex (0x...).
// Uses string concatenation to build result.
//
//-------------------------------------------------------------------------------------------------
CxString
MarkUp::colorizeNumbers( CxString line, CxString colorStart, CxString colorEnd )
{
    if (colorStart.length() == 0) return line;

    CxString result;
    char *data = line.data();
    int len = line.length();
    int lastPos = 0;
    int i = 0;

    while (i < len) {
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

    return result.length() > 0 ? result : line;
}


//-------------------------------------------------------------------------------------------------
// MarkUp::colorizeInlineComment
//
// Colorize a trailing comment on a line (e.g., code // comment).
// Uses string concatenation to build result.
//
//-------------------------------------------------------------------------------------------------
CxString
MarkUp::colorizeInlineComment( CxString line, const char* commentMarker, CxString colorStart, CxString colorEnd )
{
    if (commentMarker == NULL || colorStart.length() == 0) return line;

    // Find comment marker, but not inside a string
    char *data = line.data();
    int len = line.length();
    int markerLen = strlen(commentMarker);
    int inString = 0;
    char stringChar = 0;

    for (int i = 0; i < len - markerLen + 1; i++) {
        // Track string state
        if (!inString && (data[i] == '"' || data[i] == '\'')) {
            inString = 1;
            stringChar = data[i];
        } else if (inString && data[i] == '\\' && i + 1 < len) {
            i++; // skip escaped char
        } else if (inString && data[i] == stringChar) {
            inString = 0;
        } else if (!inString) {
            // Check for comment marker
            int match = 1;
            for (int j = 0; j < markerLen; j++) {
                if (data[i + j] != commentMarker[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                // Found comment - build result with colorized comment
                CxString result;
                if (i > 0) {
                    result = line.subString(0, i);
                }
                result = result + colorStart;
                result = result + line.subString(i, len - i);
                result = result + colorEnd;
                return result;
            }
        }
    }

    return line;
}


//-------------------------------------------------------------------------------------------------
// MarkUp::colorizeMakefileSpecial
//
// Colorize makefile-specific syntax: $(VAR), ${VAR}, targets, automatic variables.
// Uses string concatenation to build result.
//
//-------------------------------------------------------------------------------------------------
CxString
MarkUp::colorizeMakefileSpecial( CxString line, CxString varColor, CxString targetColor, CxString resetColor )
{
    char *data = line.data();
    int len = line.length();

    // Check for target line (word followed by : at reasonable position, not ::)
    int colonPos = line.index(":");
    if (colonPos > 0 && colonPos < 40 && targetColor.length()) {
        // Make sure it's not := or :: or part of a variable
        char afterColon = (colonPos + 1 < len) ? data[colonPos + 1] : 0;
        if (afterColon != '=' && afterColon != ':') {
            // Check if everything before colon looks like a target name
            int isTarget = 1;
            for (int i = 0; i < colonPos; i++) {
                char c = data[i];
                if (c == '$' || c == '(' || c == ')' || c == '=') {
                    isTarget = 0;
                    break;
                }
            }
            if (isTarget) {
                // Build result with colorized target
                CxString result = targetColor;
                result = result + line.subString(0, colonPos);
                result = result + resetColor;
                result = result + line.subString(colonPos, len - colonPos);
                line = result;
                data = line.data();
                len = line.length();
            }
        }
    }

    // Colorize variable references $(VAR) and ${VAR}
    if (varColor.length()) {
        CxString result;
        int lastPos = 0;
        int i = 0;

        while (i < len - 1) {
            if (data[i] == '$') {
                int startPos = i;
                int endPos = -1;

                if (data[i + 1] == '(') {
                    // Find closing paren
                    int j = i + 2;
                    while (j < len && data[j] != ')') j++;
                    if (j < len) endPos = j + 1;
                } else if (data[i + 1] == '{') {
                    // Find closing brace
                    int j = i + 2;
                    while (j < len && data[j] != '}') j++;
                    if (j < len) endPos = j + 1;
                } else if (data[i + 1] == '@' || data[i + 1] == '<' ||
                           data[i + 1] == '^' || data[i + 1] == '?' ||
                           data[i + 1] == '*' || data[i + 1] == '+') {
                    // Automatic variable like $@, $<, $^
                    endPos = i + 2;
                }

                if (endPos > startPos) {
                    // Append text before the variable
                    if (startPos > lastPos) {
                        result = result + line.subString(lastPos, startPos - lastPos);
                    }
                    // Append colorized variable
                    result = result + varColor;
                    result = result + line.subString(startPos, endPos - startPos);
                    result = result + resetColor;
                    lastPos = endPos;
                    i = endPos;
                    continue;
                }
            }
            i++;
        }

        // Append remaining text
        if (lastPos > 0) {
            if (lastPos < len) {
                result = result + line.subString(lastPos, len - lastPos);
            }
            return result;
        }
    }

    return line;
}


//-------------------------------------------------------------------------------------------------
// MarkUp::colorizeMarkdown
//
// Colorize markdown syntax: headers, bold, italic, inline code.
// Uses string concatenation to build result.
//
//-------------------------------------------------------------------------------------------------
CxString
MarkUp::colorizeMarkdown( CxString line, CxString headerColor, CxString emphasisColor, CxString codeColor, CxString resetColor )
{
    char *data = line.data();
    int len = line.length();

    if (len == 0) return line;

    // Check for header (# at start of line)
    if (data[0] == '#' && headerColor.length()) {
        return encapsolateWithEntryExitText(line, headerColor, resetColor);
    }

    // Colorize inline code `code`
    if (codeColor.length()) {
        CxString result;
        int lastPos = 0;
        int i = 0;

        while (i < len) {
            if (data[i] == '`') {
                int startPos = i;
                i++;
                while (i < len && data[i] != '`') i++;
                if (i < len) {
                    int endPos = i + 1;

                    // Append text before the code
                    if (startPos > lastPos) {
                        result = result + line.subString(lastPos, startPos - lastPos);
                    }
                    // Append colorized code
                    result = result + codeColor;
                    result = result + line.subString(startPos, endPos - startPos);
                    result = result + resetColor;
                    lastPos = endPos;
                    i = endPos;
                }
            } else {
                i++;
            }
        }

        if (lastPos > 0) {
            if (lastPos < len) {
                result = result + line.subString(lastPos, len - lastPos);
            }
            line = result;
            data = line.data();
            len = line.length();
        }
    }

    // Colorize bold **text** or __text__
    if (emphasisColor.length()) {
        CxString result;
        int lastPos = 0;
        int i = 0;

        while (i < len - 3) {
            if ((data[i] == '*' && data[i+1] == '*') ||
                (data[i] == '_' && data[i+1] == '_')) {
                char marker = data[i];
                int startPos = i;
                i += 2;

                // Find closing **
                while (i < len - 1) {
                    if (data[i] == marker && data[i+1] == marker) {
                        int endPos = i + 2;

                        // Append text before bold
                        if (startPos > lastPos) {
                            result = result + line.subString(lastPos, startPos - lastPos);
                        }
                        // Append colorized bold
                        result = result + emphasisColor;
                        result = result + line.subString(startPos, endPos - startPos);
                        result = result + resetColor;
                        lastPos = endPos;
                        i = endPos;
                        break;
                    }
                    i++;
                }
            } else {
                i++;
            }
        }

        if (lastPos > 0) {
            if (lastPos < len) {
                result = result + line.subString(lastPos, len - lastPos);
            }
            line = result;
            data = line.data();
            len = line.length();
        }
    }

    return line;
}


//-------------------------------------------------------------------------------------------------
// MarkUp::colorizePythonDecorators
//
// Colorize Python @decorator lines.
// Uses string concatenation to build result.
//
//-------------------------------------------------------------------------------------------------
CxString
MarkUp::colorizePythonDecorators( CxString line, CxString colorStart, CxString colorEnd )
{
    if (colorStart.length() == 0) return line;

    // Check if line starts with @ (after optional whitespace)
    char *data = line.data();
    int len = line.length();
    int i = 0;

    // Skip leading whitespace
    while (i < len && (data[i] == ' ' || data[i] == '\t')) i++;

    if (i < len && data[i] == '@') {
        // This is a decorator line - colorize the whole decorator
        int startPos = i;
        i++;

        // Find end of decorator name (could include dots for @module.decorator)
        while (i < len && (data[i] != '(' && data[i] != ' ' && data[i] != '\t' && data[i] != '\n')) {
            i++;
        }

        int endPos = i;

        // Build result with colorized decorator
        CxString result;
        if (startPos > 0) {
            result = line.subString(0, startPos);
        }
        result = result + colorStart;
        result = result + line.subString(startPos, endPos - startPos);
        result = result + colorEnd;
        if (endPos < len) {
            result = result + line.subString(endPos, len - endPos);
        }
        return result;
    }

    return line;
}

