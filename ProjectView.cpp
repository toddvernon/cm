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
// Platform-conditional selection indicator
//-------------------------------------------------------------------------------------------------
#if defined(_LINUX_) || defined(_OSX_)
static const char *SELECTION_INDICATOR = "\xe2\x96\xb6";  // ▶ (UTF-8)
#else
static const char *SELECTION_INDICATOR = ">";
#endif


//-------------------------------------------------------------------------------------------------
// FileListView::FileListView (constructor)
//
// Constructs an overlay file list view allowing the user to select a file from the edit buffer
//
//-------------------------------------------------------------------------------------------------
FileListView::FileListView( ProgramDefaults *pd, CmEditBufferList *ebl, Project *proj, CxScreen *screenPtr )
{
    programDefaults = pd;
    editBufferList  = ebl;
    screen          = screenPtr;
    project         = proj;

    // create the box frame for modal display
    frame = new CxBoxFrame(screen);

    // initially not visible
    _visible = 0;

    // NOTE: No resize callback here - ScreenEditor owns all resize handling

    // recalc where everything should display
    recalcScreenPlacements();
}


//-------------------------------------------------------------------------------------------------
// FileListView::screenResizeCallback (callback)
//
// Called when the user resizes the terminal window.  Recalculates key parts of the screen
// and redraws if the modal is currently visible. The modal draws on top of whatever the
// editors have drawn, so we don't need to clear the screen.
//
//-------------------------------------------------------------------------------------------------
void
FileListView::screenResizeCallback( void )
{
    // recalculate all the component placements
    recalcScreenPlacements();

    // if modal is visible, redraw it on top of the editors
    if (_visible) {
        screen->hideCursor();
        redraw();
    }
}


//-------------------------------------------------------------------------------------------------
// FileListView::recalcScreenPlacements
//
// Calculate centered modal bounds with 15% margins on each side.
// Content height is based on buffer count, clamped to min/max limits.
//
//-------------------------------------------------------------------------------------------------
void
FileListView::recalcScreenPlacements(void)
{
    // get screen dimensions
    screenNumberOfLines = screen->rows();
    screenNumberOfCols  = screen->cols();

    // calculate horizontal margins (15% on each side)
    int marginCols = (int)(screenNumberOfCols * 0.15);
    int frameLeft  = marginCols;
    int frameRight = screenNumberOfCols - marginCols - 1;

    // ensure minimum width
    int frameWidth = frameRight - frameLeft + 1;
    if (frameWidth < 40) {
        frameLeft  = (screenNumberOfCols - 40) / 2;
        frameRight = frameLeft + 39;
    }

    // calculate content height
    // Frame layout: top border + title + separator + content + separator + footer + bottom border = 6 + content
    int minItems = 5;
    int maxItems = (int)(screenNumberOfLines * 0.6) - 6;  // reserve 6 for frame rows
    if (maxItems < minItems) maxItems = minItems;

    int bufferCount = editBufferList->items();
    if (bufferCount < minItems) {
        screenFileListNumberOfLines = minItems;
    } else if (bufferCount > maxItems) {
        screenFileListNumberOfLines = maxItems;
    } else {
        screenFileListNumberOfLines = bufferCount;
    }

    // total height = content lines + 6 (top, title, sep, content..., sep, footer, bottom)
    int totalHeight = screenFileListNumberOfLines + 6;

    // vertical centering
    int frameTop    = (screenNumberOfLines - totalHeight) / 2;
    int frameBottom = frameTop + totalHeight - 1;

    // store column info for redraw
    screenFileListNumberOfCols = frameRight - frameLeft - 1;  // content width

    // update the frame bounds
    frame->resize(frameTop, frameLeft, frameBottom, frameRight);

    // content starts after top border, title, and separator (row + 3)
    screenFileListTitleBarLine  = frameTop + 1;  // title is on row 1
    screenFileListFrameLine     = frameTop + 2;  // separator is on row 2
    screenFileListFirstListLine = frameTop + 3;  // content starts on row 3
    screenFileListLastListLine  = frameBottom - 3;  // before footer separator

    // set the first list index visible in the scrolling list
    firstVisibleListIndex = 0;

    // set the selected item in the list
    selectedListItemIndex = editBufferList->currentItemIndex();
}


int
FileListView::calcLongestName(void)
{
    int maxLength = 0;
    
    // looping on visible  items
    for (int c=0; c<editBufferList->items(); c++) {
        
        // get the edit buffer at that index
        CmEditBuffer *eb = editBufferList->at(c);
        
        // get the file assuming we haven't made a mistake
        CxString filePath = eb->getFilePath( );
        
        if (filePath.length() > maxLength) maxLength = filePath.length();
    }
    
    return( maxLength );
}


//-------------------------------------------------------------------------------------------------
// FileListView::redraw
//
// Draw centered modal dialog with box frame, title, content, and footer.
//
//-------------------------------------------------------------------------------------------------
void
FileListView::redraw( void )
{
    int cursorRow = 0;

    reframe();

    // get frame content bounds
    int contentLeft  = frame->contentLeft();
    int contentRight = frame->contentRight();
    int contentWidth = frame->contentWidth();

    //---------------------------------------------------------------------------------------------
    // set frame colors and draw the frame with title and footer
    //---------------------------------------------------------------------------------------------
    frame->setFrameColor(programDefaults->statusBarTextColor(),
                         programDefaults->statusBarBackgroundColor());

    // build title string: Project: <name>
    CxString title = CxString("Project: ") + project->projectName();

    // build footer string with keyboard hints
    CxString footer = CxString("[Enter] Load   [S] Save   [A] Save All   [Esc] Cancel");

    frame->drawWithTitleAndFooter(title, footer);

    //---------------------------------------------------------------------------------------------
    // draw the file list content
    //---------------------------------------------------------------------------------------------
    for (int c = 0; c < screenFileListNumberOfLines; c++) {

        // get the logical list index
        int logicalItem = firstVisibleListIndex + c;
        int row = screenFileListFirstListLine + c;

        // position cursor at start of content area
        screen->placeCursor(row, contentLeft);

        // if this item exists in the buffer list
        if (logicalItem < editBufferList->items()) {

            CmEditBuffer *eb = editBufferList->at(logicalItem);

            // build status text
            CxString statusText = "";
            if (eb->isTouched()) {
                statusText = "/modified";
            } else if (eb->isInMemory()) {
                statusText = "/in-memory";
            }

            // get file path
            CxString filePath = eb->getFilePath();

            // Layout: " X path...padding...status "
            // where X is indicator (▶) for selected, space for unselected
            // Total display width = contentWidth
            // Indicator area = 3 columns (" X ")
            // Status area = status length + 1 trailing space
            // Path area = remaining space

            int indicatorDisplayLen = 3;  // " X " display columns
            int statusDisplayLen = statusText.length() + 1;  // status + trailing space
            int pathAreaLen = contentWidth - indicatorDisplayLen - statusDisplayLen;

            // truncate path if needed
            if ((int)filePath.length() > pathAreaLen) {
                filePath = filePath.subString(0, pathAreaLen - 3);
                filePath += "...";
            }

            // pad path to fill path area
            while ((int)filePath.length() < pathAreaLen) {
                filePath += " ";
            }

            //-------------------------------------------------------------------------------------
            // draw selected item
            //-------------------------------------------------------------------------------------
            if (selectedListItemIndex == logicalItem) {

                // set selection colors
                screen->setForegroundColor(programDefaults->statusBarTextColor());
                screen->setBackgroundColor(programDefaults->statusBarBackgroundColor());

                // build the line: " ▶ " + padded_path + status + " "
                CxString line = " ";
                line += SELECTION_INDICATOR;
                line += " ";
                line += filePath;
                line += statusText;
                line += " ";

                screen->writeText(line);
                screen->resetColors();

                cursorRow = row;

            //-------------------------------------------------------------------------------------
            // draw unselected item
            //-------------------------------------------------------------------------------------
            } else {

                // use modal content colors (dark background for overlay effect)
                screen->setForegroundColor(programDefaults->modalContentTextColor());
                screen->setBackgroundColor(programDefaults->modalContentBackgroundColor());

                // build the line: "   " + padded_path + status + " "
                CxString line = "   ";
                line += filePath;
                line += statusText;
                line += " ";

                screen->writeText(line);
                screen->resetColors();
            }

        //-----------------------------------------------------------------------------------------
        // draw empty line if beyond buffer list
        //-----------------------------------------------------------------------------------------
        } else {
            // fill empty lines with modal background
            screen->setForegroundColor(programDefaults->modalContentTextColor());
            screen->setBackgroundColor(programDefaults->modalContentBackgroundColor());
            CxString emptyLine;
            for (int i = 0; i < contentWidth; i++) {
                emptyLine += " ";
            }
            screen->writeText(emptyLine);
            screen->resetColors();
        }
    }

    screen->placeCursor(cursorRow, contentLeft);
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
    CmEditBuffer *eb = editBufferList->at( selectedListItemIndex );

    // get the file assuming we haven't made a mistake
    CxString filePath = eb->getFilePath( );

    // return the name of the path
    return(filePath);
}


//-------------------------------------------------------------------------------------------------
// FileListView::getSelectedBuffer
//
// return the edit buffer for the currently selected item (for save operations)
//
//-------------------------------------------------------------------------------------------------
CmEditBuffer *
FileListView::getSelectedBuffer( void )
{
    return editBufferList->at( selectedListItemIndex );
}


//-------------------------------------------------------------------------------------------------
// FileListView::setVisible
//
// Set the visibility state for resize handling
//
//-------------------------------------------------------------------------------------------------
void
FileListView::setVisible( int visible )
{
    _visible = visible;
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
