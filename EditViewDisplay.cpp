//-------------------------------------------------------------------------------------------------
//
//  EditViewDisplay.cpp
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  Screen rendering and coordinate translation methods for EditView.
//
//-------------------------------------------------------------------------------------------------

#include "EditView.h"


//-------------------------------------------------------------------------------------------------
// Status line fill character - use UTF-8 box drawing on Unix/Mac, '=' elsewhere
//-------------------------------------------------------------------------------------------------
#if defined(_LINUX_) || defined(_OSX_)
static const char *STATUS_FILL = "\xe2\x94\x80";  // ─ (U+2500 BOX DRAWINGS LIGHT HORIZONTAL)
static const int   STATUS_FILL_BYTES = 3;
#else
static const char *STATUS_FILL = "=";
static const int   STATUS_FILL_BYTES = 1;
#endif


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
    statusLineTextLeft  = STATUS_FILL;
    statusLineTextLeft += STATUS_FILL;
    statusLineTextLeft += " cm: Editing [ ";
    statusLineTextLeft += editBuffer->getFilePath();
    statusLineTextLeft += " ] ";

    // Track display width separately from byte length for UTF-8 compatibility
    // The prefix is 2 fill chars + space + text = 3 display columns for "── " or "== "
    int leftDisplayWidth = 3 + 14 + editBuffer->getFilePath().length() + 3;

#if defined(_LINUX_) || defined(_OSX_)
    // Add git branch name if available
    if (_gitBranch.length() > 0) {
        statusLineTextLeft += "(git:";
        statusLineTextLeft += _gitBranch;
        statusLineTextLeft += ") ";
        leftDisplayWidth += 5 + _gitBranch.length() + 2;  // "(git:" + branch + ") "
    }
#endif

    //---------------------------------------------------------------------------------------------
    // do the line part of the status line
    //
    //---------------------------------------------------------------------------------------------

    CxString colPartString;
    CxString statusLineTextRight;
    int rightDisplayWidth = 0;

    if (programDefaults->liveStatusLine()) {

        char buffer[100];

#if defined(_LINUX_) || defined(_OSX_)
        // Add Claude connection indicator on the right side
        if (_mcpConnected) {
            statusLineTextRight += "[ Claude ] ";
            rightDisplayWidth += 11;
        }
#endif

        sprintf(buffer, "line(%lu,%lu,%.0lf%%)", row + 1, numberOfLines, percent);
        CxString linePartString = buffer;
        int linePartDisplayWidth = linePartString.length();

        // Pad linePartString to fixed width (22 chars for max "line(10000,10000,100%)")
        // so [ Claude ] indicator stays in a fixed position
        while (linePartDisplayWidth < 22) {
            linePartString = CxString(STATUS_FILL) + linePartString;
            linePartDisplayWidth++;
        }

        //---------------------------------------------------------------------------------------------
        // do the col part of the status line, we pad the col part so the line of text doesn't
        // jump around between lines with where the cursor is in a different stop
        //
        //---------------------------------------------------------------------------------------------

        sprintf(buffer, "col(%lu)", col);
        colPartString = buffer;
        int colPartDisplayWidth = colPartString.length();

        while (colPartDisplayWidth < 8) {
            colPartString += STATUS_FILL;
            colPartDisplayWidth++;
        }

        statusLineTextRight += linePartString + CxString(" ") + colPartString;
        rightDisplayWidth += 22 + 1 + 8;  // linePartString + space + colPartString

    } else {
        // Default 8-column padding when live status line is disabled
        for (int i = 0; i < 8; i++) {
            colPartString += STATUS_FILL;
        }
        rightDisplayWidth = 8;
    }

    //---------------------------------------------------------------------------------------------
    // Calculate the number of fill characters needed between left and right
    // Use display width (column count) instead of byte length for UTF-8 compatibility
    //
    //---------------------------------------------------------------------------------------------
    int statusLineDisplayWidth = leftDisplayWidth + rightDisplayWidth;
    int positionsLeft = screen->cols() - statusLineDisplayWidth;

    //---------------------------------------------------------------------------------------------
    // Build the full status line with fill characters between left and right parts
    //
    //---------------------------------------------------------------------------------------------
    CxString theText = statusLineTextLeft;
    for (int c = 0; c < positionsLeft; c++) {
        theText.append(STATUS_FILL);
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
// In split screen mode, _screenEditFirstRow offsets the result to the correct region.
//
//-------------------------------------------------------------------------------------------------
unsigned long
EditView::bufferRowToScreenRow(unsigned long bufferRow)
{
    // get the physical screen line, offset by the region start row
    unsigned long screenLine = bufferRow - _visibleFirstEditBufferRow + _screenEditFirstRow;
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
    // check the the logical row is visible return if not
    //
    //---------------------------------------------------------------------------------------------
    if (bufferRow < _visibleFirstEditBufferRow) return("");
    if (bufferRow > _visibleLastEditBufferRow)  return("");

    //---------------------------------------------------------------------------------------------
    // In region mode, ensure we don't draw past the region boundary
    //---------------------------------------------------------------------------------------------
    unsigned long screenRow = bufferRowToScreenRow(bufferRow);
    if (_regionStartRow != -1 || _regionEndRow != -1) {
        // Region mode - check screen row is within bounds
        if (screenRow > _screenEditLastRow) return("");
    }

    //---------------------------------------------------------------------------------------------
    // place the cursor at the correct place on the screen
    //---------------------------------------------------------------------------------------------
    CxString lineNumberString = CxCursor::locateTerminalString(screenRow, 0);

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
        return lineNumberString;
    }
    // Use toBytesExpanded() to expand tabs to spaces for display
    CxString fullText = utfLine->toBytesExpanded();
#else
    CxString *textPtr = editBuffer->line(bufferRow);
    if (textPtr == NULL) {
        return lineNumberString;
    }
    CxString fullText = *textPtr;
#endif

    //---------------------------------------------------------------------------------------------
    // get a copy of the full text line and the visible text line we need to pass both to
    // to the colorize method
    //
    //---------------------------------------------------------------------------------------------
#ifdef CM_UTF8_SUPPORT
    // For UTF-8, extract visible bytes using display column math.
    // toBytesExpanded() converts tabs to spaces, but multi-byte characters
    // still occupy more bytes than display columns, so byte offset != display column.
    int visStartCol = (int)_visibleFirstEditBufferCol;
    int visEndCol   = visStartCol + (int)_screenEditNumberOfCols;

    int displayCol = 0;
    int bytePos    = 0;
    int idx        = 0;

    // Skip characters before the visible area
    while (idx < utfLine->charCount() && displayCol < visStartCol) {
        const CxUTFCharacter *ch = utfLine->at(idx);
        int w = ch->displayWidth();
        int b = ch->isTab() ? w : ch->byteCount();
        displayCol += w;
        bytePos    += b;
        idx++;
    }
    int byteStart = bytePos;

    // Collect characters within the visible area
    while (idx < utfLine->charCount() && displayCol < visEndCol) {
        const CxUTFCharacter *ch = utfLine->at(idx);
        int w = ch->displayWidth();
        int b = ch->isTab() ? w : ch->byteCount();
        displayCol += w;
        bytePos    += b;
        idx++;
    }
    int byteEnd = bytePos;

    CxString visibleText = fullText.subString(byteStart, byteEnd - byteStart);
#else
    CxString visibleText = fullText.subString(_visibleFirstEditBufferCol, _screenEditNumberOfCols );
#endif

    //---------------------------------------------------------------------------------------------
    // now colorize the text
    //
    //---------------------------------------------------------------------------------------------
    if (programDefaults->colorizeSyntax() ) {
#if defined(_LINUX_) || defined(_OSX_)
        // check if line is inside a block comment (modern platforms only)
        // line is part of block comment if:
        //   - it STARTS inside a block comment (previous line ended inside), OR
        //   - it ENDS inside a block comment (this line contains /* that opens one)
        int startsInside = isLineInsideBlockComment((int)bufferRow);
        int endsInside = ((int)bufferRow < _blockCommentStateSize) ? _blockCommentState[bufferRow] : 0;

        if (startsInside || endsInside) {
            // entire line is part of /* */ block comment - apply comment color
            int lang = (int)markUp->getLanguageMode();
            CxString commentColor = programDefaults->commentTextColor(lang)->terminalString();
            if (commentColor.length() == 0) {
                commentColor = programDefaults->commentTextColor()->terminalString();
            }
            CxString resetColor = "\033[0m";
            visibleText = commentColor + visibleText + resetColor;
        } else {
            visibleText = markUp->colorizeText( fullText, visibleText );
        }
#else
        visibleText = markUp->colorizeText( fullText, visibleText );
#endif
    }

#if defined(_LINUX_) || defined(_OSX_)
    //---------------------------------------------------------------------------------------------
    // apply search highlights on modern platforms
    //
    //---------------------------------------------------------------------------------------------
    visibleText = applySearchHighlights(visibleText, bufferRow, (int)_visibleFirstEditBufferCol);
#endif

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

