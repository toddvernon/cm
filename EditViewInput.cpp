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

    // recompute block comment state from the edit anchor — incremental rescan
    // stops as soon as the running state matches the stored state (see EditView.cpp).
    recomputeBlockCommentState((int)hint.startRow());
#endif

    // invalidate colorization cache based on the hint's scope
    switch (hint.updateHint()) {
        case CxEditHint::UPDATE_HINT_LINE:
        case CxEditHint::UPDATE_HINT_LINE_PAST_POINT:
            invalidateColorCache((int)hint.startRow());
            break;
        case CxEditHint::UPDATE_HINT_SCREEN_PAST_POINT:
        case CxEditHint::UPDATE_HINT_SCREEN:
            bumpColorCacheGeneration();
            break;
        default:
            break;
    }

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
	if (keyAction.tag() == "<arrow-left>" || keyAction.tag() == "<shift-arrow-left>") {

        CxEditHint hint = editBuffer->cursorLeftRequest();

        if (reframe()) {
            updateScreen();
        }

        screen->placeCursor(
            bufferRowToScreenRow(editBuffer->cursor.row),
            bufferColToScreenCol(editBuffer->cursor.col)
            );
	}

	if (keyAction.tag() == "<arrow-right>" || keyAction.tag() == "<shift-arrow-right>") {

		CxEditHint hint = editBuffer->cursorRightRequest();

        if (reframe()) {
            updateScreen();
        }

        placeCursor();
	}

	if (keyAction.tag() == "<arrow-down>" || keyAction.tag() == "<shift-arrow-down>") {

        CxEditHint hint = editBuffer->cursorDownRequest();

        // Use terminal scrolling for efficient scroll on all platforms
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

        placeCursor();
	}

	if (keyAction.tag() == "<arrow-up>" || keyAction.tag() == "<shift-arrow-up>") {

		CxEditHint hint = editBuffer->cursorUpRequest();

        // Use terminal scrolling for efficient scroll on all platforms
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
    // an editing keypress with an active selection replaces the selected text:
    //   - insertion keys (alpha/number/symbol/newline/tab) delete the selection,
    //     then fall through so the new character is inserted in its place
    //   - deletion keys (backspace, option-delete, CTRL-H) delete the selection
    //     and consume the keypress — the selection deletion is the user's intent
    //   - other non-cursor keys just clear the highlight, no buffer change
    // cursor / shift-cursor keys don't clear — they move through / extend the selection
    {
        int aType = keyAction.actionType();
        if (_mouseSelectionActive &&
            aType != CxKeyAction::CURSOR &&
            aType != CxKeyAction::SHIFT_CURSOR &&
            aType != CxKeyAction::COMMAND &&
            aType != CxKeyAction::NOTHING)
        {
            int isInsertion =
                (aType == CxKeyAction::LOWERCASE_ALPHA) ||
                (aType == CxKeyAction::UPPERCASE_ALPHA) ||
                (aType == CxKeyAction::NUMBER) ||
                (aType == CxKeyAction::SYMBOL) ||
                (aType == CxKeyAction::NEWLINE) ||
                (aType == CxKeyAction::TAB);
            int isDeletion =
                (aType == CxKeyAction::BACKSPACE) ||
                ((aType == CxKeyAction::OPTION) && (keyAction.tag() == "<option-delete>")) ||
                ((aType == CxKeyAction::CONTROL) && (keyAction.tag() == "H"));

            if ((isInsertion || isDeletion) && editBuffer != NULL) {
                // cut may span multiple lines — invalidate color cache
                bumpColorCacheGeneration();
                editBuffer->cutToMark();
            }
            // clear flag BEFORE redraw — _selectionMark is now stale after cut
            _mouseSelectionActive = 0;
            updateScreen();

            if (isDeletion) {
                placeCursor();
                if (programDefaults->liveStatusLine()) updateStatusLine();
                screen->flush();
                return( OK );  // selection deletion satisfies the keypress
            }
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

#if defined(_OSX_) || defined(_LINUX_)
		//-----------------------------------------------------------------------------------------
		// shift+arrow extends a selection (modern platforms only). first shift+arrow of a
		// gesture drops the mark at the current cursor; subsequent shift+arrows extend it.
		// reuses the same _selectionMark / _mouseSelectionActive path that mouse drag uses,
		// so the existing highlight renderer paints it and edit-copy / edit-cut work as-is.
		//-----------------------------------------------------------------------------------------
		case CxKeyAction::SHIFT_CURSOR:
			{
                if (editBuffer != NULL) {
                    if (!_mouseSelectionActive) {
                        editBuffer->setMark();
                        _selectionMark = CxEditBufferPosition(
                            editBuffer->cursor.row, editBuffer->cursor.col);
                        _mouseSelectionActive = 1;
                    }
                    handleArrows( keyAction );
                    updateScreen();   // refresh selection highlight as range grows/shrinks
                    placeCursor();
                    if (programDefaults->liveStatusLine()) updateStatusLine();
                    screen->flush();
                }
			}
			break;
#endif

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
                int oldOffset = (int)_lineNumberOffset;
				CxEditHint hint = editBuffer->addReturn();
                recalcLineNumberDigits();
#if defined(_LINUX_) || defined(_OSX_)
                clearSearchMatches();
                recomputeBlockCommentState((int)hint.startRow());
#endif

                // line inserted — rows from startRow onward shift; invalidate cache
                bumpColorCacheGeneration();

                if (oldOffset != (int)_lineNumberOffset) {
                    // line-number column width changed (crossed a power-of-10 boundary);
                    // every line's content column shifts — full repaint is required
                    reframe();
                    updateScreen();
                } else if (!reframe()) {
                    // No scrolling needed - try terminal insert optimization
                    if (!terminalInsertLineAndDraw(hint.startRow())) {
                        // Optimization failed (row not visible) - full redraw
                        updateScreen();
                    }
                } else {
                    // Scrolling was needed - full redraw
                    updateScreen();
                }
			}
			break;

		//-----------------------------------------------------------------------------------------
		// handle a backspace action
		//
		//-----------------------------------------------------------------------------------------
     	case CxKeyAction::BACKSPACE:
			{
                // Check if this will be a line join (cursor at col 0, not at row 0)
                int isLineJoin = (editBuffer->cursor.col == 0 && editBuffer->cursor.row > 0);
                unsigned long joinedRow = editBuffer->cursor.row - 1;
                int oldOffset = (int)_lineNumberOffset;

                CxEditHint editHint = editBuffer->addBackspace();

                if (isLineJoin) {
                    recalcLineNumberDigits();
#if defined(_LINUX_) || defined(_OSX_)
                    clearSearchMatches();
                    recomputeBlockCommentState((int)joinedRow);
#endif

                    // line removed — rows from joinedRow onward shift; invalidate cache
                    bumpColorCacheGeneration();

                    if (oldOffset != (int)_lineNumberOffset) {
                        // line-number column width changed; full repaint required
                        reframe();
                        updateScreen();
                    } else if (!reframe()) {
                        // No scrolling needed - try terminal delete optimization
                        if (!terminalDeleteLineAndDraw(joinedRow)) {
                            // Optimization failed (row not visible) - fall back
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

