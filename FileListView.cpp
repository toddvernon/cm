//-------------------------------------------------------------------------------------------------
//
//  FileListView.cpp
//  cmacs
//
//  Created by Todd Vernon on 9/15/23.
//  Copyright    2022 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------




//-------------------------------------------------------------------------------------------------
// FileListView.cpp
//
// Edit View the class that handles display of the edit buffer on the screen.  It handles
// screen sizing and resizing and reframing the visible part of the edit buffer on the
// visible screen.
//
//-------------------------------------------------------------------------------------------------

#include "FileListView.h"

//-------------------------------------------------------------------------------------------------
// FileListView::FileListView (constructor)
//
// Constructs an overlay file list view allowing the user to select a file from the edit buffer
//
//-------------------------------------------------------------------------------------------------
FileListView::FileListView( ProgramDefaults *pd, CxEditBufferList *ebl, Project *proj, CxScreen *screenPtr )
{
    programDefaults = pd;
    editBufferList  = ebl;
    screen          = screenPtr;
    project         = proj;
    
    // setup the resize callback
    screen->addScreenSizeCallback( CxDeferCall( this, &FileListView::screenResizeCallback ));
    
    // recalc where everything should disolay
    recalcScreenPlacements();
}


//-------------------------------------------------------------------------------------------------
// FileListView::screenResizeCallback (callback)
//
// Called with the user resizes the terminal window.  Recalculates key parts of the screen.
//
//-------------------------------------------------------------------------------------------------
void
FileListView::screenResizeCallback( void )
{
    // recalculate all the component placements
    recalcScreenPlacements();
}


//-------------------------------------------------------------------------------------------------
// FileListView::recalcScreenPlacements
//
// Given a screen window size, this calculates all the rows that are important to
// to the editor.
//
//
//-------------------------------------------------------------------------------------------------
void
FileListView::recalcScreenPlacements(void)
{
    // number of lines on the screen
    screenNumberOfLines = screen->rows();

	// number of cols on the screen
    screenNumberOfCols  = screen->cols();

	double maxPossibleListItems = (double) screenNumberOfLines * 0.5;

	if (editBufferList->items() < maxPossibleListItems) {
		screenFileListNumberOfLines = editBufferList->items();
	} else {
		screenFileListNumberOfLines = maxPossibleListItems;
	}
    
    // number of line to show, start with all of them
    //screenFileListNumberOfLines = 10;
        
    // the title bar on the file list window
    screenFileListTitleBarLine = screenNumberOfLines - screenFileListNumberOfLines - 3;
    
    screenFileListFirstListLine = screenFileListTitleBarLine +1;

	// set the first list index visible in the scrolling list
	firstVisibleListIndex = 0;
    
    // set the selected item in the list, zero by default
    selectedListItemIndex = editBufferList->currentItemIndex();
}


int
FileListView::calcLongestName(void)
{
    int maxLength = 0;
    
    // looping on visible  items
    for (int c=0; c<editBufferList->items(); c++) {
        
        // get the edit buffer at that index
        CxEditBuffer *eb = editBufferList->at(c);
        
        // get the file assuming we haven't made a mistake
        CxString filePath = eb->getFilePath( );
        
        if (filePath.length() > maxLength) maxLength = filePath.length();
    }
    
    return( maxLength );
}


//-------------------------------------------------------------------------------------------------
// FileListView::redraw
//
// Moves the visible window indexes so the cursor is visible and updates the screen
//
//-------------------------------------------------------------------------------------------------
void
FileListView::redraw( void )
{
    int cursorRow = 0;
    int longestName = calcLongestName();
    
    reframe();
    
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
    
    CxString header = CxString("-- PROJECT: ") + project->projectName() + CxString(" ");
    while( header.length() < screenNumberOfCols ) header += CxString("-");
    
    screen->writeTextAt( screenFileListTitleBarLine, 0, header, false );
    screen->resetColors();
    
    //---------------------------------------------------------------------------------------------
    // draw the window lines
    //---------------------------------------------------------------------------------------------
    // looping on visible  items
    for (int c=0; c<screenFileListNumberOfLines; c++) {
        
        // get the logical list index that is the first visible item on the screen
        int logicalItem = firstVisibleListIndex + c;
        
        //CxColor resetColor;
        CxString text;
        int realTextLength = 0;
        
        
        // if the logical Item is less than the number of items in the buffer list
        if (logicalItem < editBufferList->items()) {
            
            // get the edit buffer at that index
            CxEditBuffer *eb = editBufferList->at(logicalItem);
            
            CxString touchedText  = "";
            CxString inMemoryText = "";
            
            if (eb->isTouched()) {
                touchedText = " /text-modified";
            }
            
            if (eb->isInMemory()) {
                inMemoryText = " /in-memory";
            }

			CxString postFixText = touchedText + inMemoryText;


            // get the file path for this edit buffer
            CxString filePath = eb->getFilePath( );
            
            // debug
            //filePath = filePath + CxString("  "); // + CxString( logicalItem );
            
            while( filePath.length() < longestName) {
                filePath += " ";
            }
            
            // buffer
            filePath += "     ";
            
            
            //-------------------------------------------------------------------------------------
            // draw a selected list item
            //-------------------------------------------------------------------------------------
            if (selectedListItemIndex == logicalItem) {
                
                text + CxString("");
                realTextLength = 0;
                
                text += statusBarTextColorCode + statusBarBackgroundColorCode;
            
                // add the file pointer text
                text += pointerText; realTextLength += pointerText.length();
                
                // add the file path string
                text += filePath; realTextLength += filePath.length();
                
                // add the highlight padding after the file name
                text += afterFileNameText; realTextLength += afterFileNameText.length();
                 
                text += statusBarTextColorResetCode + statusBarBackgroundColorResetCode;
                
                
                
                text += postFixText; realTextLength += postFixText.length();
                
                // write out the string
                screen->writeTextAt( screenFileListFirstListLine + c, 0, text, TRUE );
                
                cursorRow = screenFileListFirstListLine + c;

            //-------------------------------------------------------------------------------------
            // draw an unselected list item
            //-------------------------------------------------------------------------------------
            } else {
                
                text + CxString("");
                realTextLength = 0;
                
                // add space text before filename
                text += statusBarTextColorResetCode + statusBarBackgroundColorResetCode;
                text += CxString("  "); realTextLength += 2;

                // add the file path string
                text += filePath; realTextLength += filePath.length();
                
                // add the highlight padding after the file name
                text += afterFileNameText; realTextLength += afterFileNameText.length();
                
                text += postFixText; realTextLength += postFixText.length();
                
                screen->writeTextAt( screenFileListFirstListLine + c, 0, text, TRUE );
                
            }
            
        //-----------------------------------------------------------------------------------------
        // draw list items beyond the the list if the window is larger than the list
        //-----------------------------------------------------------------------------------------
        } else {
            
            text + CxString("");
            realTextLength = 0;
                
            screen->writeTextAt( screenFileListFirstListLine + c, 0, text, TRUE );
        }
        
    }
        
    screen->placeCursor(cursorRow,0);
 
    screen->resetColors();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// FileListView::getSelectedItem
//
// return the path of the currently selected item
//
//-------------------------------------------------------------------------------------------------
CxString
FileListView::getSelectedItem( void )
{
    // get the edit buffer at that index
    CxEditBuffer *eb = editBufferList->at( selectedListItemIndex );
    
    // get the file assuming we haven't made a mistake
    CxString filePath = eb->getFilePath( );
    
    // return the name of the path
    return(filePath);
}


//-------------------------------------------------------------------------------------------------
// FileListView::routeKeyAction
//	
// Keys passed into routeKeyAction are targeted for the edit view changing the text buffer
// and then the screen is updated to reflect.
//
//-------------------------------------------------------------------------------------------------
void
FileListView::routeKeyAction( CxKeyAction keyAction )
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
// FileListView::selectedItemVisible
//
// if selected item isn't visible move the text in the visible window
//
//-------------------------------------------------------------------------------------------------
int
FileListView::reframe( )
{
    int changeMade = false;
    
    while ( selectedListItemIndex < firstVisibleListIndex ) {
        changeMade = true;
        firstVisibleListIndex--;
    }
    
    while (selectedListItemIndex >= firstVisibleListIndex + screenFileListNumberOfLines) {
        changeMade = true;
        firstVisibleListIndex++;
    }
    
    if (changeMade) return(true);
    
    return(false);
}


//-------------------------------------------------------------------------------------------------
// FileListView::handleArrows
//
// moves the cursor according to the desired direction and what is valid in the
// edit buffer
//
//-------------------------------------------------------------------------------------------------
int
FileListView::handleArrows( CxKeyAction keyAction )
{
    if (keyAction.tag() == "<arrow-down>") {
        
        selectedListItemIndex++;
        
        if (selectedListItemIndex >= editBufferList->items()) {
            selectedListItemIndex--;
        }
    
        return( true );

    }

    if (keyAction.tag() == "<arrow-up>") {
        
        selectedListItemIndex--;
        
        if (selectedListItemIndex < 0) {
            selectedListItemIndex = 0;
        }
        
        return( true );
    }

    return(false);
}
