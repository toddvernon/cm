//-------------------------------------------------------------------------------------------------
//
//  ProgramDefaults.cpp
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

#include "CommandLineView.h"

//---------------------------------------------------------------------------------------------------------
// CommandLineView::CommandLineView (constructor)
//
// Constructs an edit window object and registers a callback with the passed in screen
// object so it knows of terminal window size changes.
//
//---------------------------------------------------------------------------------------------------------

CommandLineView::CommandLineView(
    ProgramDefaults *programDefaults,
    CxScreen *screen,
    EditView *editView,
    unsigned long screenRow_,               // zero based
    unsigned long screenCol_,               // zero based
    unsigned long length_ )
{
    _programDefaults  = programDefaults;
	_screen           = screen;
    _editView         = editView;
    
    _screenRow        = _screen->rows()-1;
    _screenCol        = 1;
    _totalWidth       = _screen->cols()-1;
    
    _editWidth        = 0;
    _promptWidth      = 0;
    _prompt           = "";
    _screenWidth      = CxScreen::cols();
    _screenHeight     = CxScreen::rows();
    _editRow          = 0;
    _editCol          = 0;
    _firstVisibleEditBufferCol  = 0;        // in buffer coordinates
    _lastVisibleEditBufferCol   = 0;        // in buffer coordinates

    _commandLineInputDone = true;

    // setup the resize callback
	_screen->addScreenSizeCallback( CxDeferCall( this, &CommandLineView::screenResizeCallback ));

    // calculate where the parts go
    calculatePlacements();

    // create the edit line buffer
    _editLine  = new CxEditLine( _editWidth );

    // update the screen
    updateScreen();

    _mode = EDIT;
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::screenResizeCallback (callback)
//
// Called with the user resizes the terminal window.  Recalculates key parts of the screen.
//
//---------------------------------------------------------------------------------------------------------
void
CommandLineView::screenResizeCallback( void )
{
    _screenWidth  = CxScreen::cols();
    _screenHeight = CxScreen::rows();
    
    _screenRow    = _screen->rows()-1;
    _screenCol    = 0;
    
    _totalWidth   = _screenWidth -1;

    calculatePlacements();
    
    updateScreen();
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::screenResizeCallback (callback)
//
// Called with the user resizes the terminal window.  Recalculates key parts of the screen.
//
//---------------------------------------------------------------------------------------------------------
void
CommandLineView::calculatePlacements()
{
    // where is the prompt located on the physical screen
    _promptScreenRow = _screenRow;
    _promptScreenCol = _screenCol;

    // the width of the prompt
    _promptWidth = _prompt.length();

    // the width of the editable area
    _editWidth  = _totalWidth - _promptWidth;

    // where is the editable part located on the physical screen
    _editRow = _screenRow;
    _editCol = _promptScreenCol + _promptWidth;

    // the last col that is visible in the scrolling area
    _lastVisibleEditBufferCol  = _firstVisibleEditBufferCol + _editWidth;
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::setPrompt
//
// Called with the user resizes the terminal window.  Recalculates key parts of the screen.
//
//---------------------------------------------------------------------------------------------------------
void
CommandLineView::setPrompt(CxString prompt_ )
{
    // set the new prompt text
    _prompt = prompt_;

    // recalc the areas of the screen
    calculatePlacements();

    // update the display
    updateScreen();
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::getPrompt
//
//
//---------------------------------------------------------------------------------------------------------
CxString
CommandLineView::getPrompt( void )
{
    return( _prompt );
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::setText
//
//
//
//---------------------------------------------------------------------------------------------------------
void
CommandLineView::setText(CxString text)
{
    _editLine->setText( text );
    reframe();
    updateScreen();
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::typeText
//
// 
//
//---------------------------------------------------------------------------------------------------------
void CommandLineView::typeText( CxString text )
{
    for (unsigned long i=0; i<text.length(); i++) {

        // add the character to the buffer
        CxEditHint editHint = _editLine->addCharacter( text.charAt(i) );

        // first check if a simple update of the last part of the line will do
        if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE_PAST_POINT ) {
            reframe();
            updateScreen();
        }

        // first check if a simple update of the last part of the line will do
        if (editHint.updateHint() == CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT ) {
            reframe();
            updateScreen();
        }
    }
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::findMatchingCommandNames
//
//---------------------------------------------------------------------------------------------------------
CxSList< CxString >
CommandLineView::findMatchingCommandNames( CxString partialCommand )
{
    CxSList< CxString > keeperList;
    CxMatchTemplate temp(partialCommand+CxString("*"));

    for( int i=0; i<_commandList.entries(); i++) {
        CxString command = _commandList.at(i);
        if (temp.test(command)) {
            keeperList.append(command);
        }
    }

    return(keeperList);
}


//---------------------------------------------------------------------------------------------------------
// commandBeginningCharacters
//
// returns the leading string that is common to all the strings in the
// list
//
//---------------------------------------------------------------------------------------------------------
CxString
CommandLineView::commonBeginningCharacters( CxSList< CxString > list)
{
    int shortest = 1000000;

    if (list.entries()==0) {
        return("");
    }

    // find the shortest string in the group
    for (int c=0; c<list.entries(); c++) {
        if (shortest > list.at(c).length()) {
            shortest = list.at(c).length();
        }
    }

    if (shortest==0) {
        return("");
    }

    for (int charPosition=0; charPosition<shortest; charPosition++) {
        char matchChar = list.at(0).charAt(charPosition);
        for (int c=1; c<list.entries(); c++) {
            if (matchChar != list.at(c).charAt(charPosition)) {
                return( list.at(0).subString(0, charPosition));
            }
        }
    }
    return( list.at(0).subString(0, shortest));
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::bufferColToScreenCol
//
// transforms a editbuffer index into a screen index (zero based)
//
//---------------------------------------------------------------------------------------------------------
unsigned long
CommandLineView::bufferColToScreenCol(unsigned long bufferCol)
{
    // get the physical screen line (zero based)
    unsigned long screenCol = _editCol + bufferCol - _firstVisibleEditBufferCol;
    return(screenCol);
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::updateScreen
//
// updates the screen to look like the visible part of the buffer
//
//---------------------------------------------------------------------------------------------------------
void
CommandLineView::updateScreen( void )
{
    _screen->setForegroundColor( _programDefaults->commandLineMessageTextColor() );
    
    // write the prompt text
    _screen->writeTextAt(
            _promptScreenRow,
            _promptScreenCol,
            _prompt.data(),
            true
    );
    
    _screen->resetColors();

    // write the edit text
    CxString text = _editLine->text();

    // get the part of the text item to display beginning
    text = text.subString(_firstVisibleEditBufferCol, text.length() );

    // now the string might be too long or too short for the window
    // so this limits it to the at most the width of the window
    text = text.subString(0, _editWidth);

    // it could be too short however, so this pads it to the width of
    // the window if it is
    _screen->writeTextAt(
            _editRow,
            _editCol,
            text,
            true
    );

    _screen->resetForegroundColor();

    // put the cursor back
    _screen->placeCursor(
        _editRow + 1,
        bufferColToScreenCol(_editLine->cursorCol())
    );

    _screen->flush();

}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::placeCursor
//
//
//
//---------------------------------------------------------------------------------------------------------

void
CommandLineView::placeCursor(void)
{
    // put the cursor back
    _screen->placeCursor(
        _editRow + 1,
        bufferColToScreenCol(_editLine->cursorCol())
    );
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::getText
//
// return the edit buffer as one line string
//
//---------------------------------------------------------------------------------------------------------
CxString
CommandLineView::getText(void)
{
    return( _editLine->text());
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::reframe
//
// make sure the cursor is still visible, if not then scroll content and refresh the
// window
//
//---------------------------------------------------------------------------------------------------------
void
CommandLineView::reframe(void)
{
    unsigned long bufferCol = _editLine->cursorCol();

    // if cursor is visible, nothing to do
    if (colVisible(bufferCol)) {
        return;
    }

    // if cursor col is before the first edit col, move
    // window left so its visible
    if (bufferCol < _firstVisibleEditBufferCol) {

        // move window down
        recalcVisibleBufferFromLeft( bufferCol );

        // update everything
        updateScreen();

        return;
    }

    // if cursor row is below the last edit row, move the
    // visible window down so the line is the last edit line
    if (bufferCol > _lastVisibleEditBufferCol) {

        // move visible window so cursor is visible
        recalcVisibleBufferFromRight( bufferCol );

        // update everything
        updateScreen();

        return;
    }

    // this is a should never happen situation
    printf("error: fell through reframe");
    exit(0);
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::colVisible
//
// return true if the the requested col is visible
//
//
//---------------------------------------------------------------------------------------------------------
int
CommandLineView::colVisible( unsigned long bufferCol )
{
    if ((bufferCol >= _firstVisibleEditBufferCol) &&
        (bufferCol <= _lastVisibleEditBufferCol)) {
        return( true );
    }

    return( false );
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::recalcVisibleBufferFromLeft
//
// return true if the the requested col is visible
//
//
//---------------------------------------------------------------------------------------------------------
void
CommandLineView::recalcVisibleBufferFromLeft( unsigned long bufferCol )
{
    _firstVisibleEditBufferCol = bufferCol;
    _lastVisibleEditBufferCol  = _firstVisibleEditBufferCol + _editWidth;
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::recalcVisibleBufferFromRight
//
// return true if the the requested col is visible
//
//
//---------------------------------------------------------------------------------------------------------
void
CommandLineView::recalcVisibleBufferFromRight( unsigned long bufferCol )
{
    _lastVisibleEditBufferCol = bufferCol;
    _firstVisibleEditBufferCol = _lastVisibleEditBufferCol - _editWidth;
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::handleArrows
//
// moves the cursor according to the desired direction and what is valid in the
// edit buffer
//
//
//---------------------------------------------------------------------------------------------------------
void
CommandLineView::handleArrows( CxKeyAction keyAction )
{
	//-----------------------------------------------------------------------------------------------------
	// handle left arrow action
	//
	//-----------------------------------------------------------------------------------------------------
	if (keyAction.tag() == "<arrow-left>") {

        CxEditHint hint = _editLine->cursorLeftRequest();
        reframe();
	}

	//-----------------------------------------------------------------------------------------------------
	// handle right arrow action
	//
	//-----------------------------------------------------------------------------------------------------
	if (keyAction.tag() == "<arrow-right>") {
		CxEditHint hint = _editLine->cursorRightRequest();
        reframe();
	}

	//-----------------------------------------------------------------------------------------------------
	// handle down arrow action
	//
	//-----------------------------------------------------------------------------------------------------
	if (keyAction.tag() == "<arrow-down>") {
		CxEditHint hint = _editLine->cursorDownRequest();
	}

	//-----------------------------------------------------------------------------------------------------
	// handle up arrow action
	//
	//-----------------------------------------------------------------------------------------------------
	if (keyAction.tag() == "<arrow-up>") {
		CxEditHint hint = _editLine->cursorUpRequest();
	}

    return;
}


//---------------------------------------------------------------------------------------------------------
// CommandLineView::routeKeyAction
//
// Handles key actions sent to the command line processors
//
//---------------------------------------------------------------------------------------------------------
void
CommandLineView::routeKeyAction( CxKeyAction keyAction )
{

	//-----------------------------------------------------------------------------------------------------
	// do something based on action type
	//
	//-----------------------------------------------------------------------------------------------------
    switch (keyAction.actionType() )
    {

		//-------------------------------------------------------------------------------------------------
		// if a cursor type action pass to arrows handlers
		//
		//-------------------------------------------------------------------------------------------------
        case CxKeyAction::CURSOR:
            {
                handleArrows( keyAction );
                updateScreen();
            }
            break;

		//-------------------------------------------------------------------------------------------------
		// normal input characters
		//
		//-------------------------------------------------------------------------------------------------
        case CxKeyAction::LOWERCASE_ALPHA:
        case CxKeyAction::UPPERCASE_ALPHA:
        case CxKeyAction::NUMBER:
        case CxKeyAction::SYMBOL:
            {

                // add the character to the buffer
                CxEditHint editHint = _editLine->addCharacter( keyAction.tag() );

                // first check if a simple update of the last part of the line will do
                if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE_PAST_POINT ) {

                    updateScreen();
                    reframe();
                }

                // first check if a simple update of the last part of the line will do
                if (editHint.updateHint() == CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT ) {

                    updateScreen();
                    reframe();
                }

            }
            break;

		//-------------------------------------------------------------------------------------------------
		// do backspace action
		//
		//-------------------------------------------------------------------------------------------------
        case CxKeyAction::BACKSPACE:
            {

                CxEditHint editHint = _editLine->addBackspace();

                if (editHint.updateHint() == CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT ) {
                    updateScreen();
                    reframe();
                }

                if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE_PAST_POINT ) {
                    updateScreen();
                    reframe();
                }

                if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE) {
                    updateScreen();
                    reframe();
                }

            }
            break;

		//-------------------------------------------------------------------------------------------------
		// handle the tab action
		//
		//-------------------------------------------------------------------------------------------------
        case CxKeyAction::TAB:
            {
                CxEditHint editHint = _editLine->addTab();

                if (editHint.updateHint() == CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT ) {
                    updateScreen();
                }

                if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE_PAST_POINT ) {
                    updateScreen();
                }

            }
            break;

        default:
            break;
    }

}

