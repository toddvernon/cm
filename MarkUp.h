//-------------------------------------------------------------------------------------------------
//
//  MarkUp.h
//
//  Created by Todd Vernon on 12/30/23.
//  Copyright © 2023 Todd Vernon. All rights reserved.
//
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
//
//
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
    // given a string, colorize it according to the program defaults for C++ attributes

    CxString
    colorizeHelpText( CxString fullText, CxString visibleText );
    // perform some colorization with help text
    
private:
    
    ProgramDefaults *programDefaults;
    // pointer to the startup defaults class

    CxScreen *screen;
    // pointer to the screen object

};

#endif

