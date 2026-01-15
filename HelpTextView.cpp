//-------------------------------------------------------------------------------------------------
//
//  HelpView.cpp
//  cmacs
//
//  Created by Todd Vernon on 9/15/23.
//  Copyright © 2022 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// HelpView.cpp
//
// Edit View the class that handles display of the edit buffer on the screen.  It handles
// screen sizing and resizing and reframing the visible part of the edit buffer on the
// visible screen.
//
//-------------------------------------------------------------------------------------------------

#include "HelpTextView.h"

//-------------------------------------------------------------------------------------------------
// HelpView::HelpView (constructor)
//
// Constructs an overlay help view allowing the user to select a file from the edit buffer
//
//-------------------------------------------------------------------------------------------------
HelpTextView::HelpTextView( ProgramDefaults *pd, Project *proj, CxScreen *screenPtr )
{
    programDefaults = pd;
    screen  = screenPtr;
    project = proj;
    markUp  = new MarkUp( pd, screenPtr );
    
    // initially the help text is not loaded
    eb = new CxEditBuffer();
    
    // setup the resize callback
    screen->addScreenSizeCallback( CxDeferCall( this, &HelpTextView::screenResizeCallback ));
    
    // recalc where everything should disolay
    recalcScreenPlacements();
}


//-------------------------------------------------------------------------------------------------
// HelpView::loadHelp
//
// load the help text into a seperate buffer than the buffer list
//
//-------------------------------------------------------------------------------------------------
int
HelpTextView::loadHelpText(CxString filePath)
{
    filePath = filePath.stripLeading(" \t\r\n");
    filePath = filePath.stripTrailing(" \t\r\n");
    
    // if the file path is actually some text
    if (filePath.length()) {
                                         
        // load the text into it
        eb->loadText( filePath, TRUE );   
        return( TRUE );
    }
    
    recalcScreenPlacements();
    
    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// HelpTextView::screenResizeCallback (callback)
//
// Called with the user resizes the terminal window.  Recalculates key parts of the screen.
//
//-------------------------------------------------------------------------------------------------
void
HelpTextView::screenResizeCallback( void )
{
    // recalculate all the component placements
    recalcScreenPlacements();
}


//-------------------------------------------------------------------------------------------------
// HelpTextView::recalcScreenPlacements
//
// Given a screen window size, this calculates all the rows that are important to
// to the editor.
//
//
//-------------------------------------------------------------------------------------------------
void
HelpTextView::recalcScreenPlacements(void)
{
    // number of lines on the screen
    screenNumberOfLines = screen->rows();

	// number of cols on the screen
    screenNumberOfCols  = screen->cols();
    
    // this calculates the size of the screen area that help will take up
	double maxPossibleListItems = (double) screenNumberOfLines * 0.5;

	if (eb->numberOfLines() < maxPossibleListItems) {
		screenFileListNumberOfLines = eb->numberOfLines();
	} else {
		screenFileListNumberOfLines = maxPossibleListItems;
	}
        
    // the title bar on the file list window
    screenFileListTitleBarLine = screenNumberOfLines - screenFileListNumberOfLines - 3;
    
    // first line of the help text region
    screenFileListFirstListLine = screenFileListTitleBarLine + 1;

	// set the first list index visible in the scrolling list
	firstVisibleListIndex = 0;
    
    // the max first line that can happen to keep text from scrolling up forever
    maxFirstVisibleListIndex = eb->numberOfLines() - screenFileListNumberOfLines;    
}


//-------------------------------------------------------------------------------------------------
// HelpTextView::redraw
//
// Moves the visible window indexes so the cursor is visible and updates the screen
//
//-------------------------------------------------------------------------------------------------
void
HelpTextView::redraw( void )
{
	CxString leftBuffer = "      ";
    CxString pointerText = "> ";
    CxString afterFileNameText = "  ";
    CxString borderText = " ";
    CxString statusBarTextColorCode = programDefaults->statusBarTextColor()->terminalString();
    CxString statusBarBackgroundColorCode = programDefaults->statusBarBackgroundColor()->terminalString();
    CxString statusBarTextColorResetCode = programDefaults->statusBarTextColor()->resetTerminalString();
    CxString statusBarBackgroundColorResetCode = programDefaults->statusBarBackgroundColor()->resetTerminalString();

    //---------------------------------------------------------------------------------------------
    // draw the title bar
    //---------------------------------------------------------------------------------------------
    // set the status bar foreground and background colors
    screen->setForegroundColor(programDefaults->statusBarTextColor() );
    screen->setBackgroundColor(programDefaults->statusBarBackgroundColor() );
    
    CxString header = CxString("-- HELP: ");
    while( header.length() < screenNumberOfCols ) header += CxString("-");
    
    screen->writeTextAt( screenFileListTitleBarLine, 0, header, false );
    screen->resetColors();
    
    CxString helpTextLine;
    
    //---------------------------------------------------------------------------------------------
    // draw the window lines
    //---------------------------------------------------------------------------------------------
    // looping on visible  items
    for (int c=0; c<screenFileListNumberOfLines; c++) {
        
        // get the logical list index that is the first visible item on the screen
        int logicalItem = firstVisibleListIndex + c;
        
        CxString text;
        int realTextLength = 0;
        
		//-----------------------------------------------------------------------------------------        
        // if the logical Item is less than the number of items in the buffer list
		//-----------------------------------------------------------------------------------------
        if (logicalItem < eb->numberOfLines()) {

            helpTextLine = leftBuffer + *(eb->line(logicalItem));
            
            helpTextLine = markUp->colorizeHelpText( helpTextLine, helpTextLine );
            
            screen->writeTextAt( screenFileListFirstListLine + c, 0, helpTextLine, TRUE );
                
        //-----------------------------------------------------------------------------------------
        // draw list items beyond the the list if the window is larger than the list
        //-----------------------------------------------------------------------------------------
        } else {
            
            helpTextLine = leftBuffer;
            realTextLength = 0;
                
            screen->writeTextAt( screenFileListFirstListLine + c, 0, helpTextLine, TRUE );
        }
    }
        
    screen->placeCursor(screenFileListTitleBarLine, 0);

    screen->resetColors();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// HelpTextView::routeKeyAction
//
// Keys passed into routeKeyAction are targeted for the edit view changing the text buffer
// and then the screen is updated to reflect.
//
//-------------------------------------------------------------------------------------------------
void
HelpTextView::routeKeyAction( CxKeyAction keyAction )
{
	//---------------------------------------------------------------------------------------------
	// based on the kind of key
	//
	//---------------------------------------------------------------------------------------------
	switch (keyAction.actionType() )
	{
		//-----------------------------------------------------------------------------------------
		// select another file in the list
		//
		//-----------------------------------------------------------------------------------------
		case CxKeyAction::CURSOR:
			{
                if (handleArrows( keyAction )) {
                    redraw();
                }
                
                screen->flush();
			}
			break;

        //-----------------------------------------------------------------------------------------
        // ignore everything else
        //-----------------------------------------------------------------------------------------
      	default:
        	break;
	}

    return;
}


//-------------------------------------------------------------------------------------------------
// HelpTextView::handleArrows
//
// moves the cursor according to the desired direction and what is valid in the
// edit buffer
//
//-------------------------------------------------------------------------------------------------
int
HelpTextView::handleArrows( CxKeyAction keyAction )
{
 
    if (keyAction.tag() == "<arrow-down>") {
        
        firstVisibleListIndex++;
        if (firstVisibleListIndex > maxFirstVisibleListIndex) firstVisibleListIndex = maxFirstVisibleListIndex;
        
        return( true );

    }

    if (keyAction.tag() == "<arrow-up>") {
        
        firstVisibleListIndex--;
        if (firstVisibleListIndex<0) firstVisibleListIndex=0;

        return( true );
    }

    return(false);
}
