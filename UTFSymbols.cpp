//-------------------------------------------------------------------------------------------------
//
//  UTFSymbols.cpp
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  UTF-8 symbol table for box drawing and common symbols (macOS, Linux only).
//
//-------------------------------------------------------------------------------------------------

#include "UTFSymbols.h"

#ifdef CM_UTF8_SUPPORT

//-------------------------------------------------------------------------------------------------
// Symbol table
//
// Box drawing characters from Unicode U+2500-U+257F
// Symbols are matched using literal prefix matching.
//-------------------------------------------------------------------------------------------------
UTFSymbolEntry UTFSymbols::_symbols[] = {

    //---------------------------------------------------------------------------------------------
    // Single-line box drawing
    //---------------------------------------------------------------------------------------------
    { "box-horizontal",
      "\xe2\x94\x80",           // ─ U+2500
      "Horizontal line" },

    { "box-vertical",
      "\xe2\x94\x82",           // │ U+2502
      "Vertical line" },

    { "box-upper-left",
      "\xe2\x94\x8c",           // ┌ U+250C
      "Upper left corner" },

    { "box-upper-right",
      "\xe2\x94\x90",           // ┐ U+2510
      "Upper right corner" },

    { "box-lower-left",
      "\xe2\x94\x94",           // └ U+2514
      "Lower left corner" },

    { "box-lower-right",
      "\xe2\x94\x98",           // ┘ U+2518
      "Lower right corner" },

    { "box-tee-left",
      "\xe2\x94\x9c",           // ├ U+251C
      "Left tee" },

    { "box-tee-right",
      "\xe2\x94\xa4",           // ┤ U+2524
      "Right tee" },

    { "box-tee-top",
      "\xe2\x94\xac",           // ┬ U+252C
      "Top tee" },

    { "box-tee-bottom",
      "\xe2\x94\xb4",           // ┴ U+2534
      "Bottom tee" },

    { "box-cross",
      "\xe2\x94\xbc",           // ┼ U+253C
      "Cross / plus" },

    //---------------------------------------------------------------------------------------------
    // Double-line box drawing
    //---------------------------------------------------------------------------------------------
    { "box-double-horizontal",
      "\xe2\x95\x90",           // ═ U+2550
      "Double horizontal line" },

    { "box-double-vertical",
      "\xe2\x95\x91",           // ║ U+2551
      "Double vertical line" },

    { "box-double-upper-left",
      "\xe2\x95\x94",           // ╔ U+2554
      "Double upper left corner" },

    { "box-double-upper-right",
      "\xe2\x95\x97",           // ╗ U+2557
      "Double upper right corner" },

    { "box-double-lower-left",
      "\xe2\x95\x9a",           // ╚ U+255A
      "Double lower left corner" },

    { "box-double-lower-right",
      "\xe2\x95\x9d",           // ╝ U+255D
      "Double lower right corner" },

    { "box-double-tee-left",
      "\xe2\x95\xa0",           // ╠ U+2560
      "Double left tee" },

    { "box-double-tee-right",
      "\xe2\x95\xa3",           // ╣ U+2563
      "Double right tee" },

    { "box-double-tee-top",
      "\xe2\x95\xa6",           // ╦ U+2566
      "Double top tee" },

    { "box-double-tee-bottom",
      "\xe2\x95\xa9",           // ╩ U+2569
      "Double bottom tee" },

    { "box-double-cross",
      "\xe2\x95\xac",           // ╬ U+256C
      "Double cross" },

    //---------------------------------------------------------------------------------------------
    // Rounded corners
    //---------------------------------------------------------------------------------------------
    { "box-round-upper-left",
      "\xe2\x95\xad",           // ╭ U+256D
      "Rounded upper left" },

    { "box-round-upper-right",
      "\xe2\x95\xae",           // ╮ U+256E
      "Rounded upper right" },

    { "box-round-lower-right",
      "\xe2\x95\xaf",           // ╯ U+256F
      "Rounded lower right" },

    { "box-round-lower-left",
      "\xe2\x95\xb0",           // ╰ U+2570
      "Rounded lower left" },

    //---------------------------------------------------------------------------------------------
    // Block elements
    //---------------------------------------------------------------------------------------------
    { "block-full",
      "\xe2\x96\x88",           // █ U+2588
      "Full block" },

    { "block-light",
      "\xe2\x96\x91",           // ░ U+2591
      "Light shade" },

    { "block-medium",
      "\xe2\x96\x92",           // ▒ U+2592
      "Medium shade" },

    { "block-dark",
      "\xe2\x96\x93",           // ▓ U+2593
      "Dark shade" },

    //---------------------------------------------------------------------------------------------
    // Common symbols (sym- prefix for utf-symbol command)
    //---------------------------------------------------------------------------------------------
    { "sym-bullet",
      "\xe2\x80\xa2",           // • U+2022
      "Bullet point" },

    { "sym-check",
      "\xe2\x9c\x93",           // ✓ U+2713
      "Check mark" },

    { "sym-cross",
      "\xe2\x9c\x97",           // ✗ U+2717
      "Cross mark / X" },

    { "sym-star",
      "\xe2\x98\x85",           // ★ U+2605
      "Black star" },

    { "sym-diamond",
      "\xe2\x97\x86",           // ◆ U+25C6
      "Black diamond" },

    { "sym-arrow-left",
      "\xe2\x86\x90",           // ← U+2190
      "Left arrow" },

    { "sym-arrow-up",
      "\xe2\x86\x91",           // ↑ U+2191
      "Up arrow" },

    { "sym-arrow-right",
      "\xe2\x86\x92",           // → U+2192
      "Right arrow" },

    { "sym-arrow-down",
      "\xe2\x86\x93",           // ↓ U+2193
      "Down arrow" },

    //---------------------------------------------------------------------------------------------
    // End of table
    //---------------------------------------------------------------------------------------------
    { NULL, NULL, NULL }
};


//-------------------------------------------------------------------------------------------------
// UTFSymbols::findMatches
//
// Note: Uses Completer for consistent matching behavior across the codebase.
//-------------------------------------------------------------------------------------------------
int
UTFSymbols::findMatches( CxString prefix, UTFSymbolEntry **matches, int maxMatches )
{
    int count = 0;

    for (int i = 0; _symbols[i].name != NULL && count < maxMatches; i++) {
        if (Completer::matchesPrefix( _symbols[i].name, prefix )) {
            matches[count++] = &_symbols[i];
        }
    }

    return count;
}


//-------------------------------------------------------------------------------------------------
// UTFSymbols::findExact
//-------------------------------------------------------------------------------------------------
UTFSymbolEntry *
UTFSymbols::findExact( CxString name )
{
    for (int i = 0; _symbols[i].name != NULL; i++) {
        if (name == _symbols[i].name) {
            return &_symbols[i];
        }
    }

    return NULL;
}


//-------------------------------------------------------------------------------------------------
// UTFSymbols::completePrefix
//-------------------------------------------------------------------------------------------------
CxString
UTFSymbols::completePrefix( CxString prefix )
{
    UTFSymbolEntry *matches[64];
    int count = findMatches( prefix, matches, 64 );

    if (count == 0) {
        return prefix;
    }

    if (count == 1) {
        return matches[0]->name;
    }

    // Collect names into array for Completer
    CxString names[64];
    for (int i = 0; i < count; i++) {
        names[i] = matches[i]->name;
    }

    return Completer::findCommonPrefix( names, count );
}


//-------------------------------------------------------------------------------------------------
// UTFSymbols::symbolCount
//-------------------------------------------------------------------------------------------------
int
UTFSymbols::symbolCount( void )
{
    int count = 0;

    while (_symbols[count].name != NULL) {
        count++;
    }

    return count;
}


//-------------------------------------------------------------------------------------------------
// UTFSymbols::symbolAt
//-------------------------------------------------------------------------------------------------
UTFSymbolEntry *
UTFSymbols::symbolAt( int index )
{
    if (index < 0 || index >= symbolCount()) {
        return NULL;
    }

    return &_symbols[index];
}


//-------------------------------------------------------------------------------------------------
// UTFSymbols::findMatchesFiltered
//
// Find symbols matching prefix, but only consider symbols that start with filter.
// Uses Completer for consistent matching behavior.
//-------------------------------------------------------------------------------------------------
int
UTFSymbols::findMatchesFiltered( CxString prefix, CxString filter, UTFSymbolEntry **matches, int maxMatches )
{
    int count = 0;

    // prepend filter to prefix for matching
    CxString fullPrefix = filter;
    fullPrefix += prefix;

    for (int i = 0; _symbols[i].name != NULL && count < maxMatches; i++) {
        // symbol must start with filter
        CxString symName = _symbols[i].name;
        if (symName.length() >= filter.length()) {
            CxString symStart = symName.subString(0, filter.length());
            if (symStart == filter) {
                // now check if it matches the full prefix
                if (Completer::matchesPrefix( _symbols[i].name, fullPrefix )) {
                    matches[count++] = &_symbols[i];
                }
            }
        }
    }

    return count;
}


//-------------------------------------------------------------------------------------------------
// UTFSymbols::completePrefixFiltered
//
// Returns the common prefix among filtered matches, with the filter prefix stripped.
// For example, with filter "box-" and matches ["box-upper-left", "box-upper-right"],
// returns "upper-" (the common prefix without "box-").
//-------------------------------------------------------------------------------------------------
CxString
UTFSymbols::completePrefixFiltered( CxString prefix, CxString filter )
{
    UTFSymbolEntry *matches[64];
    int count = findMatchesFiltered( prefix, filter, matches, 64 );

    if (count == 0) {
        return prefix;
    }

    if (count == 1) {
        // return name without the filter prefix
        CxString fullName = matches[0]->name;
        return fullName.subString( filter.length(), fullName.length() - filter.length() );
    }

    // Collect names into array for Completer
    CxString names[64];
    for (int i = 0; i < count; i++) {
        names[i] = matches[i]->name;
    }

    CxString commonPrefix = Completer::findCommonPrefix( names, count );

    // return without the filter prefix
    if (commonPrefix.length() > filter.length()) {
        return commonPrefix.subString( filter.length(), commonPrefix.length() - filter.length() );
    }
    return prefix;
}


//-------------------------------------------------------------------------------------------------
// UTFSymbols::symbolCountFiltered
//-------------------------------------------------------------------------------------------------
int
UTFSymbols::symbolCountFiltered( CxString filter )
{
    int count = 0;

    for (int i = 0; _symbols[i].name != NULL; i++) {
        CxString symName = _symbols[i].name;
        if (symName.length() >= filter.length()) {
            CxString symStart = symName.subString(0, filter.length());
            if (symStart == filter) {
                count++;
            }
        }
    }

    return count;
}


//-------------------------------------------------------------------------------------------------
// UTFSymbols::symbolAtFiltered
//-------------------------------------------------------------------------------------------------
UTFSymbolEntry *
UTFSymbols::symbolAtFiltered( int index, CxString filter )
{
    int count = 0;

    for (int i = 0; _symbols[i].name != NULL; i++) {
        CxString symName = _symbols[i].name;
        if (symName.length() >= filter.length()) {
            CxString symStart = symName.subString(0, filter.length());
            if (symStart == filter) {
                if (count == index) {
                    return &_symbols[i];
                }
                count++;
            }
        }
    }

    return NULL;
}


#endif // CM_UTF8_SUPPORT
