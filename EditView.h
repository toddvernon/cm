//-------------------------------------------------------------------------------------------------
//
//  EditView.h
//  cmacs
//
//  Created by Todd Vernon on 6/24/22.
//  Copyright © 2022 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <math.h>

#include <sys/types.h>
#include <iostream>
#include <cx/base/string.h>
#include <cx/base/slist.h>
#include <cx/base/star.h>
#include <cx/base/hashmap.h>

#include <cx/editbuffer/edithint.h>
#include <cx/editbuffer/stringutils.h>

#include "CmTypes.h"
#include <cx/keyboard/keyboard.h>
#include <cx/screen/screen.h>
#include <cx/screen/cursor.h>
#include <cx/functor/defercall.h>

#include "ProgramDefaults.h"
#include "MarkUp.h"


#ifndef _EditView_h_
#define _EditView_h_

//---------------------------------------------------------------------------------------------------------
//
//
//---------------------------------------------------------------------------------------------------------
class EditView
{
  public:

    enum EditStatus {
        OK,
        COMMAND,
        QUIT
    };

    EditView( ProgramDefaults *pd, CxScreen *screen );
    // Constructor

    EditStatus routeKeyAction( CxKeyAction keyAction );
    // route a keyboard action
    
    void reframeAndUpdateScreen(void);
    // recalculate the visible buffer and redraw
    
    void updateScreen(void);
    //  visually redraw the screen

	int findString( CxString findString );
    // find the first occurance of the find string in the buffer past or at the current cursor
    // location.
    
    int findAgain( CxString findString );
    // repeasts the last find command.  If the cursor is on the location of the last successul find
    // it will advance the cursor and do another find to avoid finding the same string over and over
    
    int replaceString( CxString findString, CxString replaceString );
    // replace the current findString with the replaceString.  Note this only replaces if the cursor
    // is located at the begining of the find string in the buffer.  If not it just repeats the last
    // find and leaves the cursor there.  This allows the behavior of moving through the file
    // find-by-find and replacing or not.  If the user wants to replace, they just ctrl-r, the
    // string is replaced, and moves to the next find.  If the user does not want to replace the
    // occurance, then they hit the right arrow and the ctrl-r just finds the next occurance leaving
    // the cursor on it.
    
    int replaceAgain( CxString findString, CxString replaceString );
    
    
    
    CxEditBufferPosition cursorPosition( void );

    void cursorGotoLine( unsigned long row);
    // move cursor to the specified line

	void cursorGotoPosition( CxEditBufferPosition loc );
	// place cursor at the row col position and make visible if need be

    void placeCursor(void);

    void pageDown( void );
    // jump down a page of material

    void pageUp( void );
    // jump up a page of material
    
    CxString cutTextCursorToEndOfLine( void );
	// cut text from the cursor to the end of the current line

    void pasteText( CxString text );
	// paste text from the cut buffer

    void setMark( void );
	// set a mark at the current cursor position	

    CxString cutToMark( void );
	// cut text from the cursor to the mark

    CxString cutTextToEndOfLine(void);
	// cut text from cursor to end of the line    

    CxString currentFilePath(void);
    // get the current edit file path
    
    void setCurrentFilePath( CxString path);
    // set the current edit file path

	void insertCommentBlock( unsigned long lastCol );
    // insert a comment block in the text
    
    CmEditBuffer *getEditBuffer(void);
    // get a pointer to the current edit buffer

    void setEditBuffer( CmEditBuffer *eb);
    // install a new edit buffer into the editview
    
    void toggleLineNumbers( void );
    // toggle line numbers on/off
    
    void toggleJumpScroll( void );
    // turns jump scrolling on and off

#if defined(_LINUX_) || defined(_OSX_)
    void setMcpConnected( int connected );
    // set MCP connection status for status bar display
#endif

  private:

    void screenResizeCallback( void );
    // callback to receive host window size updates

    void recalcScreenPlacements(void);
    // recalcs the key places in the window that parts are placed

    void recalcLineNumberDigits( void );
    // recalcs the number of spaces to use for line numbers based on the largest
    // line number

    int reframe( void );
    // makes sure the cursor is visible and updates the screen if it is not
    
    int reframe_jump( void );
	// makes sure the cursor is visible, it not it will jump the screen to make it visible
	// this is the same as the reframe call but does jump scrolling rather than line by
	// line scrolling.  jump looks better on slower machines

    void recalcVisibleBufferFromTopEditLine(unsigned long newUpperRow);
    // recalculates the visible window position based on a new first visible line

    void recalcVisibleBufferFromBottomEditLine(unsigned long newLowerRow);
    // recalculates the visible window position based a new last visible last line

    void recalcVisibleBufferFromLeft( unsigned long bufferCol );
    // recalculates the visible window position based on a new left column

    void recalcVisibleBufferFromRight( unsigned long bufferCol );
    // recalculates the visible window position based on a new right column

    int  rowVisible(unsigned long row);
    // returns true if a logical buffer line is currently visible

    int colVisible( unsigned long bufferCol );
    // returns true if a logical buffer col in currently visible
    
    //---------------------------------------------------------------------------------------------
    // PRIVATE SCREEN DRAWING
    //
    //---------------------------------------------------------------------------------------------

    void updateStatusLine(void);
    // update the status line

    void updateRemainderOfWindowLine(unsigned long bufferRow, unsigned long bufferCol );
    // updates the remainder of a edit buffer line on the screen past the row, col
    // position passwed into the method, only a single line is changed on the screen

    
    CxString formatEditorLine(unsigned long bufferRow );
    // returns the string that will render the line of text
    
    CxString formatMultipleEditorLines(unsigned long bufferRow, unsigned long bufferCol);
    // updates the remainder of edit buffer window part based on the row, col
    
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
    
    //---------------------------------------------------------------------------------------------

    EditStatus handleArrows( CxKeyAction keyAction );
    // handles arrow key functions

    void updateAfterEdit( CxEditHint& hint, CxString& lineText );
    // common pattern for updating display after an edit operation

    unsigned long bufferRowToScreenRow(unsigned long bufferRow);
    // translates edit buffer row location to screen locations

    unsigned long bufferColToScreenCol(unsigned long bufferCol);
    // translates edit buffer col locations to screen locations

    ProgramDefaults *programDefaults;
    // pointer to the startup defaults class

    CxScreen *screen;
    // pointer to the screen object

    CmEditBuffer *editBuffer;
    // pointer to the edit buffer object
    
    MarkUp *markUp;
    // pointer to the markup class
    
    // these hold the key screen locations (zero based) for the editor all in screen
    // coordinates
    unsigned long _screenNumberOfLines;
    unsigned long _screenNumberOfCols;

    // this is the number of visible edit lines and columns in the edit area
    unsigned long _screenEditNumberOfLines;
	unsigned long _screenEditNumberOfCols;
    unsigned long _screenEditFirstRow;  // index of the first visible edit row (zero based)
    unsigned long _screenEditLastRow;   // index of the last visible edit row (zero based)
    
    // status line row
    unsigned long _screenStatusRow;
    
    // command line row
    unsigned long _screenCommandRow;    // index of the command row (zero based)

    // these hold the windowing values that are used to translate edit buffer coordinates
    // to screen coordinates
    unsigned long _visibleEditBufferOffset;
    
    unsigned long _visibleFirstEditBufferRow;  // index of the first visible edit row
    unsigned long _visibleLastEditBufferRow;   // index of the last visible edit row

    unsigned long _visibleFirstEditBufferCol;
    unsigned long _visibleLastEditBufferCol;

    unsigned long _lineNumberOffset;
    
    int _showLineNumbers;
    // should the editor display line numbers

    int _jumpScroll;
    // sould editor do jump scrolling or smooth scrolling
    
    // a boolean that tells the window its too small to draw itself
    int _windowTooSmall;

#if defined(_LINUX_) || defined(_OSX_)
    int _mcpConnected;
    // is MCP bridge connected
#endif

};

#endif

