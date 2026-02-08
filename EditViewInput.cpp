//-------------------------------------------------------------------------------------------------
//
//  EditViewInput.cpp
//  cmacs
//
//  Input handling, reframing, and visibility methods for EditView.
//  Extracted from EditView.cpp to reduce file size.
//
//-------------------------------------------------------------------------------------------------

#include "EditView.h"


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
    // -1 because _visibleLastEditBufferRow is inclusive (the last visible row)
    // With 10 screen lines and first=0, last should be 9, not 10
    _visibleLastEditBufferRow  = _visibleFirstEditBufferRow + _screenEditNumberOfLines - 1;
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
    // +1 because _visibleLastEditBufferRow is inclusive
    // With 10 screen lines and last=10, first should be 1 (rows 1-10 = 10 rows)
    _visibleEditBufferOffset = newLowerRow - _screenEditNumberOfLines + 1;
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
        // use _screenEditNumberOfLines for region-aware jump scrolling
        //-----------------------------------------------------------------------------------------
        if (bufferRow < (_screenEditNumberOfLines/2)) {
            newBufferTargetRow = 0;
        } else {
            newBufferTargetRow = bufferRow - (_screenEditNumberOfLines/2);
        }

        recalcVisibleBufferFromTopEditLine( newBufferTargetRow );
    }

    // if cursor row is below the last edit row, move the
    // visible window down so the line is the last edit line
    if (bufferRow > _visibleLastEditBufferRow) {

        // use _screenEditNumberOfLines for region-aware jump scrolling
        unsigned long newBufferRowTarget = bufferRow + (_screenEditNumberOfLines/2);
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
// EditView::updateAfterEdit
//
// Common pattern for updating the display after an edit operation. Handles reframing,
// choosing the appropriate update method based on the hint, updating status line, and flushing.
//
//-------------------------------------------------------------------------------------------------
void
EditView::updateAfterEdit( CxEditHint& hint, CxString& lineText )
{
    reframe();

    if (hint.updateHint() == CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT) {
        lineText = formatMultipleEditorLines(hint.startRow(), hint.startCol());
    }

    if (hint.updateHint() == CxEditHint::UPDATE_HINT_LINE_PAST_POINT) {
        lineText = formatEditorLine(hint.startRow());
    }

    if (hint.updateHint() == CxEditHint::UPDATE_HINT_LINE) {
        lineText = formatEditorLine(hint.startRow());
    }

    if (programDefaults->liveStatusLine()) updateStatusLine();

    screen->flush();
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

        placeCursor();
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

        placeCursor();
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

        placeCursor();
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
					updateAfterEdit(editHint, lineText);
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
					updateAfterEdit(editHint, lineText);
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
				CxEditHint editHint = editBuffer->addCharacter( keyAction.tag() );
				updateAfterEdit(editHint, lineText);
			}
           	break;

		//-----------------------------------------------------------------------------------------
		// handle enter/return key
		//
		//-----------------------------------------------------------------------------------------
      	case CxKeyAction::NEWLINE:
			{
				editBuffer->addReturn();
                reframe();
                updateScreen();
			}
			break;

		//-----------------------------------------------------------------------------------------
		// handle a backspace action
		//
		//-----------------------------------------------------------------------------------------
     	case CxKeyAction::BACKSPACE:
			{
				CxEditHint editHint = editBuffer->addBackspace();
				updateAfterEdit(editHint, lineText);
			}
			break;

		//-----------------------------------------------------------------------------------------
		// hande a tab action
		//
		//-----------------------------------------------------------------------------------------
       	case CxKeyAction::TAB:
			{
				CxEditHint editHint = editBuffer->addTab();
				updateAfterEdit(editHint, lineText);
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

