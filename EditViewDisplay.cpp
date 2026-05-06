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

    int termWidth = screen->cols();

    screen->placeCursor( _screenStatusRow, 0);

	// set the status bar foreground and background colors
    screen->setForegroundColor(programDefaults->statusBarTextColor() );
    screen->setBackgroundColor(programDefaults->statusBarBackgroundColor() );

    //---------------------------------------------------------------------------------------------
    // Build right-side pieces individually so we can drop them as the terminal narrows.
    // Pieces are added in display order (left to right within the right side) but dropped
    // in reverse priority: col first, then line, then claude, then git, then help hint.
    //---------------------------------------------------------------------------------------------

    char buffer[100];

    // -- help hint --
    CxString helpHintString;
    int helpHintWidth = 0;
#if defined(_OSX_) || defined(_LINUX_)
    helpHintString = " Ctrl+H: Help ";
    helpHintWidth = 14;
#else
    helpHintString = " ESC: Help ";
    helpHintWidth = 11;
#endif

    // -- claude indicator --
    CxString claudeString;
    int claudeWidth = 0;
#if defined(_LINUX_) || defined(_OSX_)
    if (programDefaults->liveStatusLine() && _mcpConnected) {
        claudeString = "[ Claude ] ";
        claudeWidth = 11;
    }
#endif

    // -- line part --
    CxString linePartString;
    int linePartWidth = 0;
    if (programDefaults->liveStatusLine()) {
        sprintf(buffer, "line(%lu,%lu,%.0lf%%)", row + 1, numberOfLines, percent);
        linePartString = buffer;
        linePartWidth = linePartString.length();

        while (linePartWidth < 22) {
            linePartString = CxString(STATUS_FILL) + linePartString;
            linePartWidth++;
        }
    }

    // -- col part --
    CxString colPartString;
    int colPartWidth = 0;
    if (programDefaults->liveStatusLine()) {
        sprintf(buffer, "col(%lu)", col);
        colPartString = buffer;
        colPartWidth = colPartString.length();

        while (colPartWidth < 8) {
            colPartString += STATUS_FILL;
            colPartWidth++;
        }
    } else {
        for (int i = 0; i < 8; i++) {
            colPartString += STATUS_FILL;
        }
        colPartWidth = 8;
    }

    // -- git branch --
    CxString gitString;
    int gitWidth = 0;
#if defined(_LINUX_) || defined(_OSX_)
    if (_gitBranch.length() > 0) {
        gitString = "(git:";
        gitString += _gitBranch;
        gitString += ") ";
        gitWidth = 5 + _gitBranch.length() + 2;
    }
#endif

    //---------------------------------------------------------------------------------------------
    // Build left side: "── cm: Editing [ <filepath> ] "
    //---------------------------------------------------------------------------------------------
    CxString filePath = editBuffer->getFilePath();
    // prefix: 2 fill + " cm: Editing [ " = 3 + 14 = 17,  suffix: " ] " = 3
    int leftFixedWidth = 17 + 3;

    //---------------------------------------------------------------------------------------------
    // Calculate total right-side width, then drop pieces that don't fit.
    // Drop order (least important first): col, line, claude, git, help hint
    //---------------------------------------------------------------------------------------------
    int rightDisplayWidth = helpHintWidth + claudeWidth + linePartWidth
                          + (linePartWidth > 0 && colPartWidth > 0 ? 1 : 0)
                          + colPartWidth;

    int totalWidth = leftFixedWidth + (int) filePath.length() + gitWidth + rightDisplayWidth;

    // drop col part
    if (totalWidth > termWidth && colPartWidth > 0) {
        totalWidth -= colPartWidth;
        if (linePartWidth > 0) totalWidth -= 1;  // the space between line and col
        colPartString = "";
        colPartWidth = 0;
    }

    // drop line part
    if (totalWidth > termWidth && linePartWidth > 0) {
        totalWidth -= linePartWidth;
        linePartString = "";
        linePartWidth = 0;
    }

    // drop claude indicator
    if (totalWidth > termWidth && claudeWidth > 0) {
        totalWidth -= claudeWidth;
        claudeString = "";
        claudeWidth = 0;
    }

    // drop git branch
    if (totalWidth > termWidth && gitWidth > 0) {
        totalWidth -= gitWidth;
        gitString = "";
        gitWidth = 0;
    }

    // drop help hint
    if (totalWidth > termWidth && helpHintWidth > 0) {
        totalWidth -= helpHintWidth;
        helpHintString = "";
        helpHintWidth = 0;
    }

    // truncate filepath as last resort
    if (totalWidth > termWidth) {
        int maxPathLen = termWidth - leftFixedWidth;
        if (maxPathLen < 1) maxPathLen = 1;
        filePath = filePath.subString(0, maxPathLen);
    }

    //---------------------------------------------------------------------------------------------
    // Assemble left side
    //---------------------------------------------------------------------------------------------
    CxString statusLineTextLeft;
    statusLineTextLeft  = STATUS_FILL;
    statusLineTextLeft += STATUS_FILL;
    statusLineTextLeft += " cm: Editing [ ";
    statusLineTextLeft += filePath;
    statusLineTextLeft += " ] ";
    int leftDisplayWidth = leftFixedWidth + filePath.length();

    if (gitWidth > 0) {
        statusLineTextLeft += gitString;
        leftDisplayWidth += gitWidth;
    }

    //---------------------------------------------------------------------------------------------
    // Assemble right side
    //---------------------------------------------------------------------------------------------
    CxString statusLineTextRight;
    rightDisplayWidth = 0;

    if (helpHintWidth > 0) {
        statusLineTextRight += helpHintString;
        rightDisplayWidth += helpHintWidth;
    }
    if (claudeWidth > 0) {
        statusLineTextRight += claudeString;
        rightDisplayWidth += claudeWidth;
    }
    if (linePartWidth > 0) {
        statusLineTextRight += linePartString;
        rightDisplayWidth += linePartWidth;
        if (colPartWidth > 0) {
            statusLineTextRight += " ";
            rightDisplayWidth += 1;
        }
    }
    if (colPartWidth > 0) {
        statusLineTextRight += colPartString;
        rightDisplayWidth += colPartWidth;
    }

    //---------------------------------------------------------------------------------------------
    // Fill between left and right
    //---------------------------------------------------------------------------------------------
    int positionsLeft = termWidth - leftDisplayWidth - rightDisplayWidth;

    CxString theText = statusLineTextLeft;
    for (int c = 0; c < positionsLeft; c++) {
        theText.append(STATUS_FILL);
    }
    theText += statusLineTextRight;


    //---------------------------------------------------------------------------------------------
    // finally write out the status line
    //
    //---------------------------------------------------------------------------------------------
    screen->writeTextAt( _screenStatusRow, 0, theText, false );
    screen->resetColors();
    screen->clearScreenFromCursorToEndOfLine();

    // put cursor back
    screen->placeCursor(
        bufferRowToScreenRow(editBuffer->cursor.row),
        bufferColToScreenCol(editBuffer->cursor.col)
        );

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
    // now colorize the text, consulting the per-line colorization cache first.
    // cache stores the post-colorize visibleText (pre-highlight) keyed by
    // (startCol, visCols, blockState, endsState, generation).
    //---------------------------------------------------------------------------------------------
    if (programDefaults->colorizeSyntax() ) {

        // compute block-comment start/end state for this row (0 on vintage)
#if defined(_LINUX_) || defined(_OSX_)
        int startsInside = isLineInsideBlockComment((int)bufferRow);
        int endsInside = ((int)bufferRow < _blockCommentStateSize) ? _blockCommentState[bufferRow] : 0;
#else
        int startsInside = 0;
        int endsInside = 0;
#endif

        // grow cache to cover this row, then look up
        ensureColorCacheSize((int)bufferRow + 1);
        int startColNow = (int)_visibleFirstEditBufferCol;
        int visColsNow  = (int)_screenEditNumberOfCols;
        int hit = 0;
        if (_colorCache != NULL && (int)bufferRow < _colorCacheSize) {
            ColorCacheEntry *e = &_colorCache[bufferRow];
            if (e->valid &&
                e->startCol == startColNow &&
                e->visCols == visColsNow &&
                e->blockState == startsInside &&
                e->endsState == endsInside &&
                e->generation == _colorCacheGeneration) {
                visibleText = e->bytes;
                hit = 1;
            }
        }

        if (!hit) {
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

            // store into cache
            if (_colorCache != NULL && (int)bufferRow < _colorCacheSize) {
                ColorCacheEntry *e = &_colorCache[bufferRow];
                e->bytes      = visibleText;
                e->startCol   = startColNow;
                e->visCols    = visColsNow;
                e->blockState = startsInside;
                e->endsState  = endsInside;
                e->generation = _colorCacheGeneration;
                e->valid      = 1;
            }
        }
    }

#if defined(_LINUX_) || defined(_OSX_)
    //---------------------------------------------------------------------------------------------
    // apply search highlights on modern platforms
    //
    //---------------------------------------------------------------------------------------------
    visibleText = applySearchHighlights(visibleText, bufferRow, (int)_visibleFirstEditBufferCol);
    visibleText = applySelectionHighlight(visibleText, bufferRow, (int)_visibleFirstEditBufferCol);
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


//-------------------------------------------------------------------------------------------------
// EditView::terminalScrollAndDraw
//
// Use terminal scroll escape sequences to efficiently update the screen when scrolling.
// Instead of redrawing all visible lines, we:
//   1. Set scroll region to this view's edit area
//   2. Scroll the terminal content (existing text shifts)
//   3. Draw only the newly revealed line(s)
//   4. Reset scroll region
//
// Parameters:
//   direction: +1 = scrolling down (content moves up, new lines at bottom)
//              -1 = scrolling up (content moves down, new lines at top)
//   lines:     number of lines to scroll
//
//-------------------------------------------------------------------------------------------------
void
EditView::terminalScrollAndDraw(int direction, int lines)
{
    if (lines <= 0) return;

    // Use synchronized output to eliminate flicker
    CxScreen::beginSyncUpdate();

    //---------------------------------------------------------------------------------------------
    // Set scroll region to just the edit area (not including status bar)
    // CxScreen uses 0-indexed rows internally
    //---------------------------------------------------------------------------------------------
    CxScreen::setScrollRegion((int)_screenEditFirstRow, (int)_screenEditLastRow);

    //---------------------------------------------------------------------------------------------
    // Perform the terminal scroll
    //---------------------------------------------------------------------------------------------
    if (direction > 0) {
        // Scrolling down: content moves up, new blank lines appear at bottom
        CxScreen::scrollUp(lines);
    } else {
        // Scrolling up: content moves down, new blank lines appear at top
        CxScreen::scrollDown(lines);
    }

    //---------------------------------------------------------------------------------------------
    // Reset scroll region before drawing (drawing may use cursor positioning)
    //---------------------------------------------------------------------------------------------
    CxScreen::resetScrollRegion();

    //---------------------------------------------------------------------------------------------
    // Draw only the newly revealed lines
    //---------------------------------------------------------------------------------------------
    CxString newContent;

    if (direction > 0) {
        // Scrolled down - new lines appear at the bottom of the visible area
        // Draw the last 'lines' lines of the visible buffer
        unsigned long startRow = _visibleLastEditBufferRow - (unsigned long)(lines - 1);
        for (int i = 0; i < lines; i++) {
            unsigned long bufferRow = startRow + (unsigned long)i;
            if (bufferRow <= _visibleLastEditBufferRow) {
                newContent += formatEditorLine(bufferRow);
            }
        }
    } else {
        // Scrolled up - new lines appear at the top of the visible area
        // Draw the first 'lines' lines of the visible buffer
        for (int i = 0; i < lines; i++) {
            unsigned long bufferRow = _visibleFirstEditBufferRow + (unsigned long)i;
            if (bufferRow <= _visibleLastEditBufferRow) {
                newContent += formatEditorLine(bufferRow);
            }
        }
    }

    newContent = CxStringUtils::replaceTabExtensionsWithSpaces(newContent);
    fputs(newContent.data(), stdout);

    //---------------------------------------------------------------------------------------------
    // Update status line (gated the same way as cursor-move status refresh) and flush
    //---------------------------------------------------------------------------------------------
    if (programDefaults->liveStatusLine()) updateStatusLine();
    CxScreen::endSyncUpdate();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// EditView::lineNumberPrefix
//
// Build a cursor-position + colored line-number column for the given buffer row.
// Returns an empty string if line numbers are disabled, or just clear-to-EOL if the
// row is past the end of the buffer.
//-------------------------------------------------------------------------------------------------
CxString
EditView::lineNumberPrefix(unsigned long bufferRow)
{
    CxString out = CxCursor::locateTerminalString(bufferRowToScreenRow(bufferRow), 0);

    if (!_showLineNumbers) return out;

    if (bufferRow >= editBuffer->numberOfLines()) {
        // past end of buffer: emit only clear-to-EOL so stale digits get wiped
        out += CxCursor::clearToEndOfLineTerminalString();
        return out;
    }

    CxString num( bufferRow + 1 );
    num = num + "| ";
    while (num.length() < _lineNumberOffset) {
        num = CxString(" ") + num;
    }

    out += programDefaults->lineNumberTextColor()->terminalString();
    out += num;
    out += programDefaults->lineNumberTextColor()->resetTerminalString();

    return out;
}


//-------------------------------------------------------------------------------------------------
// EditView::terminalInsertLineAndDraw
//
// Efficiently handle Enter key using terminal insert line sequence.
// When a line is split, instead of redrawing from that point to bottom:
//   1. Position cursor at the row AFTER the split
//   2. Insert 1 blank line (pushes existing content down)
//   3. Redraw the split line (now truncated) and the new line (wrapped content)
//
// Returns 1 if optimization was used, 0 if caller should do full redraw.
//
//-------------------------------------------------------------------------------------------------
int
EditView::terminalInsertLineAndDraw(unsigned long originalRow)
{
    // Check if both the original row and the new row are visible
    // The new row is originalRow + 1 (where the wrapped content went)
    unsigned long newRow = originalRow + 1;

    if (!rowVisible(originalRow) || !rowVisible(newRow)) {
        return 0;  // Need full redraw - affected rows not visible
    }

    // Use synchronized output to eliminate flicker
    CxScreen::beginSyncUpdate();

    // Get screen coordinates
    unsigned long screenRowForNew = bufferRowToScreenRow(newRow);

    // Set scroll region to edit area (so insert doesn't affect status bar)
    CxScreen::setScrollRegion((int)_screenEditFirstRow, (int)_screenEditLastRow);

    // Position cursor at the screen row where we want to insert
    CxScreen::placeCursor((int)screenRowForNew, 0);

    // Insert one blank line - this pushes all content from here down
    CxScreen::insertLines(1);

    // Reset scroll region before drawing
    CxScreen::resetScrollRegion();

    // Redraw the two affected lines:
    // - originalRow: now contains truncated content (before cursor)
    // - newRow: contains the wrapped content (after cursor)
    CxString content;
    content += formatEditorLine(originalRow);
    content += formatEditorLine(newRow);

    content = CxStringUtils::replaceTabExtensionsWithSpaces(content);
    fputs(content.data(), stdout);

    // Refresh line-number column for every row the terminal shifted down by one.
    // Their content bytes are still correct (moved by CSI L) but their printed
    // line numbers are now off-by-one until we overwrite the prefix.
    if (_showLineNumbers) {
        CxString numRefresh;
        unsigned long numLines = editBuffer->numberOfLines();
        for (unsigned long r = newRow + 1; r <= _visibleLastEditBufferRow; r++) {
            if (r >= numLines) break;
            numRefresh += lineNumberPrefix(r);
        }
        if (numRefresh.length() > 0) {
            fputs(numRefresh.data(), stdout);
        }
    }

    // Update status line (gated — vintage suppresses unless liveStatusLine) and flush
    if (programDefaults->liveStatusLine()) updateStatusLine();
    CxScreen::endSyncUpdate();
    screen->flush();

    return 1;  // Optimization was used
}


//-------------------------------------------------------------------------------------------------
// EditView::terminalDeleteLineAndDraw
//
// Efficiently handle line join (backspace at start of line) using terminal delete line.
// When lines are joined, instead of redrawing from that point to bottom:
//   1. Position cursor at the joined row
//   2. Delete 1 line (pulls existing content up)
//   3. Redraw the joined line (now contains merged content)
//
// Returns 1 if optimization was used, 0 if caller should do full redraw.
//
//-------------------------------------------------------------------------------------------------
int
EditView::terminalDeleteLineAndDraw(unsigned long joinedRow)
{
    // Check if the joined row is visible
    if (!rowVisible(joinedRow)) {
        return 0;  // Need full redraw - affected row not visible
    }

    // Use synchronized output to eliminate flicker
    CxScreen::beginSyncUpdate();

    // Get screen coordinate
    unsigned long screenRow = bufferRowToScreenRow(joinedRow);

    // Set scroll region to edit area
    CxScreen::setScrollRegion((int)_screenEditFirstRow, (int)_screenEditLastRow);

    // Position cursor at the row BELOW the joined row (the line being removed)
    // Actually, we delete at joinedRow + 1 (the old next line position)
    // Wait - after the join, the old "next line" no longer exists in the buffer.
    // The terminal still shows it, so we delete it to pull content up.
    CxScreen::placeCursor((int)screenRow + 1, 0);

    // Delete one line - this pulls all content below up
    CxScreen::deleteLines(1);

    // Reset scroll region before drawing
    CxScreen::resetScrollRegion();

    // Redraw the joined line (contains merged content)
    CxString content = formatEditorLine(joinedRow);
    content = CxStringUtils::replaceTabExtensionsWithSpaces(content);
    fputs(content.data(), stdout);

    // Refresh line-number column for every row the terminal shifted up by one.
    // Their content bytes are still correct (moved by CSI M) but their printed
    // line numbers are now off-by-one (too high) until we overwrite the prefix.
    if (_showLineNumbers) {
        CxString numRefresh;
        unsigned long numLines = editBuffer->numberOfLines();
        for (unsigned long r = joinedRow + 1; r < _visibleLastEditBufferRow; r++) {
            if (r >= numLines) break;
            numRefresh += lineNumberPrefix(r);
        }
        if (numRefresh.length() > 0) {
            fputs(numRefresh.data(), stdout);
        }
    }

    // CSI M blanked the cells at the bottom row of the scroll region. If a
    // buffer row scrolled into view from below, redraw it fully (line number
    // AND text). Without this, repeated joins leave a growing band of
    // blank-but-occupied rows at the bottom: the buffer holds the text but
    // those cells were never repainted. Skip when joinedRow IS the bottom
    // row — we already redrew it above and CSI M was a no-op there.
    if (joinedRow < _visibleLastEditBufferRow) {
        CxString bottomLine = formatEditorLine(_visibleLastEditBufferRow);
        bottomLine = CxStringUtils::replaceTabExtensionsWithSpaces(bottomLine);
        fputs(bottomLine.data(), stdout);
    }

    // Update status line (gated — vintage suppresses unless liveStatusLine) and flush
    if (programDefaults->liveStatusLine()) updateStatusLine();
    CxScreen::endSyncUpdate();
    screen->flush();

    return 1;  // Optimization was used
}


#if defined(_LINUX_) || defined(_OSX_)  // resume modern-only block
//-------------------------------------------------------------------------------------------------
// EditView::screenToBufferPosition
//
// Translates terminal screen coordinates to edit buffer coordinates.
// Returns 1 if the screen position maps to a valid edit area position.
// Returns 0 if the click is outside the edit area (status bar, line numbers, etc.).
//
//-------------------------------------------------------------------------------------------------
int
EditView::screenToBufferPosition(int screenRow, int screenCol,
                                 unsigned long *bufferRow, unsigned long *bufferCol)
{
    if (editBuffer == NULL) return 0;

    // check row is in edit area
    if (screenRow < (int)_screenEditFirstRow) return 0;
    if (screenRow > (int)_screenEditLastRow) return 0;

    // check col is past line number offset
    if (screenCol < (int)_lineNumberOffset) return 0;

    // translate screen row to buffer row
    unsigned long bRow = (unsigned long)screenRow - _screenEditFirstRow + _visibleFirstEditBufferRow;

    // clamp to buffer bounds
    unsigned long numLines = editBuffer->numberOfLines();
    if (numLines == 0) {
        *bufferRow = 0;
        *bufferCol = 0;
        return 1;
    }
    if (bRow >= numLines) {
        bRow = numLines - 1;
    }

    // translate screen col to buffer col
    unsigned long bCol = (unsigned long)(screenCol - (int)_lineNumberOffset) + _visibleFirstEditBufferCol;

    // clamp col to line length
#ifdef CM_UTF8_SUPPORT
    CxUTFString *utfLine = editBuffer->line(bRow);
    if (utfLine != NULL) {
        // for UTF-8 we need display-column-aware clamping
        int displayCol = (int)bCol;
        int charIdx = 0;
        int dc = 0;
        while (charIdx < utfLine->charCount() && dc < displayCol) {
            const CxUTFCharacter *ch = utfLine->at(charIdx);
            dc += ch->displayWidth();
            charIdx++;
        }
        // charIdx is the logical character position
        if ((unsigned long)charIdx > (unsigned long)utfLine->charCount()) {
            bCol = (unsigned long)utfLine->charCount();
        } else {
            bCol = (unsigned long)charIdx;
        }
    } else {
        bCol = 0;
    }
#else
    CxString *linePtr = editBuffer->line(bRow);
    if (linePtr != NULL) {
        if (bCol > linePtr->length()) {
            bCol = linePtr->length();
        }
    } else {
        bCol = 0;
    }
#endif

    *bufferRow = bRow;
    *bufferCol = bCol;
    return 1;
}


//-------------------------------------------------------------------------------------------------
// EditView::applySelectionHighlight
//
// Apply selection highlight (light blue background, dark text) for the range between
// mark and cursor. Follows the same ANSI-escape-aware walk as applySearchHighlights.
//
//-------------------------------------------------------------------------------------------------
CxString
EditView::applySelectionHighlight(CxString text, int bufferRow, int visibleStartCol)
{
    if (editBuffer == NULL) return text;
    if (!_mouseSelectionActive) return text;

    // determine selection range: normalize mark and cursor into start/end
    unsigned long markRow = _selectionMark.row;
    unsigned long markCol = _selectionMark.col;
    unsigned long curRow = editBuffer->cursor.row;
    unsigned long curCol = editBuffer->cursor.col;

    unsigned long startRow, startCol, endRow, endCol;
    if (markRow < curRow || (markRow == curRow && markCol <= curCol)) {
        startRow = markRow;  startCol = markCol;
        endRow = curRow;     endCol = curCol;
    } else {
        startRow = curRow;   startCol = curCol;
        endRow = markRow;    endCol = markCol;
    }

    // if this line is entirely outside the selection range, return unchanged
    if ((unsigned long)bufferRow < startRow || (unsigned long)bufferRow > endRow) {
        return text;
    }

    // determine which visible columns are selected on this line
    int selStartCol, selEndCol;
    int visibleEndCol = visibleStartCol + (int)_screenEditNumberOfCols;

    if ((unsigned long)bufferRow == startRow && (unsigned long)bufferRow == endRow) {
        // selection starts and ends on this line
        selStartCol = (int)startCol - visibleStartCol;
        selEndCol = (int)endCol - visibleStartCol;
    } else if ((unsigned long)bufferRow == startRow) {
        // selection starts on this line, continues to end
        selStartCol = (int)startCol - visibleStartCol;
        selEndCol = (int)_screenEditNumberOfCols;
    } else if ((unsigned long)bufferRow == endRow) {
        // selection ends on this line
        selStartCol = 0;
        selEndCol = (int)endCol - visibleStartCol;
    } else {
        // entire line is selected
        selStartCol = 0;
        selEndCol = (int)_screenEditNumberOfCols;
    }

    // clamp to visible area
    if (selStartCol < 0) selStartCol = 0;
    if (selEndCol > (int)_screenEditNumberOfCols) selEndCol = (int)_screenEditNumberOfCols;
    if (selStartCol >= selEndCol) return text;

    // selection color: light blue bg, dark text (matches ss)
    const char *selOn  = "\033[38;2;40;40;60m\033[48;2;180;200;230m";
    const char *selOff = "\033[0m";

    // walk through colorized text, tracking display column, inject highlight
    CxString result;
    int displayCol = 0;
    int inSelection = 0;
    int textLen = (int)text.length();

    for (int i = 0; i < textLen; i++) {
        char c = text.charAt(i);

        // skip ANSI escape sequences (preserve them as-is)
        if (c == '\033' && i + 1 < textLen && text.charAt(i + 1) == '[') {
            // if we're in selection, close it before the escape, reopen after
            if (inSelection) {
                result += selOff;
            }
            result += c;
            i++;
            while (i < textLen) {
                c = text.charAt(i);
                result += c;
                if (c == 'm') break;
                i++;
            }
            if (inSelection) {
                result += selOn;
            }
            continue;
        }

        // check if entering selection
        if (!inSelection && displayCol >= selStartCol && displayCol < selEndCol) {
            result += selOn;
            inSelection = 1;
        }

        // output the character
        result += c;
        displayCol++;

        // check if leaving selection
        if (inSelection && displayCol >= selEndCol) {
            result += selOff;
            inSelection = 0;
        }
    }

    // close any open selection
    if (inSelection) {
        result += selOff;
    }

    return result;
}
#endif

