//-------------------------------------------------------------------------------------------------
//
//  UTFSymbols.h
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  UTF-8 symbol table definitions (macOS, Linux only).
//
//-------------------------------------------------------------------------------------------------

#ifndef _UTFSymbols_h_
#define _UTFSymbols_h_

#include "CmTypes.h"

#ifdef CM_UTF8_SUPPORT

#include <cx/base/string.h>
#include <cx/commandcompleter/completer.h>

//-------------------------------------------------------------------------------------------------
// UTFSymbolEntry - defines a single UTF symbol
//-------------------------------------------------------------------------------------------------
struct UTFSymbolEntry
{
    const char *name;           // "box-upper-left", "box-horizontal", etc.
    const char *utf8;           // UTF-8 byte sequence
    const char *description;    // "Upper left corner"
};

//-------------------------------------------------------------------------------------------------
// UTFSymbols - manages UTF symbol table and provides completion
//-------------------------------------------------------------------------------------------------
class UTFSymbols
{
public:

    static int findMatches( CxString prefix, UTFSymbolEntry **matches, int maxMatches );
    // returns count of symbols matching prefix (literal prefix match)

    static int findMatchesFiltered( CxString prefix, CxString filter, UTFSymbolEntry **matches, int maxMatches );
    // returns count of symbols matching prefix, filtered to only symbols starting with filter

    static UTFSymbolEntry *findExact( CxString name );
    // returns exact match or NULL

    static CxString completePrefix( CxString prefix );
    // returns longest common prefix among all matches

    static CxString completePrefixFiltered( CxString prefix, CxString filter );
    // returns longest common prefix among filtered matches

    static int symbolCount( void );
    // total number of symbols

    static int symbolCountFiltered( CxString filter );
    // count of symbols starting with filter

    static UTFSymbolEntry *symbolAt( int index );
    // return symbol at index

    static UTFSymbolEntry *symbolAtFiltered( int index, CxString filter );
    // return symbol at index within filtered set

private:

    static UTFSymbolEntry _symbols[];
    // Note: Matching uses Completer static methods
};

#endif // CM_UTF8_SUPPORT

#endif // _UTFSymbols_h_
