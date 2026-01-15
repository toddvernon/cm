//-------------------------------------------------------------------------------------------------
//
//  CommandLineView.h
//  cmacs
//
//  Created by Todd Vernon on 6/24/22.
//  Copyright © 2022 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <signal.h>

#if !defined(_SUNOS_)
#include <sys/ioctl.h>
#endif

#include <sys/types.h>
#include <cx/base/string.h>
#include <cx/base/slist.h>
#include <cx/base/star.h>
#include <cx/base/hashmap.h>

#include <cx/editbuffer/editline.h>
#include <cx/editbuffer/edithint.h>
#include <cx/editbuffer/stringutils.h>
#include <cx/keyboard/keyboard.h>
#include <cx/screen/screen.h>

#include "EditView.h"



#ifndef _CommandLineView_h_
#define _CommandLineView_h_

//-------------------------------------------------------------------------------------------------
//
// The EditorCommandLineView is a container that holds an editLine object that composes a CxString
// using full editing commands.  This is a single line edit field so a CR finishes an edit
// and NUL will cancel the edit.  The screenRow and screenCol parameters represent a real
// screen coordinate section where the edit field resids.  If a prompt is defined that prompt
// fits inside the screen area and the edit area is shrunk enough to fit both
//
// [prompt:[edit area]]
//-------------------------------------------------------------------------------------------------




//-------------------------------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------------------------------
class CommandLineView
{
  public:

    enum AppMode {
        COMMAND,
        EDIT,
        QUIT
    };

    CommandLineView(
        ProgramDefaults *programDefaults,
        CxScreen *screen,
        EditView *editView,
        unsigned long screenRow_,
        unsigned long screenCol_,
        unsigned long maxLength_ );
    // Constructor

	void keyLoop( void );
    // route a keyboard action

    void updateCommandLine(void);
    // update the command line

    void setPrompt(CxString prompt_);
    // use the passed in text as a prompt for the field

    CxString getPrompt(void);
    // get the current prompt text

    void updateScreen( void );
    // updates the entire line of text on the screen based on the edit buffer row

    void routeKeyAction( CxKeyAction keyAction );
	// send a key to the correct place

    CxString getText(void);
	// get the command line text

    void placeCursor(void);
	// place the cursor

    void setText( CxString text_ );
	// set the text part of the command line

    void typeText( CxString text );
	// type the text into the command line

     

  private:

    void calculatePlacements();
    // calculate different screen areas

	void screenResizeCallback( void );
    // callback to receive host window size updates

    void reframe(void);
    // adjusts the content on the line scrolling left or right to make
    // the cursor visible if it is not

    int colVisible( unsigned long bufferCol );
    // returns true if the buffer column is visible on the screen

    void recalcVisibleBufferFromLeft( unsigned long bufferCol );
    // recalculates the visible window based on the column that should be
    // the left most

    void recalcVisibleBufferFromRight( unsigned long bufferCol );
    // recalculates the visible window based on the column that should be
    // the right most

    void handleArrows( CxKeyAction keyAction );
    // handles arrow key functions

    unsigned long bufferColToScreenCol(unsigned long bufferCol);
    // translates edit buffer col locations to screen locations

	CxScreen *_screen;
    // pointer to the screen object

	CxEditLine *_editLine;
    // pointer to the edit buffer object

    void handleControl( CxKeyAction keyAction );
    // handle control keys

    void handleCommand( CxKeyAction keyAction );
	// handle a command line

    void handleEscape( CxKeyAction keyAction );
	// handle the escape key


    CxSList< CxString > findMatchingCommandNames( CxString partialCommand );
    CxString commonBeginningCharacters( CxSList< CxString > list);

    int _commandLineInputDone;

    unsigned long _screenRow;
    unsigned long _screenCol;
    unsigned long _totalWidth;
    unsigned long _editRow;
    unsigned long _editCol;
    unsigned long _editWidth;
    unsigned long _promptScreenRow;
    unsigned long _promptScreenCol;
    unsigned long _promptWidth;
    unsigned long _screenWidth;
    unsigned long _screenHeight;

    unsigned long _firstVisibleEditBufferCol;
    unsigned long _lastVisibleEditBufferCol;

    CxString _prompt;
    
    ProgramDefaults *_programDefaults;

    EditView *_editView;

    CxSList< CxString > _commandList;

    AppMode _mode;
 

};

#endif


