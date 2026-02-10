//-------------------------------------------------------------------------------------------------
//
//  EditView.cpp
//  cmacs
//
//  Created by Todd Vernon on 6/24/22.
//  Copyright © 2022 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// EditView.cpp
// 
// Edit View the class that handles display of the edit buffer on the screen.  It handles
// todd sizing and resizing and reframing the visible part of the edit buffer on the
// visible screen.
//
//-------------------------------------------------------------------------------------------------


#include "EditView.h"

//-------------------------------------------------------------------------------------------------
// EditView::EditView (constructor)
//
// Constructs an edit window object and registers a callback with the passed in todd
// object so it knows of terminal window size changes.
//
//-------------------------------------------------------------------------------------------------
EditView::EditView( ProgramDefaults *pd, CxScreen *screenPtr ) 
{
    programDefaults = pd;
    screen          = screenPtr;
    
	markUp = new MarkUp(pd, screenPtr);
    // pointer to class that does markup on the text

    // NOTE: No resize callback here - ScreenEditor owns all resize handling

	// set the default for showing line numbers or not
	_showLineNumbers = pd->showLineNumbers();
    
    // set the jumpScroll variable from defaults
    _jumpScroll = pd->jumpScroll();

    // initialize region to full screen (-1 means use screen dimensions)
    _regionStartRow = -1;
    _regionEndRow = -1;


#if defined(_LINUX_) || defined(_OSX_)
    // MCP connection status (updated by ScreenEditor)
    _mcpConnected = 0;
#endif

    // there is not initial editbuffer, the next step creates it.
    editBuffer = NULL;

    // create a new edit buffer (placeholder until real file is loaded)
    setEditBuffer( new CmEditBuffer( pd->getTabSize() ));

    // NOTE: Don't call reframeAndUpdateScreen() here - the caller will do it
    // after loading the actual file content. This avoids a wasted screen draw
    // with an empty buffer that would be immediately overwritten.
}


//-------------------------------------------------------------------------------------------------
// EditView::setEditBuffer 
//
// Sets a pointer to the underlying edit buffer that the EditView will use.
//
//-------------------------------------------------------------------------------------------------
void
EditView::setEditBuffer( CmEditBuffer *eb)
{
    if (editBuffer != NULL) {
        // remember the first visual line in the current edit buffer before changing edit buffers
        editBuffer->setVisualFirstScreenLine( _visibleEditBufferOffset );
        editBuffer->setVisualFirstScreenCol( _visibleFirstEditBufferCol );
    }
    
    // change the edit buffer
    editBuffer = eb;

    // detect language mode from file path for syntax highlighting
    markUp->setLanguageFromFilePath( editBuffer->getFilePath() );

    // recalc some information
    _windowTooSmall = false;

    _screenNumberOfCols = screen->cols();

    // the offset into the edit buffer of the visible part
    
    // get these from the editbuffer which the values were stored in from last context switch
    _visibleEditBufferOffset   = editBuffer->getVisualFirstScreenLine( );
    _visibleFirstEditBufferCol = editBuffer->getVisualFirstScreenCol( );
    
    // everything else is calculated
    _visibleLastEditBufferCol  = _screenNumberOfCols;
    _lineNumberOffset          = 6;

    // calculate location of other screen components
    recalcScreenPlacements();

    // setup some key windowing variables
    recalcVisibleBufferFromTopEditLine( _visibleEditBufferOffset );

}


//-------------------------------------------------------------------------------------------------
// EditView::reframeAndUpdateScreen(void)
//
// redraw the editview after reframing the buffer
//-------------------------------------------------------------------------------------------------
void
EditView::reframeAndUpdateScreen(void)
{
    // update the entire window
    reframe();
    updateScreen();
}

//-------------------------------------------------------------------------------------------------
// EditView::updateScreen
//
// Updates the screen using all the existing calculated geometry.
//
//-------------------------------------------------------------------------------------------------
void
EditView::updateScreen(void)
{
    CxString text = formatMultipleEditorLines(0,0);
    fputs( text.data() , stdout);
    updateStatusLine();
    screen->flush();
}

//-------------------------------------------------------------------------------------------------
// EditView::cursorPosition
//
// returns the row and col of the current cursor in the edit buffer.  Its a passthrough to the
// underlying editbuffer structure
//
//-------------------------------------------------------------------------------------------------
CxEditBufferPosition
EditView::cursorPosition(void)
{
    return( editBuffer->cursor );
}


//-------------------------------------------------------------------------------------------------
// EditView::currentFilePath
// 
// returns the filepath of the current file being edited.
// 
//-------------------------------------------------------------------------------------------------
CxString
EditView::currentFilePath(void)
{
    return( editBuffer->getFilePath() );
}


//-------------------------------------------------------------------------------------------------
// EditView::setCurrentFilePath
// 
// sets the file path that will be used to save the file
// 
//-------------------------------------------------------------------------------------------------
void
EditView::setCurrentFilePath( CxString path)
{
    editBuffer->setFilePath( path );
}


//-------------------------------------------------------------------------------------------------
// EditView::toggleLineNumbers
// 
// toggle line numbers on/off
// 
//-------------------------------------------------------------------------------------------------
void
EditView::toggleLineNumbers(void)
{
    if (_showLineNumbers == TRUE) {
        _showLineNumbers = FALSE;
    } else {
        _showLineNumbers = TRUE;
    }
    
    recalcLineNumberDigits();
    recalcForResize();
}


//-------------------------------------------------------------------------------------------------
// EditView::toggleJumpScroll
//
// toggle line numbers on/off
//
//-------------------------------------------------------------------------------------------------
void
EditView::toggleJumpScroll(void)
{
    if (_jumpScroll == TRUE) {
        _jumpScroll = FALSE;
    } else {
        _jumpScroll = TRUE;
    }
}


#if defined(_LINUX_) || defined(_OSX_)
//-------------------------------------------------------------------------------------------------
// EditView::setMcpConnected
//
// Set MCP connection status for status bar display
//
//-------------------------------------------------------------------------------------------------
void
EditView::setMcpConnected(int connected)
{
    _mcpConnected = connected;
}
#endif


//-------------------------------------------------------------------------------------------------
// EditView::setRegion
//
// Sets the screen region this view occupies. Use -1 for either parameter to indicate
// "use full screen" behavior. When both are -1, the view uses the entire screen.
// When set to specific values, the view only uses rows from startRow to endRow.
//
//-------------------------------------------------------------------------------------------------
void
EditView::setRegion(int startRow, int endRow)
{
    _regionStartRow = startRow;
    _regionEndRow = endRow;
    recalcScreenPlacements();
    // Recalculate visible buffer range based on new _screenEditNumberOfLines
    recalcVisibleBufferFromTopEditLine(_visibleFirstEditBufferRow);
}




//-------------------------------------------------------------------------------------------------
// EditView::recalcForResize
//
// Public method called by ScreenEditor after screen resize.
// Recalculates screen placements and visible buffer range.
//
//-------------------------------------------------------------------------------------------------
void
EditView::recalcForResize(void)
{
    recalcScreenPlacements();
    recalcVisibleBufferFromTopEditLine(_visibleFirstEditBufferRow);
    reframe();
}


//-------------------------------------------------------------------------------------------------
// EditView::recalcLineNumberDigits
//
// Calculates the space needed for the line numbers ahead of of each line of text.
// 
//
//-------------------------------------------------------------------------------------------------
void
EditView::recalcLineNumberDigits( void )
{
    char buffer[20];
    sprintf(buffer, "%lu| ", editBuffer->numberOfLines());
    CxString check(buffer);

    unsigned long length = check.length();
    if (length < 6) length = 6;

    if (_lineNumberOffset != length) {
        _lineNumberOffset = length;
    }
    
    if (_showLineNumbers == FALSE) {
        _lineNumberOffset = 0;
    }
}


//-------------------------------------------------------------------------------------------------
// EditView::recalcScreenPlacements
//
// Given a screen window size, this calculates all the rows that are important to
// to the editor. Supports both full-screen mode (region == -1) and split-screen mode
// where this view only occupies a portion of the screen.
//
// TODO: have to fix the calculations on firstScreenLine and firstScreenCol for buffer
//       switches.
//
//-------------------------------------------------------------------------------------------------
void
EditView::recalcScreenPlacements(void)
{
    // get the number of lines
    _screenNumberOfLines = screen->rows();
    _screenNumberOfCols  = screen->cols();

    // set the first visible edit column
    _visibleFirstEditBufferCol = 0;         // TODO: I think this one needs to change

	// set the last visible edit column
    if (_showLineNumbers == TRUE) {
		_screenEditNumberOfCols    = _screenNumberOfCols - _lineNumberOffset;
	   	_visibleLastEditBufferCol  = _screenNumberOfCols - _lineNumberOffset -1;
    } else {
        _screenEditNumberOfCols    = _screenNumberOfCols;
        _visibleLastEditBufferCol  = _screenNumberOfCols;
    }

    // Check if using region mode or full screen mode
    if (_regionStartRow == -1 && _regionEndRow == -1) {
        //-------------------------------------------------------------------------------------
        // Full screen mode - use original calculations exactly
        //-------------------------------------------------------------------------------------

        // set the number of edit lines (subtract 2 for status and command rows)
        _screenEditNumberOfLines = _screenNumberOfLines - 2;

        // if window is too small to support the app, just bail
        if ((screen->rows() < 6) || (_screenNumberOfCols < 10)) {
            _windowTooSmall = true;
            return;
        }

        // upper left is always zero (zero based screen coordinates)
        _screenEditFirstRow = 0;

        // the last edit row is always three up from the bottom of screen
        _screenEditLastRow  = _screenNumberOfLines - 3;

        // the status row is always 2 up
        _screenStatusRow    = _screenNumberOfLines - 2;

        // the command row is always the last row
        _screenCommandRow   = _screenNumberOfLines - 1;

    } else {
        //-------------------------------------------------------------------------------------
        // Region mode - use specified bounds for split screen
        //-------------------------------------------------------------------------------------
        int effectiveStartRow = (_regionStartRow == -1) ? 0 : _regionStartRow;
        int effectiveEndRow = (_regionEndRow == -1) ? (int)_screenNumberOfLines - 2 : _regionEndRow;

        // all rows in region are for editing
        _screenEditNumberOfLines = effectiveEndRow - effectiveStartRow;

        // if region is too small, bail
        if (_screenEditNumberOfLines < 3 || _screenNumberOfCols < 10) {
            _windowTooSmall = true;
            return;
        }

        // first edit row is start of region
        _screenEditFirstRow = effectiveStartRow;

        // last edit row is end of region minus 1
        _screenEditLastRow = effectiveEndRow - 1;

        // status line is at end of region (each EditView owns its status bar)
        _screenStatusRow = effectiveEndRow;

        // command line is still at global position
        _screenCommandRow = _screenNumberOfLines - 1;
    }

    // Screen Locations key (full screen mode)
    //-----------------------------------------
    // 0 TEXT          = _screenFirstEditRow;
    // 1 TEXT          = _screenLastEditRow;
    // 2 STATUS LINE   = _screenStatusRow;
    // 3 COMMAND LINE  = _screenCommandRow;
    //------------------------------------------
}


//-------------------------------------------------------------------------------------------------
// EditView::findString
// 
// find the first occurance of the find string in the buffer past or at the current cursor
// location.
//
//-------------------------------------------------------------------------------------------------
int
EditView::findString( CxString findString )
{
    // try and find the string starting at loc
	int result = editBuffer->findString( findString );
    
    CxEditBufferPosition loc = editBuffer->cursor;
    cursorGotoPosition( loc );
    
    return( result );
}


//-------------------------------------------------------------------------------------------------
// EditView::findAgain
//
// repeasts the last find command.  If the cursor is on the location of the last successul find
// it will advance the cursor and do another find to avoid finding the same string over and over
//
//-------------------------------------------------------------------------------------------------
int
EditView::findAgain( CxString findString  )
{
    int result = editBuffer->findAgain( findString, TRUE );
    
    CxEditBufferPosition loc = editBuffer->cursor;
    cursorGotoPosition( loc );
    
    return( result );
}


//-------------------------------------------------------------------------------------------------
// EditView:: replaceString
//
// replace the current findString with the replaceString.  Note this only replaces if the cursor
// is located at the begining of the find string in the buffer.  If not it just repeats the last
// find and leaves the cursor there.  This allows the behavior of moving through the file
// find-by-find and replacing or not.  If the user wants to replace, they just ctrl-r, the
// string is replaced, and moves to the next find.  If the user does not want to replace the
// occurance, then they hit the right arrow and the ctrl-r just finds the next occurance leaving
// the cursor on it.
//
//-------------------------------------------------------------------------------------------------
int
EditView::replaceString( CxString findString, CxString replaceString )
{
    // remember the location of the last find
    CxEditBufferPosition loc = editBuffer->cursor;
    
    // call replace
    int result = editBuffer->replaceString( findString, replaceString );

  	// update the screen after replacement
    if (findString.firstChar('\n') != -1 || replaceString.firstChar('\n') != -1) {
        reframe();
        updateScreen();
    } else {
        CxString line = formatEditorLine( loc.row );
        line = CxStringUtils::replaceTabExtensionsWithSpaces( line );
        screen->writeText( line );
    }

/*
	if (!result) {
		for (int c=0; c<replaceString.length(); c++){
			editBuffer->cursorRightRequest();
		}	
	}
*/

            
	// now get the resulting cursor line
//    loc = editBuffer->cursor;

//	for (int c=0; c<replaceString.length(); c++){
//		editBuffer->cursorRightRequest();
//	}

  //  loc = editBuffer->cursor;

    // move the screen cursor to that line
    //cursorGotoPosition( loc );
    
    
    return( result );
}


//-------------------------------------------------------------------------------------------------
// EditView::replaceAgain
//
// Do the same replacement we did last time
//
//-------------------------------------------------------------------------------------------------
int
EditView::replaceAgain( CxString findString, CxString replaceString )
{
    // remember the location of the last find
    CxEditBufferPosition loc = editBuffer->cursor;

    // replace the string again, if its at the cursor
    int result = editBuffer->replaceAgain( findString, replaceString );

	// update the screen after replacement
    if (findString.firstChar('\n') != -1 || replaceString.firstChar('\n') != -1) {
        reframe();
        updateScreen();
    } else {
        CxString line = formatEditorLine( loc.row );
        line = CxStringUtils::replaceTabExtensionsWithSpaces( line );
        screen->writeText( line );
    }

/*
	if (!result) {
		for (int c=0; c<replaceString.length(); c++){
			editBuffer->cursorRightRequest();
		}	
	}
*/
	// now get the resulting cursor line
//    loc = editBuffer->cursor;

//	for (int c=0; c<replaceString.length(); c++){
//		editBuffer->cursorRightRequest();
//	}

//    loc = editBuffer->cursor;

    // move the screen cursor to that line
//    cursorGotoPosition( loc );








       
    // get the current location
//    loc = editBuffer->cursor;
        
    // put the screen cursor there
//    cursorGotoPosition( loc );
   
    
    return( result );
}


//-------------------------------------------------------------------------------------------------
// EditView::pageUp
//
// Move the cursor up a page and reframe the page
//
//-------------------------------------------------------------------------------------------------
void
EditView::cursorGotoLine( unsigned long row )
{
    if (editBuffer->numberOfLines() == 0) return;

    if (row >= editBuffer->numberOfLines()) {
        row  = editBuffer->numberOfLines()-1;
    }

    editBuffer->cursorGotoLine( row );

    if ( rowVisible(editBuffer->cursor.row) && colVisible(editBuffer->cursor.col)) {

    } else {
        reframe();
		updateScreen();
    }
}


//-------------------------------------------------------------------------------------------------
// EditView::cursorGotoPosition
//
// Move the cursor up a page and reframe the page
//
//-------------------------------------------------------------------------------------------------
void
EditView::cursorGotoPosition( CxEditBufferPosition loc )
{
    if (editBuffer->numberOfLines() == 0) return;

    if (loc.row >= editBuffer->numberOfLines()) {
		return;
    }

    editBuffer->cursorGotoRequest( loc.row, loc.col );
	
    if ( !rowVisible(editBuffer->cursor.row) || !colVisible(editBuffer->cursor.col)) {
        if (reframe()) {
            updateScreen();
        }
    }
}


//-------------------------------------------------------------------------------------------------
// EditView::pageDown
//
// Move the cursor down a page and the reframe the page
//
//-------------------------------------------------------------------------------------------------
void
EditView::pageDown( void )
{
    // get the cursor's relative position within this view's region
    unsigned long screenRowOfCursor = bufferRowToScreenRow(editBuffer->cursor.row) - _screenEditFirstRow;

    // compute the new logical row number (use _screenEditNumberOfLines for region-aware paging)
    unsigned long newBufferRow = editBuffer->cursor.row + _screenEditNumberOfLines;

    // compute the new logical row plus the offset of the cursor screen row
    unsigned long newBufferRowWithOffset = newBufferRow + (_screenEditNumberOfLines - screenRowOfCursor);

    // jump in the logical buffer ahead to to new cursor location plus the offset
    editBuffer->cursorGotoLine( newBufferRowWithOffset );

    // update the screen
    if ( !rowVisible(editBuffer->cursor.row) || !colVisible(editBuffer->cursor.col)) {
		if (reframe()) {
			updateScreen();
		}
    }

    // now put the cursor back to the orginal physical screen location
    editBuffer->cursorGotoLine( newBufferRow );
}


//-------------------------------------------------------------------------------------------------
// EditView::insertCommentBlock
//
//
//
//-------------------------------------------------------------------------------------------------
void
EditView::insertCommentBlock( unsigned long lastCol )
{
	editBuffer->insertCommentBlock( lastCol ) ;
}


//-------------------------------------------------------------------------------------------------
// EditView::pageUp
//
// performs the page up command the reframes the edit buffer on the physical screen
//
//-------------------------------------------------------------------------------------------------
void
EditView::pageUp( void )
{
    // get the cursor's relative position within this view's region
    unsigned long screenRowOfCursor = bufferRowToScreenRow(editBuffer->cursor.row) - _screenEditFirstRow;

    // compute the new logical row number, and make sure we can still page up
    // use _screenEditNumberOfLines for region-aware paging
    unsigned long newBufferRow = 0;
    if (editBuffer->cursor.row <= _screenEditNumberOfLines ) {

        // we are on the first page, so just jump the cursor to the top of it
        editBuffer->cursorGotoLine( newBufferRow );
        return;
    }

    // compute the new buffer row number
    newBufferRow = editBuffer->cursor.row - _screenEditNumberOfLines;

    // compute a new jump with offset for the current cursor line
    unsigned long newBufferRowWithOffset = 0;
    if ( newBufferRow >= screenRowOfCursor) {
        newBufferRowWithOffset = newBufferRow - screenRowOfCursor;
    }

    // jump in the logical buffer back to to new cursor location minus the offset
    editBuffer->cursorGotoLine( newBufferRowWithOffset );

    // update the screen
    if ( !rowVisible(editBuffer->cursor.row) || !colVisible(editBuffer->cursor.col)) {
        if (reframe()) {
            updateScreen();
        }
    }

    // now put the cursor back to the orginal physical screen location
    editBuffer->cursorGotoLine( newBufferRow );
}


//-------------------------------------------------------------------------------------------------
// EditView::cutTextCursorToEndOfLine
//
// performs the cut text from cursor to end of line function, first calling the edit buffer
// version to perform the logical operation, then updating the screen as required.
//
//-------------------------------------------------------------------------------------------------
CxString
EditView::cutTextCursorToEndOfLine( void )
{
    CxString cutText = editBuffer->cutTextToEndOfLine();
    updateScreen();
    return( cutText );
}


//-------------------------------------------------------------------------------------------------
// EditView::pasteText
//
// calls the pasteText edit buffer function then updates the screen
//
//-------------------------------------------------------------------------------------------------
void
EditView::pasteText( CxString text)
{
    editBuffer->pasteFromCutBuffer( text );
    updateScreen();
}


#ifdef CM_UTF8_SUPPORT
//-------------------------------------------------------------------------------------------------
// EditView::pasteText (CxUTFString version)
//
// Paste pre-parsed UTF-8 text then update the screen
//
//-------------------------------------------------------------------------------------------------
void
EditView::pasteText( CxUTFString &text )
{
    editBuffer->pasteFromCutBuffer( text );
    updateScreen();
}
#endif


//-------------------------------------------------------------------------------------------------
// EditView::setMark
//
// calls the setMark edit buffer function
//
//-------------------------------------------------------------------------------------------------
void
EditView::setMark( void )
{
    editBuffer->setMark();
}


//-------------------------------------------------------------------------------------------------
// EditView::cutToMark
//
// calls the cutToMark edit buffer function then updates the screen
//
//-------------------------------------------------------------------------------------------------
CxString
EditView::cutToMark( void )
{
    CxString text = editBuffer->cutToMark();
    updateScreen();
    
    return(text);
}


//-------------------------------------------------------------------------------------------------
// EditView::cutTextToEndOfLine
//
// calls the cutTextToEndOfLine edit buffer function then updates the screen
//
//-------------------------------------------------------------------------------------------------
CxString
EditView::cutTextToEndOfLine(void)
{
    CxString text = editBuffer->cutTextToEndOfLine();
    updateScreen();
    
    return( text );
}


//-------------------------------------------------------------------------------------------------
// EditView::placeCursor
//
// Makes sure the cursor is in the correct location on the screen
//
//-------------------------------------------------------------------------------------------------
void
EditView::placeCursor(void)
{
    // put the cursor back
    screen->placeCursor(
        bufferRowToScreenRow(editBuffer->cursor.row),
        bufferColToScreenCol(editBuffer->cursor.col)
        );

    //screen->flush();
}


//-------------------------------------------------------------------------------------------------
// EditView:: getEditBuffer
//
// return the underlying edit buffer
// 
//-------------------------------------------------------------------------------------------------
CmEditBuffer *
EditView::getEditBuffer(void)
{
    return(editBuffer);
}

