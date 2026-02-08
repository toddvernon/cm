//-------------------------------------------------------------------------------------------------
//
//  EditViewDisplay.cpp
//  cmacs
//
//  Screen rendering and coordinate translation methods for EditView.
//  Extracted from EditView.cpp to reduce file size.
//
//-------------------------------------------------------------------------------------------------

#include "EditView.h"


//-------------------------------------------------------------------------------------------------
// EditView::updateStatusLine
//
// updates the edit window status line
//
//-------------------------------------------------------------------------------------------------
void
EditView::updateStatusLine(void)
{
    // Skip if flag is set (top view in split mode - ScreenEditor handles divider)
    if (_skipStatusLineUpdate) {
        return;
    }

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

#if defined(_LINUX_) || defined(_OSX_)
        // Add Claude connection indicator on the right side
        if (_mcpConnected) {
            statusLineTextRight += "[ Claude ] ";
        }
#endif

        sprintf(buffer, "line(%lu,%lu,%.0lf%%)", row + 1, numberOfLines, percent);
        CxString linePartString = buffer;

        // Pad linePartString to fixed width (22 chars for max "line(10000,10000,100%)")
        // so [ Claude ] indicator stays in a fixed position
        while (linePartString.length() < 22) {
            linePartString = CxString("=") + linePartString;
        }

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

        statusLineTextRight += linePartString + CxString(" ") + colPartString;

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

