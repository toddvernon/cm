//-------------------------------------------------------------------------------------------------
//
//  MarkUp.h
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  Syntax highlighting engine definitions.
//
//-------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <math.h>

#include <sys/types.h>
#include <iostream>
#include <cx/base/string.h>
#include <cx/screen/screen.h>

#include "ProgramDefaults.h"


#ifndef _MarkUp_h_
#define _MarkUp_h_

//---------------------------------------------------------------------------------------------------------
// Language mode enumeration - used to select syntax highlighting rules
//---------------------------------------------------------------------------------------------------------
enum LanguageMode {
    LANG_NONE = 0,
    LANG_C,
    LANG_CPP,
    LANG_SWIFT,
    LANG_PYTHON,
    LANG_JAVASCRIPT,
    LANG_GO,
    LANG_RUST,
    LANG_JAVA,
    LANG_SHELL,
    LANG_MAKEFILE,
    LANG_HTML,
    LANG_CSS,
    LANG_JSON,
    LANG_MARKDOWN
};

//---------------------------------------------------------------------------------------------------------
// LanguageSyntax - defines syntax rules for a language
//---------------------------------------------------------------------------------------------------------
struct LanguageSyntax {
    const char* name;               // display name
    const char* suffixes;           // file suffixes ".c.h.cpp"
    const char* lineComment;        // "//" or "#" or NULL
    const char* blockCommentStart;  // "/*" or NULL
    const char* blockCommentEnd;    // "*/" or NULL
    const char* multiStringDelim;   // "\"\"\"" or "`" or NULL
    int nestedBlockComments;        // 1 if block comments can nest (Swift, Rust)
    const char* keywords;           // comma-separated control keywords
    const char* types;              // comma-separated type keywords
    const char* constants;          // comma-separated constants (true, false, nil, etc.)
};

//---------------------------------------------------------------------------------------------------------
// ColorRegion - defines a region of text to exclude from colorization
// Used to prevent colorizing keywords/numbers inside strings and comments
//---------------------------------------------------------------------------------------------------------
#define MAX_COLOR_REGIONS 32

struct ColorRegion {
    int start;      // starting position (inclusive)
    int end;        // ending position (exclusive)
};

struct ColorRegions {
    ColorRegion regions[MAX_COLOR_REGIONS];
    int count;
};

//---------------------------------------------------------------------------------------------------------
// MarkUp class - handles syntax colorization
//---------------------------------------------------------------------------------------------------------
class MarkUp
{
  public:

    MarkUp( ProgramDefaults *pd, CxScreen *screenPtr );
    // constructor
    
    int
    parseTextConstant( CxString s, CxString item, unsigned long initialPos, unsigned long *start, unsigned long *end);
    // looks for text constand in the string and returns the character position before the text
    // as well as the text position after the text
    
    int
    parseClassMethod( CxString s, unsigned long *start, unsigned long *end);
    // looks for a signature looking for a class method in the text and returns its starting
    // and ending positions
    
    CxString
    injectTextConstantEntryExitText( CxString line, CxString item, CxString entryString, CxString exitString);
    // given a line of text and a string to look for the method prepends and append
    //string inserts the text in the string
    
    CxString
    injectMethodEntryExitText( CxString line, CxString entryString, CxString exitString );
    // given a line of text the method prepends and appends text to a pattern in the line that
    // looks like a method signature  something::something.
    
    CxString
    encapsolateWithEntryExitText( CxString line, CxString entryString, CxString exitString );
    // given a line of text with entry and exist string

    CxString
    colorizeText( CxString fullText, CxString visibleText );
    // given a string, colorize it according to the current language mode

    CxString
    colorizeHelpText( CxString fullText, CxString visibleText );
    // perform some colorization with help text

    void
    setLanguageFromFilePath( CxString filePath );
    // determine language mode from file suffix

    LanguageMode
    getLanguageMode( void );
    // return current language mode

private:

    CxString
    colorizeKeywords( CxString line, const char* keywords, CxString colorStart, CxString colorEnd );
    // colorize all matching keywords in line

    CxString
    colorizeStrings( CxString line, CxString colorStart, CxString colorEnd );
    // colorize string literals (same-line only)

    CxString
    colorizeNumbers( CxString line, CxString colorStart, CxString colorEnd );
    // colorize numeric literals

    CxString
    colorizeInlineComment( CxString line, const char* commentMarker, CxString colorStart, CxString colorEnd );
    // colorize trailing comment on a line

    CxString
    colorizeMakefileSpecial( CxString line, CxString varColor, CxString targetColor, CxString resetColor );
    // colorize makefile variables $(VAR) and targets

    CxString
    colorizeMarkdown( CxString line, CxString headerColor, CxString emphasisColor, CxString codeColor, CxString resetColor );
    // colorize markdown headers, bold, italic, code spans

    CxString
    colorizePythonDecorators( CxString line, CxString colorStart, CxString colorEnd );
    // colorize Python @decorators

    void
    findExclusionRegions( CxString line, const char* commentMarker, ColorRegions* regions );
    // find all string literals and inline comments to exclude from keyword/number colorization

    int
    isInsideRegion( int pos, ColorRegions* regions );
    // check if a position falls inside any exclusion region

    CxString
    colorizeNumbersWithExclusions( CxString line, CxString colorStart, CxString colorEnd, ColorRegions* regions );
    // colorize numeric literals, skipping exclusion regions

    CxString
    colorizeKeywordsWithExclusions( CxString line, const char* keywords, CxString colorStart, CxString colorEnd, ColorRegions* regions );
    // colorize keywords, skipping exclusion regions

    int
    parseNumber( char* data, int len, int startPos, int* endPos );
    // parse a numeric literal starting at startPos, returns 1 if valid number found
    // sets endPos to position after the number

    ProgramDefaults *programDefaults;
    // pointer to the startup defaults class

    CxScreen *screen;
    // pointer to the screen object

    LanguageMode _languageMode;
    // current language mode for syntax highlighting

    const LanguageSyntax* _currentSyntax;
    // pointer to current language syntax rules

};

#endif

