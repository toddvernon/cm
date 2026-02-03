//-------------------------------------------------------------------------------------------------
//
//  MarkUp.cpp
//  cmacs
//
//  Created by Todd Vernon on 6/24/22.
//  Copyright © 2022 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// MarkUp.cpp
// 
// Edit View the class that handles display of the edit buffer on the screen.  It handles
// screen sizing and resizing and reframing the visible part of the edit buffer on the
// visible screen.
//
//-------------------------------------------------------------------------------------------------


#include "MarkUp.h"
#include <string.h>

//-------------------------------------------------------------------------------------------------
// Language Syntax Definitions
//
// Each entry defines the syntax rules for a language. Only C/C++ and Swift are fully implemented.
// Other languages have placeholder entries for future expansion.
//
//-------------------------------------------------------------------------------------------------
static LanguageSyntax languageSyntaxTable[] = {

    // C
    { "C", ".c.h",
      "//", "/*", "*/", NULL, 0,
      "if,else,while,for,do,switch,case,default,break,continue,return,goto,sizeof,typedef,struct,union,enum,extern,static,const,volatile,register,auto,inline",
      "int,char,short,long,unsigned,signed,float,double,void,size_t,FILE",
      "NULL,TRUE,FALSE,true,false" },

    // C++
    { "C++", ".cpp.hpp.cc.cxx.hxx.C",
      "//", "/*", "*/", NULL, 0,
      "if,else,while,for,do,switch,case,default,break,continue,return,goto,sizeof,typedef,struct,union,enum,extern,static,const,volatile,register,auto,inline,class,public,private,protected,virtual,override,final,template,typename,namespace,using,new,delete,try,catch,throw,const_cast,static_cast,dynamic_cast,reinterpret_cast,explicit,friend,mutable,operator,this",
      "int,char,short,long,unsigned,signed,float,double,void,bool,size_t,wchar_t",
      "NULL,TRUE,FALSE,true,false,nullptr" },

    // Swift
    { "Swift", ".swift",
      "//", "/*", "*/", "\"\"\"", 1,  // nested block comments
      "if,else,guard,switch,case,default,for,while,repeat,break,continue,fallthrough,return,throw,throws,rethrows,try,catch,defer,do,import,func,class,struct,enum,protocol,extension,typealias,associatedtype,init,deinit,subscript,convenience,required,override,final,open,public,private,fileprivate,internal,static,mutating,nonmutating,lazy,weak,unowned,inout,let,var,where,is,as,in,self,Self,super,async,await,actor",
      "Int,Int8,Int16,Int32,Int64,UInt,UInt8,UInt16,UInt32,UInt64,Float,Double,Bool,String,Character,Array,Dictionary,Set,Optional,Result,Error,Void,Any,AnyObject,Never",
      "nil,true,false" },

    // Python (placeholder - line-by-line only, no block comments)
    { "Python", ".py",
      "#", NULL, NULL, "\"\"\"", 0,
      "if,elif,else,while,for,break,continue,return,pass,raise,try,except,finally,with,as,import,from,class,def,lambda,yield,global,nonlocal,assert,del,in,is,not,and,or,async,await",
      "int,str,float,bool,list,dict,set,tuple,bytes,type,object",
      "None,True,False" },

    // JavaScript (placeholder)
    { "JavaScript", ".js.jsx.ts.tsx",
      "//", "/*", "*/", "`", 0,
      "if,else,switch,case,default,for,while,do,break,continue,return,throw,try,catch,finally,function,class,extends,new,delete,typeof,instanceof,in,of,let,const,var,import,export,async,await,yield",
      "undefined,null,NaN,Infinity",
      "true,false" },

    // Go (placeholder)
    { "Go", ".go",
      "//", "/*", "*/", "`", 0,
      "if,else,switch,case,default,for,range,break,continue,return,go,defer,select,chan,func,type,struct,interface,map,package,import,const,var",
      "int,int8,int16,int32,int64,uint,uint8,uint16,uint32,uint64,float32,float64,complex64,complex128,byte,rune,string,bool,error",
      "nil,true,false,iota" },

    // Rust (placeholder - has nested comments like Swift)
    { "Rust", ".rs",
      "//", "/*", "*/", NULL, 1,  // nested block comments
      "if,else,match,loop,while,for,in,break,continue,return,fn,let,mut,const,static,type,struct,enum,trait,impl,pub,mod,use,crate,self,super,as,where,async,await,move,dyn,unsafe,extern",
      "i8,i16,i32,i64,i128,isize,u8,u16,u32,u64,u128,usize,f32,f64,bool,char,str,String,Vec,Option,Result,Box",
      "true,false,None,Some,Ok,Err" },

    // Java (placeholder)
    { "Java", ".java",
      "//", "/*", "*/", NULL, 0,
      "if,else,switch,case,default,for,while,do,break,continue,return,throw,throws,try,catch,finally,class,interface,extends,implements,new,instanceof,import,package,public,private,protected,static,final,abstract,synchronized,volatile,transient,native,strictfp,assert,enum,this,super",
      "int,long,short,byte,float,double,char,boolean,void,String,Object",
      "true,false,null" },

    // Shell (placeholder)
    { "Shell", ".sh.bash.zsh.ksh",
      "#", NULL, NULL, NULL, 0,
      "if,then,else,elif,fi,case,esac,for,while,until,do,done,in,function,return,exit,break,continue,local,export,source,alias,unalias,set,unset,shift,trap",
      NULL,
      "true,false" },

    // Makefile (placeholder)
    { "Makefile", "Makefile.makefile.mk.mak.GNUmakefile",
      "#", NULL, NULL, NULL, 0,
      "ifeq,ifneq,ifdef,ifndef,else,endif,define,endef,include,override,export,unexport,vpath",
      NULL,
      NULL },

    // HTML (placeholder)
    { "HTML", ".html.htm",
      NULL, "<!--", "-->", NULL, 0,
      NULL,
      NULL,
      NULL },

    // CSS (placeholder)
    { "CSS", ".css",
      NULL, "/*", "*/", NULL, 0,
      NULL,
      NULL,
      NULL },

    // JSON (placeholder)
    { "JSON", ".json",
      NULL, NULL, NULL, NULL, 0,
      "true,false,null",
      NULL,
      NULL },

    // Markdown
    { "Markdown", ".md.markdown.mdown.mkd",
      NULL, NULL, NULL, NULL, 0,
      NULL,
      NULL,
      NULL },

    // End marker
    { NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL }
};


//-------------------------------------------------------------------------------------------------
// MarkUp Class
//
// This class contains text routines that insert markup elements into a text string that
// ultimately gets sent to the screen.  It uses program defaults to colorize text according to
// the users wishes
//
//-------------------------------------------------------------------------------------------------
MarkUp::MarkUp( ProgramDefaults *pd, CxScreen *screenPtr )
{
    programDefaults = pd;
    screen          = screenPtr;
    _languageMode   = LANG_NONE;
    _currentSyntax  = NULL;
}


//-------------------------------------------------------------------------------------------------
// MarkUp::setLanguageFromFilePath
//
// Determine the language mode from the file suffix
//
//-------------------------------------------------------------------------------------------------
void
MarkUp::setLanguageFromFilePath( CxString filePath )
{
    _languageMode  = LANG_NONE;
    _currentSyntax = NULL;

    if (filePath.length() == 0) return;

    // Extract suffix from filepath
    int lastDot = filePath.lastChar('.');
    int lastSlash = filePath.lastChar('/');

    CxString suffix;
    CxString filename;

    // Get just the filename for Makefile matching
    if (lastSlash >= 0) {
        filename = filePath.subString(lastSlash + 1, filePath.length() - lastSlash - 1);
    } else {
        filename = filePath;
    }

    // Get suffix if there's a dot after the last slash
    if (lastDot > lastSlash) {
        suffix = filePath.subString(lastDot, filePath.length() - lastDot);
    }

    // Search through language table
    for (int i = 0; languageSyntaxTable[i].name != NULL; i++) {
        const char* suffixes = languageSyntaxTable[i].suffixes;

        // Check if suffix matches (suffixes are like ".c.h.cpp")
        if (suffix.length() > 0 && suffixes != NULL) {
            // Look for suffix in the suffixes string
            const char* p = strstr(suffixes, suffix.data());
            if (p != NULL) {
                // Make sure it's a complete match (next char is '.' or end)
                int suffixLen = suffix.length();
                char nextChar = p[suffixLen];
                if (nextChar == '.' || nextChar == '\0') {
                    _currentSyntax = &languageSyntaxTable[i];
                    _languageMode = (LanguageMode)(i + 1);  // +1 because LANG_NONE is 0
                    return;
                }
            }
        }

        // Check for exact filename match (Makefile)
        if (suffixes != NULL && filename.length() > 0) {
            const char* p = strstr(suffixes, filename.data());
            if (p != NULL) {
                int fnLen = filename.length();
                // Check it's at start or after a dot, and ends at dot or end
                if ((p == suffixes || p[-1] == '.') &&
                    (p[fnLen] == '.' || p[fnLen] == '\0')) {
                    _currentSyntax = &languageSyntaxTable[i];
                    _languageMode = (LanguageMode)(i + 1);
                    return;
                }
            }
        }
    }
}


//-------------------------------------------------------------------------------------------------
// MarkUp::getLanguageMode
//
// Return current language mode
//
//-------------------------------------------------------------------------------------------------
LanguageMode
MarkUp::getLanguageMode( void )
{
    return _languageMode;
}


//-------------------------------------------------------------------------------------------------
// MarkUp::colorizeKeywords
//
// Colorize all matching keywords from a comma-separated list
//
//-------------------------------------------------------------------------------------------------
CxString
MarkUp::colorizeKeywords( CxString line, const char* keywords, CxString colorStart, CxString colorEnd )
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

            // Use existing injection method
            line = injectTextConstantEntryExitText(line, CxString(keyword), colorStart, colorEnd);
        }

        if (*p == ',') p++;
    }

    return line;
}


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

        int isNumber = 0;
        int startPos = i;
        int endPos = i;

        // Check for hex number (0x...)
        if (i + 1 < len && data[i] == '0' && (data[i+1] == 'x' || data[i+1] == 'X')) {
            // Make sure it's not part of an identifier
            if (startPos > 0) {
                char prev = data[startPos - 1];
                if ((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z') || prev == '_') {
                    i++;
                    continue;
                }
            }

            i += 2;
            while (i < len && ((data[i] >= '0' && data[i] <= '9') ||
                               (data[i] >= 'a' && data[i] <= 'f') ||
                               (data[i] >= 'A' && data[i] <= 'F'))) {
                i++;
            }
            if (i > startPos + 2) {
                endPos = i;
                isNumber = 1;
            }
        }
        // Check for decimal/float number
        else if (data[i] >= '0' && data[i] <= '9') {
            // Make sure it's not part of an identifier
            if (startPos > 0) {
                char prev = data[startPos - 1];
                if ((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z') || prev == '_') {
                    i++;
                    continue;
                }
            }

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

            endPos = i;
            isNumber = 1;
        }

        if (isNumber) {
            // Append text before the number
            if (startPos > lastPos) {
                result = result + line.subString(lastPos, startPos - lastPos);
            }
            // Append colorized number
            result = result + colorStart;
            result = result + line.subString(startPos, endPos - startPos);
            result = result + colorEnd;
            lastPos = endPos;
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


//-------------------------------------------------------------------------------------------------
// MarkUp:: colorizeText
//
// Colorize text based on language-specific syntax rules (line-by-line)
//
//-------------------------------------------------------------------------------------------------
CxString
MarkUp::colorizeText( CxString fullText, CxString visibleText )
{
    int isCommentLine = FALSE;
    int isIncludeLine = FALSE;
    int lang = (int) _languageMode;

    CxString resetColor = "\033[0m";

    // get per-language colors with fallback to legacy colors for backward compatibility
    CxString commentColor = programDefaults->commentTextColor( lang )->terminalString();
    if (commentColor.length() == 0) {
        commentColor = programDefaults->commentTextColor()->terminalString();
    }

    CxString includeColor = programDefaults->includeTextColor( lang )->terminalString();
    if (includeColor.length() == 0) {
        includeColor = programDefaults->includeTextColor()->terminalString();
    }

    CxString keywordColor = programDefaults->keywordTextColor( lang )->terminalString();
    if (keywordColor.length() == 0) {
        keywordColor = programDefaults->cppLanguageElementsTextColor()->terminalString();
    }

    CxString typeColor = programDefaults->typeTextColor( lang )->terminalString();
    if (typeColor.length() == 0) {
        typeColor = programDefaults->cppLanguageTypesTextColor()->terminalString();
    }

    CxString methodColor = programDefaults->methodDefinitionTextColor( lang )->terminalString();
    if (methodColor.length() == 0) {
        methodColor = programDefaults->cppLanguageMethodDefinitionTextColor()->terminalString();
    }

    CxString constantColor = programDefaults->constantTextColor( lang )->terminalString();
    CxString stringColor = programDefaults->stringTextColor( lang )->terminalString();
    CxString numberColor = programDefaults->numberTextColor( lang )->terminalString();

    //---------------------------------------------------------------------------------------------
    // Check for line comment and preprocessor
    //---------------------------------------------------------------------------------------------
    CxString testString = fullText.nextToken(" \t\377");

    if (_currentSyntax != NULL && _currentSyntax->lineComment != NULL) {
        if (testString.index(_currentSyntax->lineComment) == 0) {
            isCommentLine = TRUE;
        }
    } else {
        // Default C/C++ behavior
        if (testString.index("//") == 0) {
            isCommentLine = TRUE;
        }
    }

    // Preprocessor directives (C/C++ only, not Markdown headers or Makefile comments)
    if (testString.index("#") == 0 &&
        _languageMode != LANG_MARKDOWN &&
        _languageMode != LANG_MAKEFILE &&
        _languageMode != LANG_PYTHON &&
        _languageMode != LANG_SHELL) {
        isIncludeLine = TRUE;
    }

    //---------------------------------------------------------------------------------------------
    // Colorize full line comment or preprocessor
    //---------------------------------------------------------------------------------------------
    if (isCommentLine) {
        return encapsolateWithEntryExitText(visibleText, commentColor, resetColor);
    }

    if (isIncludeLine) {
        return encapsolateWithEntryExitText(visibleText, includeColor, resetColor);
    }

    //=============================================================================================
    //
    // Colorization Strategy:
    //
    // 1. FIRST: Find all exclusion regions (strings and inline comments) in the original text.
    //    These regions should NOT have their contents colorized for keywords/numbers.
    //
    // 2. Colorize numbers and keywords, but SKIP any matches inside exclusion regions.
    //    This prevents colorizing numbers like "42" in printf("Error 42") or keywords
    //    like "return" in printf("return value").
    //
    // 3. LAST: Apply string and comment colorization to wrap those regions.
    //
    // Numbers must still be colorized before keywords because escape sequences contain
    // numbers (e.g., \033[38;2;200;150;255m contains "38", "2", "200", etc.).
    //
    //=============================================================================================

    //---------------------------------------------------------------------------------------------
    // Find exclusion regions (strings and inline comments) BEFORE any colorization
    //---------------------------------------------------------------------------------------------
    ColorRegions exclusionRegions;
    const char* commentMarker = (_currentSyntax != NULL) ? _currentSyntax->lineComment : "//";
    findExclusionRegions(visibleText, commentMarker, &exclusionRegions);

    //---------------------------------------------------------------------------------------------
    // Colorize numeric literals FIRST (must be before keywords due to escape sequence numbers)
    // Skip numbers inside strings and comments
    //---------------------------------------------------------------------------------------------
    if (numberColor.length()) {
        visibleText = colorizeNumbersWithExclusions(visibleText, numberColor, resetColor, &exclusionRegions);
    }

    //---------------------------------------------------------------------------------------------
    // Colorize keywords based on language, skipping keywords inside strings and comments
    //---------------------------------------------------------------------------------------------
    if (_currentSyntax != NULL) {
        // Use language-specific keywords with exclusion checking
        if (_currentSyntax->types != NULL && typeColor.length()) {
            visibleText = colorizeKeywordsWithExclusions(visibleText, _currentSyntax->types, typeColor, resetColor, &exclusionRegions);
        }
        if (_currentSyntax->keywords != NULL && keywordColor.length()) {
            visibleText = colorizeKeywordsWithExclusions(visibleText, _currentSyntax->keywords, keywordColor, resetColor, &exclusionRegions);
        }
        if (_currentSyntax->constants != NULL) {
            CxString constColor = constantColor.length() ? constantColor : keywordColor;
            if (constColor.length()) {
                visibleText = colorizeKeywordsWithExclusions(visibleText, _currentSyntax->constants, constColor, resetColor, &exclusionRegions);
            }
        }
    } else {
        // Fall back to original C++ hardcoded keywords (without exclusion checking for simplicity)
        if (typeColor.length()) {
            visibleText = injectTextConstantEntryExitText(visibleText, "char", typeColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "void", typeColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "int", typeColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "float", typeColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "double", typeColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "long", typeColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "unsigned", typeColor, resetColor);
        }

        if (keywordColor.length()) {
            visibleText = injectTextConstantEntryExitText(visibleText, "if", keywordColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "while", keywordColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "return", keywordColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "break", keywordColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "case", keywordColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "else", keywordColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "switch", keywordColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "class", keywordColor, resetColor);
            visibleText = injectTextConstantEntryExitText(visibleText, "default", keywordColor, resetColor);
        }
    }

    //---------------------------------------------------------------------------------------------
    // Colorize C++ method definitions (Class::Method)
    //---------------------------------------------------------------------------------------------
    if (methodColor.length() && (_languageMode == LANG_C || _languageMode == LANG_CPP)) {
        visibleText = injectMethodEntryExitText(visibleText, methodColor, resetColor);
    }

    //---------------------------------------------------------------------------------------------
    // Language-specific colorization
    //---------------------------------------------------------------------------------------------

    // Markdown special handling
    if (_languageMode == LANG_MARKDOWN) {
        visibleText = colorizeMarkdown(visibleText, keywordColor, typeColor, stringColor, resetColor);
        return visibleText;
    }

    // Makefile special handling
    if (_languageMode == LANG_MAKEFILE) {
        visibleText = colorizeMakefileSpecial(visibleText, constantColor, methodColor, resetColor);
    }

    // Python decorator handling
    if (_languageMode == LANG_PYTHON) {
        visibleText = colorizePythonDecorators(visibleText, keywordColor, resetColor);
    }

    //---------------------------------------------------------------------------------------------
    // Colorize string literals (same-line only)
    //---------------------------------------------------------------------------------------------
    if (stringColor.length()) {
        visibleText = colorizeStrings(visibleText, stringColor, resetColor);
    }

    //---------------------------------------------------------------------------------------------
    // Colorize inline comments (trailing comments after code)
    //---------------------------------------------------------------------------------------------
    if (_currentSyntax != NULL && _currentSyntax->lineComment != NULL && commentColor.length()) {
        visibleText = colorizeInlineComment(visibleText, _currentSyntax->lineComment, commentColor, resetColor);
    }

    return visibleText;
}

//-------------------------------------------------------------------------------------------------
// MarkUp:: colorizeHelpText
//
// return the string adjusted for color
//
//-------------------------------------------------------------------------------------------------

CxString
MarkUp::colorizeHelpText( CxString fullText, CxString visibleText )
{
    int isCommentLine = false;
    int isIncludeLine = false;
    
    //---------------------------------------------------------------------------------------------
    // get the first token from the full text to see if this is a comment or a include
    //---------------------------------------------------------------------------------------------
    CxString testString = fullText.nextToken(" \t\377");
    if (testString.index("//") == 0) {
        isCommentLine = TRUE;
    }
    
    if (testString.index("#") == 0) {
        isIncludeLine = true;
    }
    
    //---------------------------------------------------------------------------------------------
    // colorize for comments or includes
    //
    //---------------------------------------------------------------------------------------------
    CxString colorizeString = programDefaults->commentTextColor()->terminalString();
    if (isCommentLine) {
        visibleText = encapsolateWithEntryExitText( visibleText,
            programDefaults->commentTextColor()->terminalString(),
            "\033[0m");
        
    }
    
    colorizeString = programDefaults->includeTextColor()->terminalString();
    if (isIncludeLine) {
        
        visibleText = encapsolateWithEntryExitText( visibleText,
            programDefaults->includeTextColor()->terminalString(),
            "\033[0m");
    }
    
    return( visibleText );
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

int MarkUp::parseTextConstant( CxString s, CxString item, unsigned long initialPos, unsigned long *start, unsigned long *end)
{
    
    char *firstCharPtr = s.data();
    char *charPtr = (char *) NULL;

    int c = s.index( item, initialPos );
    if (c==-1) return( FALSE );

    unsigned long startPos = c;
    unsigned long endPos   = c+item.length();

    if (startPos>0) {
        char charBefore = s.data()[startPos-1];
        if ((charBefore != ' '     ) &&
            (charBefore != '\377'  ) &&
            (charBefore != '\t'    ) &&
            (charBefore != ','     ) &&
            (charBefore != ';'     ) &&
            (charBefore != '{'     ) &&
            (charBefore != '}'     ) &&
            (charBefore != ')'     ) &&
            (charBefore != '('     ))
        {
            return( FALSE );
        }
    }

    if (endPos<s.length()-1) {
        char charAfter = s.data()[endPos];
        if ((charAfter != ' '      )  &&
            (charAfter != '\377'   )  &&
            (charAfter != '\t'     )  &&
            (charAfter != ','      )  &&
            (charAfter != ';'      )  &&
            (charAfter != '('      )  &&
            (charAfter != '{'      )  &&
            (charAfter != '}'      )  &&
            (charAfter != ')'      )  &&
            (charAfter != '('      ))
        {
            return( FALSE );
        }
    }

    *start = startPos;
    *end   = endPos;

    return ( TRUE );
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

int MarkUp::parseClassMethod( CxString s, unsigned long *start, unsigned long *end)
{
    char *firstCharPtr = s.data();
    char *charPtr = (char *) NULL;

    unsigned long startPos = 0;
    unsigned long endPos   = 0;

    int c = s.index("::", 0);
    if (c != -1) {

        // get the first position
        charPtr = s.data()+c;
        while ( (charPtr > firstCharPtr-1) &&
                (*charPtr != ' ')          &&
                (*charPtr != '\t')         &&
                (*charPtr != '\377') )
        {
            charPtr--;
        }

        charPtr++;
        startPos = charPtr - firstCharPtr;


        // get the second position
        charPtr = s.data()+c+2;

       // if (*charPtr != (char) NULL) {
            while (
                (*charPtr != '\000')       &&
                (*charPtr != ' ')          &&
                (*charPtr != '\t')         &&
                (*charPtr != '\n')         &&
                (*charPtr != '(')          &&
                (*charPtr != '\377') )
            {
                charPtr++;
            }
        //}

        endPos = charPtr - firstCharPtr;

        if (endPos - startPos == 2) return( FALSE );

        *start = startPos;
        *end   = endPos;

        return( TRUE );
    }

    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

CxString
MarkUp::injectTextConstantEntryExitText( CxString line, CxString item, CxString entryString, CxString exitString)
{
    unsigned long initial = 0;
    unsigned long start = 0;
    unsigned long end = 0;
    CxString result;
    unsigned long lastPos = 0;

    while( parseTextConstant( line, item, initial, &start, &end )) {
        // Append text before this match
        if (start > lastPos) {
            result = result + line.subString(lastPos, start - lastPos);
        }
        // Append colorized match
        result = result + entryString;
        result = result + line.subString(start, end - start);
        result = result + exitString;
        lastPos = end;
        initial = end;
    }

    // Append remaining text after last match
    if (lastPos < (unsigned long)line.length()) {
        result = result + line.subString(lastPos, line.length() - lastPos);
    }

    return (lastPos > 0) ? result : line;
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

CxString
MarkUp::injectMethodEntryExitText( CxString line, CxString entryString, CxString exitString )
{
    unsigned long start;
    unsigned long end;

    if (parseClassMethod( line, &start, &end )) {
        CxString result;
        // Text before method name
        if (start > 0) {
            result = result + line.subString(0, start);
        }
        // Colorized method name
        result = result + entryString;
        result = result + line.subString(start, end - start);
        result = result + exitString;
        // Text after method name
        if (end < (unsigned long)line.length()) {
            result = result + line.subString(end, line.length() - end);
        }
        return result;
    }

    return( line );
}

CxString
MarkUp::encapsolateWithEntryExitText( CxString line, CxString entryString, CxString exitString )
{
    return( entryString + line + exitString );
}


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
        int isNumber = 0;
        int startPos = i;
        int endPos = i;

        // Check for hex number (0x...)
        if (i + 1 < len && data[i] == '0' && (data[i+1] == 'x' || data[i+1] == 'X')) {
            // Make sure it's not part of an identifier
            if (startPos > 0) {
                char prev = data[startPos - 1];
                if ((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z') || prev == '_') {
                    i++;
                    continue;
                }
            }

            i += 2;
            while (i < len && ((data[i] >= '0' && data[i] <= '9') ||
                               (data[i] >= 'a' && data[i] <= 'f') ||
                               (data[i] >= 'A' && data[i] <= 'F'))) {
                i++;
            }
            if (i > startPos + 2) {
                endPos = i;
                isNumber = 1;
            }
        }
        // Check for decimal/float number
        else if (data[i] >= '0' && data[i] <= '9') {
            // Make sure it's not part of an identifier
            if (startPos > 0) {
                char prev = data[startPos - 1];
                if ((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z') || prev == '_') {
                    i++;
                    continue;
                }
            }

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

            endPos = i;
            isNumber = 1;
        }

        if (isNumber && endPos > startPos) {
            // Make sure number isn't followed by identifier char
            if (endPos < len) {
                char next = data[endPos];
                if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || next == '_') {
                    i = endPos;
                    continue;
                }
            }

            // Append text before the number
            if (startPos > lastPos) {
                result = result + line.subString(lastPos, startPos - lastPos);
            }

            // Append colorized number
            result = result + colorStart;
            result = result + line.subString(startPos, endPos - startPos);
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
