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

    // setup the resize callback
	screen->addScreenSizeCallback( CxDeferCall( this, &EditView::screenResizeCallback ));

	// set the default for showing line numbers or not
	_showLineNumbers = pd->showLineNumbers();
    
    // set the jumpScroll variable from defaults
    _jumpScroll = pd->jumpScroll();
    
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
    // DEBUG logging
    FILE *dbg = fopen("/tmp/cm_debug.log", "a");
    if (dbg) { fprintf(dbg, "  updateScreen: entered\n"); fflush(dbg); }

    CxString text = formatMultipleEditorLines(0,0);

    if (dbg) { fprintf(dbg, "  updateScreen: formatMultipleEditorLines done, text len=%d\n", text.length()); fflush(dbg); }

    fputs( text.data() , stdout);

    if (dbg) { fprintf(dbg, "  updateScreen: fputs done\n"); fflush(dbg); }

    updateStatusLine();

    if (dbg) { fprintf(dbg, "  updateScreen: updateStatusLine done\n"); fflush(dbg); }

    screen->flush();

    if (dbg) { fprintf(dbg, "  updateScreen: flush done\n"); fclose(dbg); }
}

//-------------------------------------------------------------------------------------------------
// EditView::screenResizeCallback (callback)
//
// Called with the user resizes the terminal window.  Recalculates key parts of the screen.
//
//-------------------------------------------------------------------------------------------------
void
EditView::screenResizeCallback( void )
{
    // recalculate all the component placements
    recalcScreenPlacements();

    // keep the top line the top line (not correct)
    recalcVisibleBufferFromTopEditLine(_visibleFirstEditBufferRow );

    // recalc and redraw
    reframeAndUpdateScreen();
    
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
    screenResizeCallback();
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
// to the editor.
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
    
  	// set the number of edit lines
    _screenEditNumberOfLines = _screenNumberOfLines - 3;

    // if window is too small to support the app, just bail
    if ((screen->rows() < 6) || (_screenNumberOfCols < 10)) {
        _windowTooSmall = true;
        return;
    }

    // upper left is always zero (zero based screen coordinates)
    _screenEditFirstRow = 0;

    // the last edit row is always three up from the bottom of screen
    _screenEditLastRow  = _screenEditNumberOfLines - 3;

    // the status row is always 2 up
    _screenStatusRow    = _screenNumberOfLines - 2;

    // the command row is always the last row
    _screenCommandRow   = _screenNumberOfLines - 1;

    // Screen Locations key
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
    // get the physical screen row the cursor is on
    unsigned long screenRowOfCursor = bufferRowToScreenRow( editBuffer->cursor.row  );

    // computer the new logical row number
    unsigned long newBufferRow = editBuffer->cursor.row + _screenNumberOfLines;

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
    // get the physical screen row the cursor is on
    unsigned long screenRowOfCursor = bufferRowToScreenRow( editBuffer->cursor.row  );
    
    // computer the new logical row number, and make sure we can still page up
    unsigned long newBufferRow = 0;
    if (editBuffer->cursor.row <= _screenNumberOfLines ) {
        
        // we are on the first page, so just jump the cursor to the top of it
        editBuffer->cursorGotoLine( newBufferRow );
        return;
    }
    
    // compute the new buffer row number
    newBufferRow = editBuffer->cursor.row - _screenNumberOfLines;
 
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
// EditView::recalcVisibleBufferFromTopEditLine
//
// Recalculate the visible buffer based on a new offset into the edit buffer
//
//-------------------------------------------------------------------------------------------------
void
EditView::recalcVisibleBufferFromTopEditLine(unsigned long newUpperRow)
{
    _visibleEditBufferOffset = newUpperRow;
    _visibleFirstEditBufferRow = _visibleEditBufferOffset;
    _visibleLastEditBufferRow  = _visibleFirstEditBufferRow + _screenEditNumberOfLines;
}


//-------------------------------------------------------------------------------------------------
// EditView::recalcVisibleBufferFromBottomEditLine
//
// Recalculate the visible buffer based on a new last visible line index
//
//-------------------------------------------------------------------------------------------------
void
EditView::recalcVisibleBufferFromBottomEditLine(unsigned long newLowerRow)
{
    _visibleEditBufferOffset = newLowerRow - _screenEditNumberOfLines;
    _visibleFirstEditBufferRow = _visibleEditBufferOffset;
    _visibleLastEditBufferRow  = newLowerRow;
}


//-------------------------------------------------------------------------------------------------
// EditView::recalcVisibleBufferFromLeft
//
// calculate the visible area based on a left most buffer column
//
//
//-------------------------------------------------------------------------------------------------
void
EditView::recalcVisibleBufferFromLeft( unsigned long bufferCol )
{
    _visibleFirstEditBufferCol = bufferCol; //todd
    _visibleLastEditBufferCol  = _visibleFirstEditBufferCol + _screenEditNumberOfCols;
}


//-------------------------------------------------------------------------------------------------
// EditView::recalcVisibleBufferFromRight
//
// calculate the visible area based on a right most buffer column
//
//
//-------------------------------------------------------------------------------------------------
void
EditView::recalcVisibleBufferFromRight( unsigned long bufferCol )
{
    _visibleLastEditBufferCol  = bufferCol;
    _visibleFirstEditBufferCol = _visibleLastEditBufferCol - _screenEditNumberOfCols;
}


//-------------------------------------------------------------------------------------------------
// EditView::reframe
//
// Moves the visible window indexes so the cursor is visible and updates the screen
//
//-------------------------------------------------------------------------------------------------
int
EditView::reframe( void )
{
    unsigned long bufferRow = editBuffer->cursor.row;
    unsigned long bufferCol = editBuffer->cursor.col;

    //---------------------------------------------------------------------------------------------
    // if cursor is visible on the screen, then bail out and don't calculate any
    // thing or redraw
    //---------------------------------------------------------------------------------------------
    if (rowVisible(bufferRow) && (colVisible(bufferCol))) {
        return( FALSE );
    }
    
    //---------------------------------------------------------------------------------------------
    // check if the cursor position is outside the visible area, if it is reframe the visible
    // part of the buffer
    //---------------------------------------------------------------------------------------------

    // if cursor row is above the first edit row, move
    // window up so its visible
    if (bufferRow < _visibleFirstEditBufferRow) {
        recalcVisibleBufferFromTopEditLine( bufferRow );
    }

    // if cursor row is below the last edit row, move the
    // visible window down so the line is the last edit line
    if (bufferRow > _visibleLastEditBufferRow) {
        recalcVisibleBufferFromBottomEditLine( bufferRow );
    }

    // if the cursor col is to the left of the visible part
    // scroll all the content right
    if (bufferCol < _visibleFirstEditBufferCol) {
        recalcVisibleBufferFromLeft( bufferCol );
    }

    // if the cursor col is to the right of the visible part
    // of the window, scroll the content to left
    if (bufferCol > _visibleLastEditBufferCol) {
        recalcVisibleBufferFromRight( bufferCol );
    }
    
    return( TRUE );
    
}


//-------------------------------------------------------------------------------------------------
// EditView::reframe_jump
//
// Moves the visible window indexes so the cursor is visible. Unlike the non jump version this
// method jumps the screen up or down have a visible screen to speed scrolling on slower
// boxes.
//
//-------------------------------------------------------------------------------------------------
int
EditView::reframe_jump( void )
{
    unsigned long bufferRow = editBuffer->cursor.row;
    unsigned long bufferCol = editBuffer->cursor.col;
    
    //---------------------------------------------------------------------------------------------
    // if cursor is visible on he screen, then bail out and don't calculate any
    // thing or redraw
    //---------------------------------------------------------------------------------------------
    if (rowVisible(bufferRow) && (colVisible(bufferCol))) {
        return(FALSE);
    }
    
    //---------------------------------------------------------------------------------------------
    // check if the cursor position is outside the visible area, if it is reframe the visible
    // part of the buffer
    //---------------------------------------------------------------------------------------------

    // if cursor row is above the first edit row, move
    // window up so its visible
    if (bufferRow < _visibleFirstEditBufferRow) {
        
        unsigned long newBufferTargetRow = 0;
        
        //-----------------------------------------------------------------------------------------
        // make sure we don't target a row less than the first row
        //-----------------------------------------------------------------------------------------
        if (bufferRow < (_screenNumberOfLines/2)) {
            newBufferTargetRow = 0;
        } else {
            newBufferTargetRow = bufferRow - (_screenNumberOfLines/2);
        }
        
        recalcVisibleBufferFromTopEditLine( newBufferTargetRow );
    }

    // if cursor row is below the last edit row, move the
    // visible window down so the line is the last edit line
    if (bufferRow > _visibleLastEditBufferRow) {
        
        unsigned long newBufferRowTarget = bufferRow + (_screenNumberOfLines/2);
        if (newBufferRowTarget > editBuffer->numberOfLines()) {
            newBufferRowTarget = editBuffer->numberOfLines();
        }
        
        recalcVisibleBufferFromBottomEditLine( newBufferRowTarget );
    }

    // if the cursor col is to the left of the visible part
    // scroll all the content right
    if (bufferCol < _visibleFirstEditBufferCol) {
        recalcVisibleBufferFromLeft( bufferCol );
    }

    // if the cursor col is to the right of the visible part
    // of the window, scroll the content to left
    if (bufferCol > _visibleLastEditBufferCol) {
        recalcVisibleBufferFromRight( bufferCol );
    }
    
    return( TRUE );
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
// EditView::rowVisible
//
// returns true if the edit buffer line is currently visible, false if it is not
//
//-------------------------------------------------------------------------------------------------
int
EditView::rowVisible( unsigned long bufferRow )
{
    if ((bufferRow >= _visibleFirstEditBufferRow) &&
        (bufferRow <= _visibleLastEditBufferRow)) {
        return( true );
    }

    return(false);
}


//-------------------------------------------------------------------------------------------------
// EditorCommandLineView::colVisible
//
// return true if the the requested col is visible, false if it is not
//
//
//-------------------------------------------------------------------------------------------------
int
EditView::colVisible( unsigned long bufferCol )
{
    if ((bufferCol >= _visibleFirstEditBufferCol) &&
        (bufferCol < _visibleLastEditBufferCol-10)) {
        return( true );
    }

    return( false );
}


//-------------------------------------------------------------------------------------------------
// EditView::updateStatusLine
//
// updates the edit window status line
//
//-------------------------------------------------------------------------------------------------
void
EditView::updateStatusLine(void)
{
    unsigned long row = editBuffer->cursor.row;
    unsigned long col = editBuffer->cursor.col;
    unsigned long numberOfLines = editBuffer->numberOfLines();

    double percent = 0;
    
    if (row == 0) {
        percent = 0;
    } else {
        percent = (double) row / (double) numberOfLines;
    }
        
    percent = percent * 100;

    screen->placeCursor( _screenStatusRow, 0);

	// set the status bar foreground and background colors    
    screen->setForegroundColor(programDefaults->statusBarTextColor() );
    screen->setBackgroundColor(programDefaults->statusBarBackgroundColor() );
    
    CxString statusLineTextLeft;
    statusLineTextLeft  = "== ";
    statusLineTextLeft += "cm: Editing [ ";
    statusLineTextLeft += editBuffer->getFilePath();
    statusLineTextLeft += " ] ";
    
    //---------------------------------------------------------------------------------------------
    // do the line part of the status line
    //
    //---------------------------------------------------------------------------------------------
    
    CxString colPartString = "========";
    CxString statusLineTextRight;
    
    if (programDefaults->liveStatusLine()) {
        
        char buffer[100];
        
        sprintf(buffer, " line(%lu,%lu,%.0lf%%)", row, numberOfLines, percent);
        CxString linePartString = buffer;
        
        //---------------------------------------------------------------------------------------------
        // do the col part of the status line, we pad the col part so the line of text doesn't
        // jump around between lines with where the cursor is in a different stop
        //
        //---------------------------------------------------------------------------------------------
        
        sprintf(buffer, "col(%lu)", col);
        colPartString = buffer;
        
        while (colPartString.length() < 8) {
            colPartString += "=";
        }
        
        statusLineTextRight = linePartString + CxString(" ") + colPartString;
        
    }
        
    //---------------------------------------------------------------------------------------------
    // now put the two right had parts together
    //
    //---------------------------------------------------------------------------------------------
    
   // CxString statusLineTextRight = linePartString + CxString(" ") + colPartString;
    
    //---------------------------------------------------------------------------------------------
    // get the curent length of all the important parts of the status line
    //
    //---------------------------------------------------------------------------------------------
    int statusLineTextLength = statusLineTextLeft.length() + statusLineTextRight.length();

    //---------------------------------------------------------------------------------------------
    // calculate the number of = signs to put between the two
    //
    //---------------------------------------------------------------------------------------------
    int positionsLeft = screen->cols() - statusLineTextLength;

    //---------------------------------------------------------------------------------------------
    // append that number of equal signs
    //
    //---------------------------------------------------------------------------------------------
    CxString theText = statusLineTextLeft;
    for (int c=0; c< positionsLeft; c++) {
        theText.append("=");
    }
    theText += statusLineTextRight;
    
    
    //---------------------------------------------------------------------------------------------
    // finally write out the status line
    //
    //---------------------------------------------------------------------------------------------
    screen->writeTextAt( _screenStatusRow, 0, theText, true );
    
    // put cursor back
    screen->placeCursor(
        bufferRowToScreenRow(editBuffer->cursor.row),
        bufferColToScreenCol(editBuffer->cursor.col)
        );

    screen->resetColors();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// EditView::bufferRowToScreenRow
//
// transforms a editbuffer index into a screen index (zero based).
//
//-------------------------------------------------------------------------------------------------
unsigned long
EditView::bufferRowToScreenRow(unsigned long bufferRow)
{
    // get the physical screen line (zero based)
    unsigned long screenLine = bufferRow - _visibleFirstEditBufferRow;
    return(screenLine);
}


//-------------------------------------------------------------------------------------------------
// CxEditWindow::bufferColToScreenCol
//
// transforms a editbuffer index into a screen index (zero based).
//
//-------------------------------------------------------------------------------------------------
unsigned long
EditView::bufferColToScreenCol(unsigned long bufferCol)
{
#ifdef CM_UTF8_SUPPORT
    // For UTF-8, characters can have different display widths (CJK=2, combining=0)
    // Use the buffer's display column calculation for accurate screen positioning
    unsigned long displayCol = editBuffer->cursorDisplayColumn();
    unsigned long screenCol = displayCol - _visibleFirstEditBufferCol + _lineNumberOffset;
#else
    // For byte-based buffers, column index equals display column
    unsigned long screenCol = bufferCol - _visibleFirstEditBufferCol + _lineNumberOffset;
#endif
    return(screenCol);
}



//-------------------------------------------------------------------------------------------------
// EditView::updateRemainderOfWindow
//
// updates the entire window
//
//-------------------------------------------------------------------------------------------------
CxString
EditView::formatMultipleEditorLines(unsigned long bufferRow, unsigned long bufferCol )
{
    CxString windowText = CxCursor::hideTerminalString();
    

    // update each line, this also includes lines beyond the buffer but are
    // handled by the called function
    for (unsigned long r=bufferRow; r<=_visibleLastEditBufferRow; r++)
    {
        // if the row is visible on the screen
        if (rowVisible(r)) {
             windowText += formatEditorLine( r );
        }
    }
    
    windowText += CxCursor::locateTerminalString(bufferRowToScreenRow(editBuffer->cursor.row),
                                                 bufferColToScreenCol(editBuffer->cursor.col)
                                                 );
    
    windowText = CxStringUtils::replaceTabExtensionsWithSpaces( windowText );
    
    windowText += CxCursor::showTerminalString();
    
    return( windowText );
}




//-------------------------------------------------------------------------------------------------
// EditView::formatEditorLine
//
// the formats a string with the contents of the entire line of text including line number
// and syntax coloring if enabled.
//
//-------------------------------------------------------------------------------------------------
CxString
EditView::formatEditorLine(unsigned long bufferRow )
{
    int isCommentLine = FALSE;
    int isIncludeLine = FALSE;
    
    //---------------------------------------------------------------------------------------------
    // place the cursor at the correct place on the screen
    //---------------------------------------------------------------------------------------------
    CxString lineNumberString = CxCursor::locateTerminalString(
                bufferRowToScreenRow(bufferRow),
                0);
    
    
    //---------------------------------------------------------------------------------------------
    // check the the logical row is visible return if not
    //
    //---------------------------------------------------------------------------------------------
    if (bufferRow < _visibleFirstEditBufferRow) return(lineNumberString);
    if (bufferRow > _visibleLastEditBufferRow)  return(lineNumberString);
    
    //---------------------------------------------------------------------------------------------
    // if we are showing line numbers, then build up the line number string
    //---------------------------------------------------------------------------------------------
    
    if (_showLineNumbers) {
        
        CxString bufferRowNumberString( bufferRow+1 );
        bufferRowNumberString = bufferRowNumberString + "| ";

        while (bufferRowNumberString.length() < _lineNumberOffset) {
            bufferRowNumberString = CxString(" ") + bufferRowNumberString;
        }
        
        lineNumberString += programDefaults->lineNumberTextColor()->terminalString();
        lineNumberString += bufferRowNumberString;
        lineNumberString += programDefaults->lineNumberTextColor()->resetTerminalString();
    }

    //---------------------------------------------------------------------------------------------
    // beyond buffer, just write out empty string to clear the line
    //
    //---------------------------------------------------------------------------------------------
    if (bufferRow >= editBuffer->numberOfLines()) {

		//-----------------------------------------------------------------------------------------
		// this is a special case as if the buffer is empty we do want to show the
		// the first line number
		//----------------------------------------------------------------------------------------- 
		if (bufferRow == 0) {
 
    	 	lineNumberString += CxCursor::clearToEndOfLineTerminalString();
			

		//-----------------------------------------------------------------------------------------
		// if not an empty buffer then just wipe out the linenumber and return a clear to
		// end of line
		//-----------------------------------------------------------------------------------------
		} else {

 	       	lineNumberString = CxCursor::locateTerminalString(
                    bufferRowToScreenRow(bufferRow),
                    0);
 
    	 	lineNumberString += CxCursor::clearToEndOfLineTerminalString();

		}

        return( lineNumberString );
    }
    

	//---------------------------------------------------------------------------------------------
    // get the line of text
    //
	//---------------------------------------------------------------------------------------------
#ifdef CM_UTF8_SUPPORT
    CxUTFString *utfLine = editBuffer->line(bufferRow);
    if (utfLine == NULL) {
        printf("null line in formatEditorLine\n");
        exit(0);
    }
    // Use toBytesExpanded() to expand tabs to spaces for display
    CxString fullText = utfLine->toBytesExpanded();
#else
    CxString *textPtr = editBuffer->line(bufferRow);
    if (textPtr == NULL) {
        printf("null line in formatEditorLine\n");
        exit(0);
    }
    CxString fullText = *textPtr;
#endif

    //---------------------------------------------------------------------------------------------
    // get a copy of the full text line and the visible text line we need to pass both to
    // to the colorize method
    //
    //---------------------------------------------------------------------------------------------
    CxString visibleText = fullText.subString(_visibleFirstEditBufferCol, _screenEditNumberOfCols );
    
    //---------------------------------------------------------------------------------------------
    // now colorize the text
    //
    //---------------------------------------------------------------------------------------------
    if (programDefaults->colorizeSyntax() ) {
        // use simple colorization without block comment state scanning
        // (block comment state tracking is done in formatMultipleEditorLines for efficiency)
        visibleText = markUp->colorizeText( fullText, visibleText );
    }
    
    //---------------------------------------------------------------------------------------------
    // turn off any text attributes that might have terminated the text
    //
    //---------------------------------------------------------------------------------------------
    visibleText += programDefaults->lineNumberTextColor()->resetTerminalString();
    
    //---------------------------------------------------------------------------------------------
    // prepend the line number string if there is one
    //
    //---------------------------------------------------------------------------------------------
    visibleText = lineNumberString + visibleText;
    visibleText += CxCursor::clearToEndOfLineTerminalString();

    return( visibleText );
}





//-------------------------------------------------------------------------------------------------
// CxEditWindow::updateRemainderOfWindowLine
//
// updates from the position in the buffer to the right on the specified line
//
//-------------------------------------------------------------------------------------------------

void
EditView::updateRemainderOfWindowLine(unsigned long bufferRow, unsigned long bufferCol )
{
    // check the the logical row is visible return if not
    if (bufferRow < _visibleFirstEditBufferRow) return;
    if (bufferRow > _visibleLastEditBufferRow)  return;
      
    CxString line = formatEditorLine( bufferRow );
    
    line = CxStringUtils::replaceTabExtensionsWithSpaces( line );
    
    fputs( line.data(), stdout);
            
            
}


//-------------------------------------------------------------------------------------------------
// EditView::handleArrows
//
// moves the cursor according to the desired direction and what is valid in the
// edit buffer
//
//-------------------------------------------------------------------------------------------------
EditView::EditStatus
EditView::handleArrows( CxKeyAction keyAction )
{
	if (keyAction.tag() == "<arrow-left>") {
		
        CxEditHint hint = editBuffer->cursorLeftRequest();

        if (reframe()) {
            updateScreen();
        }

        screen->placeCursor(
            bufferRowToScreenRow(editBuffer->cursor.row),
            bufferColToScreenCol(editBuffer->cursor.col)
            );
	}

	if (keyAction.tag() == "<arrow-right>") {

		CxEditHint hint = editBuffer->cursorRightRequest();

        if (reframe()) {
            updateScreen();
        }

        screen->placeCursor(
            bufferRowToScreenRow(editBuffer->cursor.row),
            bufferColToScreenCol(editBuffer->cursor.col)
            );
	}

	if (keyAction.tag() == "<arrow-down>") {
        
        CxEditHint hint = editBuffer->cursorDownRequest();

        if (_jumpScroll) {
            if (reframe_jump()) {
                updateScreen();
            }
        } else {
            if (reframe()) {
                updateScreen();
            }
        }
        
        screen->placeCursor(
            bufferRowToScreenRow(editBuffer->cursor.row),
            bufferColToScreenCol(editBuffer->cursor.col)
            );
	}

	if (keyAction.tag() == "<arrow-up>") {

		CxEditHint hint = editBuffer->cursorUpRequest();
        
        if (_jumpScroll) {
            if (reframe_jump()) {
                updateScreen();
            }
        } else {
            if (reframe()) {
                updateScreen();
            }
        }
        
        screen->placeCursor(
            bufferRowToScreenRow(editBuffer->cursor.row),
            bufferColToScreenCol(editBuffer->cursor.col)
            );
	}

    return( OK );
}



//-------------------------------------------------------------------------------------------------
// EditView::routeKeyAction
//	
// Keys passed into routeKeyAction are targeted for the edit view changing the text buffer
// and then the screen is updated to reflect.
//
//-------------------------------------------------------------------------------------------------
EditView::EditStatus
EditView::routeKeyAction( CxKeyAction keyAction )
{
    CxString lineText;

    recalcLineNumberDigits(  );

	//---------------------------------------------------------------------------------------------
	// based on the kind of key
	//
	//---------------------------------------------------------------------------------------------
	switch (keyAction.actionType() )
	{

		//-----------------------------------------------------------------------------------------
		// ignore command  types
		//
		//-----------------------------------------------------------------------------------------
        case CxKeyAction::COMMAND:
            {
                return( COMMAND );
            }
            break;

		//-----------------------------------------------------------------------------------------
		// if a cursor action is being called for pass on the cursor functions and update the
		// status bar
		//
		//-----------------------------------------------------------------------------------------
		case CxKeyAction::CURSOR:
			{
                handleArrows( keyAction );
                
                if (programDefaults->liveStatusLine()) updateStatusLine();
                screen->flush();
			}
			break;

		//-----------------------------------------------------------------------------------------
		// handle a small set of control functions, specifically backspace on some machines
		//
		//-----------------------------------------------------------------------------------------
        case CxKeyAction::CONTROL:
			{
				if (keyAction.tag() == "H") {

					CxEditHint editHint = editBuffer->addBackspace();

					reframe();
                    
					
					if (editHint.updateHint() == CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT ) {
						lineText = formatMultipleEditorLines( editHint.startRow(), editHint.startCol() );
                        //lineText = windowLineTerminalString( editHint.startRow() );
					}	

					if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE_PAST_POINT ) {
						//updateRemainderOfWindowLine( editHint.startRow(), editHint.startCol() );
                        lineText = formatEditorLine( editHint.startRow() );
					}	
					
					if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE) {
						//updateEntireWindowLine( editHint.startRow() );
                        lineText = formatEditorLine( editHint.startRow() );
					}

                    if (programDefaults->liveStatusLine()) updateStatusLine();

                    screen->flush();
				}
			}
			break;	

		//-----------------------------------------------------------------------------------------
		// handle a small set of options keys, specifically backspace/delete on some machines
		//
		//-----------------------------------------------------------------------------------------
        case CxKeyAction::OPTION:
            {
				if (keyAction.tag() == "<option-delete>") {

					CxEditHint editHint = editBuffer->addBackspace();
	
					reframe();
				
					if (editHint.updateHint() == CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT ) {
						lineText = formatMultipleEditorLines( editHint.startRow(), editHint.startCol() );
					}	

					if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE_PAST_POINT ) {
                        lineText = formatEditorLine( editHint.startRow() );
                    }
					
					if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE) {
                        lineText = formatEditorLine( editHint.startRow() );
                    }

                    if (programDefaults->liveStatusLine()) updateStatusLine();

                    screen->flush();
				}

            }
            break;

		//-----------------------------------------------------------------------------------------
		// basic alpha/numerics and symbols
		//
		//-----------------------------------------------------------------------------------------
       	case CxKeyAction::LOWERCASE_ALPHA:
      	case CxKeyAction::UPPERCASE_ALPHA:
       	case CxKeyAction::NUMBER:
       	case CxKeyAction::SYMBOL:
			{
				// add the character to the buffer
				CxEditHint editHint = editBuffer->addCharacter( keyAction.tag() );

				reframe();

				// first check if a simple update of the last part of the line will do
				if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE_PAST_POINT ) {
                    lineText = formatEditorLine( editHint.startRow() );
				}

				// first check if a simple update of the last part of the line will do
				if (editHint.updateHint() == CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT ) {
					lineText = formatMultipleEditorLines( editHint.startRow(), editHint.startCol());
				}
			
                if (programDefaults->liveStatusLine()) updateStatusLine();

                screen->flush();
			}
           	break;

		//-----------------------------------------------------------------------------------------
		// handle enter/return key
		//
		//-----------------------------------------------------------------------------------------
      	case CxKeyAction::NEWLINE:
			{
				CxEditHint editHint = editBuffer->addReturn();

                reframe();

				if (editHint.updateHint() == CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT ) {
					lineText = formatMultipleEditorLines( editHint.startRow(), editHint.startCol() );
				}

                if (programDefaults->liveStatusLine()) updateStatusLine();

                screen->flush();

			}
			break;

		//-----------------------------------------------------------------------------------------
		// handle a backspace action
		//
		//-----------------------------------------------------------------------------------------
     	case CxKeyAction::BACKSPACE:
			{
				CxEditHint editHint = editBuffer->addBackspace();
					
				reframe();

				if (editHint.updateHint() == CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT ) {
					lineText = formatMultipleEditorLines( editHint.startRow(), editHint.startCol() );
				}	

				if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE_PAST_POINT ) {
                    lineText = formatEditorLine( editHint.startRow() );
				}	
					
				if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE) {
                    lineText = formatEditorLine( editHint.startRow() );

				}

                if (programDefaults->liveStatusLine()) updateStatusLine();

                screen->flush();
			}
			break;

		//-----------------------------------------------------------------------------------------
		// hande a tab action
		//
		//-----------------------------------------------------------------------------------------
       	case CxKeyAction::TAB:
			{
				CxEditHint editHint = editBuffer->addTab();

				reframe();

				if (editHint.updateHint() == CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT ) {
					lineText = formatMultipleEditorLines( editHint.startRow(), editHint.startCol() );
				}	

				if (editHint.updateHint() == CxEditHint::UPDATE_HINT_LINE_PAST_POINT ) {
                    lineText = formatEditorLine( editHint.startRow() );
				}

                if (programDefaults->liveStatusLine()) updateStatusLine();

                screen->flush();
			}
          	break;

      	default:
        	break;
	}
     
    lineText = CxStringUtils::replaceTabExtensionsWithSpaces(lineText);
    
    lineText += CxCursor::locateTerminalString(
                bufferRowToScreenRow(editBuffer->cursor.row),
                bufferColToScreenCol(editBuffer->cursor.col));
    
    //updateStatusLine();
    screen->writeText( lineText );
    


    return( EditView::OK );
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

