//-------------------------------------------------------------------------------------------------
//
//  EditViewInput.cpp
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  Input handling, reframing, and visibility methods for EditView.
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
// Cursor crossed the left edge of the visible window: snap the visible window so the
// cursor sits at the left edge. displayCol is a *display* column.
//
//-------------------------------------------------------------------------------------------------
void
EditView::recalcVisibleBufferFromLeft( unsigned long displayCol )
{
    _visibleFirstEditBufferCol = displayCol;
    _visibleLastEditBufferCol  = _visibleFirstEditBufferCol + _screenEditNumberOfCols;
}


//-------------------------------------------------------------------------------------------------
// EditView::recalcVisibleBufferFromRight
//
// Cursor crossed the right edge of the visible window: shift the visible window right
// just enough that the cursor sits one slot inside the right scroll margin. displayCol
// is a *display* column. Underflow-safe: if the cursor is too far left to need scrolling,
// the window stays pinned to column 0.
//
//-------------------------------------------------------------------------------------------------
void
EditView::recalcVisibleBufferFromRight( unsigned long displayCol )
{
    // We want: _visibleLastEditBufferCol >= displayCol + HORIZ_SCROLL_MARGIN + 1
    // i.e. the cursor's column plus the right margin all sit inside the visible window.
    unsigned long needed = displayCol + (unsigned long)HORIZ_SCROLL_MARGIN + 1;

    if (needed <= _screenEditNumberOfCols) {
        // cursor is close enough to col 0 that no scroll is needed; pin to the left
        _visibleFirstEditBufferCol = 0;
    } else {
        _visibleFirstEditBufferCol = needed - _screenEditNumberOfCols;
    }
    _visibleLastEditBufferCol = _visibleFirstEditBufferCol + _screenEditNumberOfCols;
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
    unsigned long bufferRow  = editBuffer->cursor.row;
    unsigned long displayCol = cmCursorDisplayColumn( editBuffer );

    //---------------------------------------------------------------------------------------------
    // if cursor is visible on the screen, then bail out and don't calculate any
    // thing or redraw
    //---------------------------------------------------------------------------------------------
    if (rowVisible(bufferRow) && colVisible()) {
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

    // if the cursor display column is to the left of the visible part, scroll content right
    if (displayCol < _visibleFirstEditBufferCol) {
        recalcVisibleBufferFromLeft( displayCol );
    }

    // if the cursor display column is at or past the right scroll margin, scroll content left.
    // Note: the threshold matches colVisible() exactly so there is no dead zone.
    if (displayCol + (unsigned long)HORIZ_SCROLL_MARGIN >=
        _visibleFirstEditBufferCol + _screenEditNumberOfCols) {
        recalcVisibleBufferFromRight( displayCol );
    }

    return( TRUE );

}


#if defined(_LINUX_) || defined(_OSX_)
//-------------------------------------------------------------------------------------------------
// EditView::reframeWithScrollInfo
//
// Like reframe() but returns detailed scroll information for terminal scrolling optimization.
// Returns direction (+1 = scrolling down/content up, -1 = scrolling up/content down) and
// the number of lines scrolled.
//
//-------------------------------------------------------------------------------------------------
EditView::ScrollResult
EditView::reframeWithScrollInfo( void )
{
    ScrollResult result;
    result.scrolled = 0;
    result.direction = 0;
    result.lines = 0;

    unsigned long bufferRow  = editBuffer->cursor.row;
    unsigned long displayCol = cmCursorDisplayColumn( editBuffer );

    //---------------------------------------------------------------------------------------------
    // if cursor is visible on the screen, no scrolling needed
    //---------------------------------------------------------------------------------------------
    if (rowVisible(bufferRow) && colVisible()) {
        return result;
    }

    //---------------------------------------------------------------------------------------------
    // remember old visible range to calculate scroll amount
    //---------------------------------------------------------------------------------------------
    unsigned long oldFirstRow = _visibleFirstEditBufferRow;

    //---------------------------------------------------------------------------------------------
    // check vertical scrolling
    //---------------------------------------------------------------------------------------------

    // cursor above visible area - scroll up (content moves down)
    if (bufferRow < _visibleFirstEditBufferRow) {
        recalcVisibleBufferFromTopEditLine( bufferRow );
        result.scrolled = 1;
        result.direction = -1;  // scrolling up
        result.lines = (int)(oldFirstRow - _visibleFirstEditBufferRow);
    }

    // cursor below visible area - scroll down (content moves up)
    if (bufferRow > _visibleLastEditBufferRow) {
        recalcVisibleBufferFromBottomEditLine( bufferRow );
        result.scrolled = 1;
        result.direction = +1;  // scrolling down
        result.lines = (int)(_visibleFirstEditBufferRow - oldFirstRow);
    }

    //---------------------------------------------------------------------------------------------
    // check horizontal scrolling - terminal scroll regions only handle vertical, so any
    // horizontal scroll forces a full redraw (direction = 0).
    //---------------------------------------------------------------------------------------------
    if (displayCol < _visibleFirstEditBufferCol) {
        recalcVisibleBufferFromLeft( displayCol );
        result.scrolled = 1;
        result.direction = 0;  // force full redraw
    }

    if (displayCol + (unsigned long)HORIZ_SCROLL_MARGIN >=
        _visibleFirstEditBufferCol + _screenEditNumberOfCols) {
        recalcVisibleBufferFromRight( displayCol );
        result.scrolled = 1;
        result.direction = 0;  // force full redraw
    }

    return result;
}


//-------------------------------------------------------------------------------------------------
// EditView::reframeJumpWithScrollInfo
//
// Like reframe_jump() but returns detailed scroll information for terminal scrolling.
// Jump scroll moves by half a screen rather than single lines.
//
//-------------------------------------------------------------------------------------------------
EditView::ScrollResult
EditView::reframeJumpWithScrollInfo( void )
{
    ScrollResult result;
    result.scrolled = 0;
    result.direction = 0;
    result.lines = 0;

    unsigned long bufferRow  = editBuffer->cursor.row;
    unsigned long displayCol = cmCursorDisplayColumn( editBuffer );

    //---------------------------------------------------------------------------------------------
    // if cursor is visible on the screen, no scrolling needed
    //---------------------------------------------------------------------------------------------
    if (rowVisible(bufferRow) && colVisible()) {
        return result;
    }

    //---------------------------------------------------------------------------------------------
    // remember old visible range to calculate scroll amount
    //---------------------------------------------------------------------------------------------
    unsigned long oldFirstRow = _visibleFirstEditBufferRow;

    //---------------------------------------------------------------------------------------------
    // check vertical scrolling - jump scroll logic (half screen jumps)
    //---------------------------------------------------------------------------------------------

    // cursor above visible area - scroll up (content moves down)
    if (bufferRow < _visibleFirstEditBufferRow) {
        unsigned long newBufferTargetRow = 0;

        if (bufferRow < (_screenEditNumberOfLines/2)) {
            newBufferTargetRow = 0;
        } else {
            newBufferTargetRow = bufferRow - (_screenEditNumberOfLines/2);
        }

        recalcVisibleBufferFromTopEditLine( newBufferTargetRow );
        result.scrolled = 1;
        result.direction = -1;  // scrolling up
        result.lines = (int)(oldFirstRow - _visibleFirstEditBufferRow);
    }

    // cursor below visible area - scroll down (content moves up)
    if (bufferRow > _visibleLastEditBufferRow) {
        unsigned long newBufferRowTarget = bufferRow + (_screenEditNumberOfLines/2);
        if (newBufferRowTarget > editBuffer->numberOfLines()) {
            newBufferRowTarget = editBuffer->numberOfLines();
        }

        recalcVisibleBufferFromBottomEditLine( newBufferRowTarget );
        result.scrolled = 1;
        result.direction = +1;  // scrolling down
        result.lines = (int)(_visibleFirstEditBufferRow - oldFirstRow);
    }

    //---------------------------------------------------------------------------------------------
    // check horizontal scrolling - jump style (mirrors vertical jump scroll). When the cursor
    // crosses either edge, re-center it horizontally at half the visible width. Terminal scroll
    // regions only handle vertical, so any horizontal scroll forces a full redraw.
    //---------------------------------------------------------------------------------------------
    int needHorizJump = 0;
    if (displayCol < _visibleFirstEditBufferCol) needHorizJump = 1;
    if (displayCol + (unsigned long)HORIZ_SCROLL_MARGIN >=
        _visibleFirstEditBufferCol + _screenEditNumberOfCols) needHorizJump = 1;

    if (needHorizJump) {
        unsigned long half = _screenEditNumberOfCols / 2;
        unsigned long newFirst = (displayCol > half) ? (displayCol - half) : 0;
        _visibleFirstEditBufferCol = newFirst;
        _visibleLastEditBufferCol  = newFirst + _screenEditNumberOfCols;
        result.scrolled = 1;
        result.direction = 0;  // force full redraw
    }

    return result;
}
#endif


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
    unsigned long bufferRow  = editBuffer->cursor.row;
    unsigned long displayCol = cmCursorDisplayColumn( editBuffer );

    //---------------------------------------------------------------------------------------------
    // if cursor is visible on the screen, then bail out and don't calculate any
    // thing or redraw
    //---------------------------------------------------------------------------------------------
    if (rowVisible(bufferRow) && colVisible()) {
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

    //---------------------------------------------------------------------------------------------
    // horizontal jump scroll - mirrors vertical jump scroll. When the cursor crosses either
    // edge, re-center it horizontally at half the visible width.
    //---------------------------------------------------------------------------------------------
    int needHorizJump = 0;
    if (displayCol < _visibleFirstEditBufferCol) needHorizJump = 1;
    if (displayCol + (unsigned long)HORIZ_SCROLL_MARGIN >=
        _visibleFirstEditBufferCol + _screenEditNumberOfCols) needHorizJump = 1;

    if (needHorizJump) {
        unsigned long half = _screenEditNumberOfCols / 2;
        unsigned long newFirst = (displayCol > half) ? (displayCol - half) : 0;
        _visibleFirstEditBufferCol = newFirst;
        _visibleLastEditBufferCol  = newFirst + _screenEditNumberOfCols;
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
// EditView::colVisible
//
// Returns true if the cursor's *display* column is currently inside the visible window,
// accounting for HORIZ_SCROLL_MARGIN reserved on the right edge. Uses display columns
// (not character indices), so double-width characters are handled correctly.
//
// The threshold matches the one used by reframe()/reframe_jump() so there is no dead
// zone where the cursor is "not visible" but no scroll happens.
//
//-------------------------------------------------------------------------------------------------
int
EditView::colVisible( void )
{
    unsigned long dc = cmCursorDisplayColumn( editBuffer );

    if (dc < _visibleFirstEditBufferCol) {
        return( false );
    }
    if (dc + (unsigned long)HORIZ_SCROLL_MARGIN >=
        _visibleFirstEditBufferCol + _screenEditNumberOfCols) {
        return( false );
    }
    return( true );
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
#if defined(_LINUX_) || defined(_OSX_)
    // clear search highlights on any edit - matches may be invalid now
    clearSearchMatches();

    // recompute block comment state from start - line insertions/deletions shift everything
    recomputeBlockCommentState(0);
#endif

    if (reframe()) {
        // Horizontal or vertical scroll happened - need full redraw
        updateScreen();
    } else {
        // No scrolling - use efficient hint-based partial update
        if (hint.updateHint() == CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT) {
            lineText = formatMultipleEditorLines(hint.startRow(), hint.startCol());
        }

        if (hint.updateHint() == CxEditHint::UPDATE_HINT_LINE_PAST_POINT) {
            lineText = formatEditorLine(hint.startRow());
        }

        if (hint.updateHint() == CxEditHint::UPDATE_HINT_LINE) {
            lineText = formatEditorLine(hint.startRow());
        }
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

#if defined(_LINUX_) || defined(_OSX_)
        // Use terminal scrolling for efficient scroll on modern platforms
        if (_jumpScroll) {
            ScrollResult sr = reframeJumpWithScrollInfo();
            if (sr.scrolled) {
                if (sr.direction != 0 && sr.lines > 0 && sr.lines < (int)_screenEditNumberOfLines) {
                    // Can use terminal scroll optimization
                    terminalScrollAndDraw(sr.direction, sr.lines);
                } else {
                    // Horizontal scroll or scroll amount >= screen - full redraw
                    updateScreen();
                }
            }
        } else {
            ScrollResult sr = reframeWithScrollInfo();
            if (sr.scrolled) {
                if (sr.direction != 0 && sr.lines > 0 && sr.lines <= (int)_screenEditNumberOfLines) {
                    // Can use terminal scroll optimization
                    terminalScrollAndDraw(sr.direction, sr.lines);
                } else {
                    // Horizontal scroll or large jump - full redraw
                    updateScreen();
                }
            }
        }
#else
        // Vintage platforms: use simple full redraw
        if (_jumpScroll) {
            if (reframe_jump()) {
                updateScreen();
            }
        } else {
            if (reframe()) {
                updateScreen();
            }
        }
#endif

        placeCursor();
	}

	if (keyAction.tag() == "<arrow-up>") {

		CxEditHint hint = editBuffer->cursorUpRequest();

#if defined(_LINUX_) || defined(_OSX_)
        // Use terminal scrolling for efficient scroll on modern platforms
        if (_jumpScroll) {
            ScrollResult sr = reframeJumpWithScrollInfo();
            if (sr.scrolled) {
                if (sr.direction != 0 && sr.lines > 0 && sr.lines < (int)_screenEditNumberOfLines) {
                    // Can use terminal scroll optimization
                    terminalScrollAndDraw(sr.direction, sr.lines);
                } else {
                    // Horizontal scroll or scroll amount >= screen - full redraw
                    updateScreen();
                }
            }
        } else {
            ScrollResult sr = reframeWithScrollInfo();
            if (sr.scrolled) {
                if (sr.direction != 0 && sr.lines > 0 && sr.lines <= (int)_screenEditNumberOfLines) {
                    // Can use terminal scroll optimization
                    terminalScrollAndDraw(sr.direction, sr.lines);
                } else {
                    // Horizontal scroll or large jump - full redraw
                    updateScreen();
                }
            }
        }
#else
        // Vintage platforms: use simple full redraw
        if (_jumpScroll) {
            if (reframe_jump()) {
                updateScreen();
            }
        } else {
            if (reframe()) {
                updateScreen();
            }
        }
#endif

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

#if defined(_OSX_) || defined(_LINUX_)
    // clear mouse selection on any editing keypress (typing, backspace, etc.)
    // cursor keys don't clear - they just move through the selection
    {
        int aType = keyAction.actionType();
        if (_mouseSelectionActive &&
            aType != CxKeyAction::CURSOR &&
            aType != CxKeyAction::COMMAND &&
            aType != CxKeyAction::NOTHING)
        {
            _mouseSelectionActive = 0;
            updateScreen();  // force full redraw to clear stale highlight on all lines
        }
    }
#endif

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
				CxEditHint hint = editBuffer->addReturn();
#if defined(_LINUX_) || defined(_OSX_)
                clearSearchMatches();
                recomputeBlockCommentState(0);

                // Try efficient terminal insert line if cursor stays visible
                if (!reframe()) {
                    // No scrolling needed - try terminal insert optimization
                    if (!terminalInsertLineAndDraw(hint.startRow())) {
                        // Optimization failed, fall back to full redraw
                        updateScreen();
                    }
                } else {
                    // Scrolling was needed - full redraw
                    updateScreen();
                }
#else
                // Vintage platforms: simple full redraw
                reframe();
                updateScreen();
#endif
			}
			break;

		//-----------------------------------------------------------------------------------------
		// handle a backspace action
		//
		//-----------------------------------------------------------------------------------------
     	case CxKeyAction::BACKSPACE:
			{
#if defined(_LINUX_) || defined(_OSX_)
                // Check if this will be a line join (cursor at col 0, not at row 0)
                int isLineJoin = (editBuffer->cursor.col == 0 && editBuffer->cursor.row > 0);
                unsigned long joinedRow = editBuffer->cursor.row - 1;

                CxEditHint editHint = editBuffer->addBackspace();

                if (isLineJoin) {
                    clearSearchMatches();
                    recomputeBlockCommentState(0);

                    // Try efficient terminal delete line if row stays visible
                    if (!reframe()) {
                        // No scrolling needed - try terminal delete optimization
                        if (!terminalDeleteLineAndDraw(joinedRow)) {
                            // Optimization failed, fall back to normal update
                            updateAfterEdit(editHint, lineText);
                        }
                    } else {
                        // Scrolling was needed - normal update
                        updateAfterEdit(editHint, lineText);
                    }
                } else {
                    // Normal character delete - use standard update
                    updateAfterEdit(editHint, lineText);
                }
#else
                // Vintage platforms: standard behavior
                CxEditHint editHint = editBuffer->addBackspace();
				updateAfterEdit(editHint, lineText);
#endif
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


#if defined(_OSX_) || defined(_LINUX_)
//-------------------------------------------------------------------------------------------------
// EditView::mouseClickAt
//
// Move cursor to the buffer position corresponding to the given screen coordinates.
// Clears the mark (no selection) and updates the display.
//
//-------------------------------------------------------------------------------------------------
void
EditView::mouseClickAt(int screenRow, int screenCol)
{
    if (editBuffer == NULL) return;

    unsigned long bRow, bCol;
    if (!screenToBufferPosition(screenRow, screenCol, &bRow, &bCol)) return;

    editBuffer->cursorGotoRequest(bRow, bCol);
    _mouseSelectionActive = 0;

    reframe();
    updateScreen();
    placeCursor();
    if (programDefaults->liveStatusLine()) updateStatusLine();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// EditView::mouseStartSelection
//
// Set the mark at the given screen position to begin a drag selection.
// Moves cursor there and records mark as the drag anchor.
//
//-------------------------------------------------------------------------------------------------
void
EditView::mouseStartSelection(int screenRow, int screenCol)
{
    if (editBuffer == NULL) return;

    unsigned long bRow, bCol;
    if (!screenToBufferPosition(screenRow, screenCol, &bRow, &bCol)) return;

    editBuffer->cursorGotoRequest(bRow, bCol);
    editBuffer->setMark();  // set editBuffer mark too so cut-to-mark works
    _selectionMark = CxEditBufferPosition(bRow, bCol);
    _mouseSelectionActive = 1;

    reframe();
    placeCursor();
    if (programDefaults->liveStatusLine()) updateStatusLine();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// EditView::mouseDragTo
//
// Update cursor during a drag operation. The selection mark stays fixed.
// The selection is the range between _selectionMark and cursor.
//
//-------------------------------------------------------------------------------------------------
void
EditView::mouseDragTo(int screenRow, int screenCol)
{
    if (editBuffer == NULL) return;

    unsigned long bRow, bCol;
    if (!screenToBufferPosition(screenRow, screenCol, &bRow, &bCol)) return;

    editBuffer->cursorGotoRequest(bRow, bCol);

    reframe();
    updateScreen();
    placeCursor();
    if (programDefaults->liveStatusLine()) updateStatusLine();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// EditView::clearMouseSelection
//
// Clear mouse selection state and redraw.
//
//-------------------------------------------------------------------------------------------------
void
EditView::clearMouseSelection(void)
{
    _mouseSelectionActive = 0;
    updateScreen();
    placeCursor();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// EditView::mouseScroll
//
// Scroll the view by N lines. Negative = up, positive = down.
// Does not move the cursor - just shifts the viewport.
//
//-------------------------------------------------------------------------------------------------
void
EditView::mouseScroll(int lines)
{
    if (editBuffer == NULL) return;

    if (lines < 0) {
        // scroll up
        int amount = -lines;
        if ((unsigned long)amount > _visibleFirstEditBufferRow) {
            amount = (int)_visibleFirstEditBufferRow;
        }
        if (amount > 0) {
            recalcVisibleBufferFromTopEditLine(_visibleFirstEditBufferRow - (unsigned long)amount);
        }
    } else if (lines > 0) {
        // scroll down
        unsigned long maxFirst = editBuffer->numberOfLines();
        if (maxFirst > _screenEditNumberOfLines) {
            maxFirst = maxFirst - _screenEditNumberOfLines;
        } else {
            maxFirst = 0;
        }
        unsigned long newFirst = _visibleFirstEditBufferRow + (unsigned long)lines;
        if (newFirst > maxFirst) {
            newFirst = maxFirst;
        }
        recalcVisibleBufferFromTopEditLine(newFirst);
    }

    updateScreen();
    placeCursor();
    if (programDefaults->liveStatusLine()) updateStatusLine();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// EditView::mouseDoubleClickAt
//
// Select the word at the given screen position. Sets mark at word start, cursor at word end.
// Word characters: a-zA-Z0-9_
//
//-------------------------------------------------------------------------------------------------
void
EditView::mouseDoubleClickAt(int screenRow, int screenCol)
{
    if (editBuffer == NULL) return;

    unsigned long bRow, bCol;
    if (!screenToBufferPosition(screenRow, screenCol, &bRow, &bCol)) return;

    // get the line text
#ifdef CM_UTF8_SUPPORT
    CxUTFString *utfLine = editBuffer->line(bRow);
    if (utfLine == NULL) return;
    CxString lineText = utfLine->toBytes();
    int lineLen = (int)utfLine->charCount();
#else
    CxString *linePtr = editBuffer->line(bRow);
    if (linePtr == NULL) return;
    CxString lineText = *linePtr;
    int lineLen = (int)lineText.length();
#endif

    if (lineLen == 0) return;
    int col = (int)bCol;
    if (col >= lineLen) col = lineLen - 1;
    if (col < 0) return;

    // check if character at position is a word character
    char ch = lineText.charAt(col);
    int isWord = ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') || ch == '_');
    if (!isWord) return;

    // scan left for word start
    int wordStart = col;
    while (wordStart > 0) {
        char c = lineText.charAt(wordStart - 1);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) break;
        wordStart--;
    }

    // scan right for word end
    int wordEnd = col + 1;
    while (wordEnd < lineLen) {
        char c = lineText.charAt(wordEnd);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) break;
        wordEnd++;
    }

    // set selection mark at word start, cursor at word end
    editBuffer->cursorGotoRequest(bRow, (unsigned long)wordStart);
    editBuffer->setMark();  // set editBuffer mark too so cut-to-mark works
    _selectionMark = CxEditBufferPosition(bRow, (unsigned long)wordStart);
    _mouseSelectionActive = 1;
    editBuffer->cursorGotoRequest(bRow, (unsigned long)wordEnd);

    reframe();
    updateScreen();
    placeCursor();
    if (programDefaults->liveStatusLine()) updateStatusLine();
    screen->flush();
}
#endif

