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
}


//-------------------------------------------------------------------------------------------------
// MarkUp:: colorizeText
//
// return the string adjusted for color
//
//-------------------------------------------------------------------------------------------------
/*static*/
CxString
MarkUp::colorizeText( CxString fullText, CxString visibleText )
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
    
    
    //---------------------------------------------------------------------------------------------
    // colorize language cpp language types
    //
    //---------------------------------------------------------------------------------------------
    colorizeString = programDefaults->cppLanguageTypesTextColor()->terminalString();
    if (colorizeString.length() && (!isCommentLine)) {
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "char",
            programDefaults->cppLanguageTypesTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "void",
            programDefaults->cppLanguageTypesTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "int",
            programDefaults->cppLanguageTypesTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "float",
            programDefaults->cppLanguageTypesTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "double",
            programDefaults->cppLanguageTypesTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "long",
            programDefaults->cppLanguageTypesTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "unsigned",
            programDefaults->cppLanguageTypesTextColor()->terminalString(),
            "\033[0m");
    }
    
    //---------------------------------------------------------------------------------------------
    // colorize language cpp language elements
    //
    //---------------------------------------------------------------------------------------------
    colorizeString = programDefaults->cppLanguageElementsTextColor()->terminalString();
    if (colorizeString.length() && (!isCommentLine)) {
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "if",
            programDefaults->cppLanguageElementsTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "while",
            programDefaults->cppLanguageElementsTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "return",
            programDefaults->cppLanguageElementsTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "break",
            programDefaults->cppLanguageElementsTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "case",
            programDefaults->cppLanguageElementsTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "else",
            programDefaults->cppLanguageElementsTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "switch",
            programDefaults->cppLanguageElementsTextColor()->terminalString(),
            "\033[0m");
        
        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "class",
            programDefaults->cppLanguageElementsTextColor()->terminalString(),
            "\033[0m");

        visibleText = injectTextConstantEntryExitText(
            visibleText,
            "default",
            programDefaults->cppLanguageElementsTextColor()->terminalString(),
            "\033[0m");
    }
    
    
    //---------------------------------------------------------------------------------------------
    // colorize language cpp language method definitions
    //
    //---------------------------------------------------------------------------------------------
    colorizeString = programDefaults->cppLanguageMethodDefinitionTextColor()->terminalString();
    if (colorizeString.length()) {
        
        visibleText = injectMethodEntryExitText(
            visibleText,
            programDefaults->cppLanguageMethodDefinitionTextColor()->terminalString(),
            "\033[0m");
    }
    
    return( visibleText );
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

    while( parseTextConstant( line, item, initial, &start, &end )) {
        line.insert( entryString, start);
        end = end + entryString.length();
        line.insert( exitString, end );
        initial = start + item.length() + entryString.length() + exitString.length();
    }

    return(line);
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

CxString
MarkUp::injectMethodEntryExitText( CxString line, CxString entryString, CxString exitString )
{
    unsigned long start;
    unsigned long end;

    if (parseClassMethod( line, &start, &end )) {

        line.insert( entryString, start);
        end = end + entryString.length();
        line.insert( exitString, end);
    }

    return( line );
}

CxString
MarkUp::encapsolateWithEntryExitText( CxString line, CxString entryString, CxString exitString )
{
    return( entryString + line + exitString );
}
