//-------------------------------------------------------------------------------------------------
//
//  BuildView.cpp
//  cmacs
//
//  Modal view for displaying build output with error/warning navigation.
//
//  Created by Todd Vernon on 2/8/26.
//  Copyright (c) 2026 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// BuildView.cpp
//
// Modal dialog that displays build output with streaming updates during build.
// Lines are color-coded by type (error=red, warning=yellow, plain=default).
// Arrow keys navigate, Enter on error line allows goto-file-line.
//
//-------------------------------------------------------------------------------------------------

#include "BuildView.h"

//-------------------------------------------------------------------------------------------------
// Platform-conditional selection indicator
//-------------------------------------------------------------------------------------------------
#if defined(_LINUX_) || defined(_OSX_)
static const char *SELECTION_INDICATOR = "\xe2\x96\xb6";  // UTF-8
#else
static const char *SELECTION_INDICATOR = ">";
#endif


//-------------------------------------------------------------------------------------------------
// BuildView::BuildView (constructor)
//
// Constructs an overlay build output view.
//
//-------------------------------------------------------------------------------------------------
BuildView::BuildView( ProgramDefaults *pd, CxScreen *screenPtr, BuildOutput *bo )
{
    programDefaults = pd;
    screen          = screenPtr;
    buildOutput     = bo;

    // create the box frame for modal display
    frame = new CxBoxFrame(screen);

    // initially not visible
    _visible = 0;

    // spinner starts at 0
    _spinnerIndex = 0;

    // NOTE: No resize callback here - ScreenEditor owns all resize handling

    // recalc where everything should display
    recalcScreenPlacements();
}


//-------------------------------------------------------------------------------------------------
// BuildView::recalcScreenPlacements
//
// Calculate modal bounds using 90% of terminal height and 80% width.
//
//-------------------------------------------------------------------------------------------------
void
BuildView::recalcScreenPlacements(void)
{
    // get screen dimensions
    screenNumberOfLines = screen->rows();
    screenNumberOfCols  = screen->cols();

    // calculate horizontal margins (10% on each side for 80% width)
    int marginCols = (int)(screenNumberOfCols * 0.10);
    int frameLeft  = marginCols;
    int frameRight = screenNumberOfCols - marginCols - 1;

    // ensure minimum width
    int frameWidth = frameRight - frameLeft + 1;
    if (frameWidth < 60) {
        frameLeft  = (screenNumberOfCols - 60) / 2;
        frameRight = frameLeft + 59;
    }

    // Use 90% of terminal height
    // Frame layout: top border + title + separator + content + separator + footer + bottom border = 6 + content
    int totalHeight = (int)(screenNumberOfLines * 0.90);
    if (totalHeight < 10) totalHeight = 10;  // minimum height

    // content lines = total height - 6 frame rows
    screenBuildNumberOfLines = totalHeight - 6;
    if (screenBuildNumberOfLines < 3) screenBuildNumberOfLines = 3;

    // vertical centering (5% margin top and bottom)
    int frameTop    = (screenNumberOfLines - totalHeight) / 2;
    int frameBottom = frameTop + totalHeight - 1;

    // store column info for redraw
    screenBuildNumberOfCols = frameRight - frameLeft - 1;  // content width

    // update the frame bounds
    frame->resize(frameTop, frameLeft, frameBottom, frameRight);

    // content starts after top border, title, and separator (row + 3)
    screenBuildTitleBarLine  = frameTop + 1;  // title is on row 1
    screenBuildFrameLine     = frameTop + 2;  // separator is on row 2
    screenBuildFirstListLine = frameTop + 3;  // content starts on row 3
    screenBuildLastListLine  = frameBottom - 3;  // before footer separator

    // set the first list index visible in the scrolling list
    firstVisibleLineIndex = 0;

    // set the selected item in the list (ensure valid bounds)
    selectedLineIndex = 0;
    if (selectedLineIndex >= buildOutput->lineCount()) {
        selectedLineIndex = buildOutput->lineCount() - 1;
        if (selectedLineIndex < 0) {
            selectedLineIndex = 0;
        }
    }
}


//-------------------------------------------------------------------------------------------------
// BuildView::advanceSpinner
//
// Advance the spinner animation for "Building..." state.
//
//-------------------------------------------------------------------------------------------------
void
BuildView::advanceSpinner(void)
{
    _spinnerIndex = (_spinnerIndex + 1) % BUILD_SPINNER_COUNT;
}


//-------------------------------------------------------------------------------------------------
// BuildView::scrollToEnd
//
// Scroll the view so the last line of output is visible.
// Called when new output arrives during a build.
//
//-------------------------------------------------------------------------------------------------
void
BuildView::scrollToEnd(void)
{
    int lineCount = buildOutput->lineCount();
    if (lineCount == 0) {
        selectedLineIndex = 0;
        firstVisibleLineIndex = 0;
        return;
    }

    // Move selection to the last line
    selectedLineIndex = lineCount - 1;

    // Adjust scroll position so selected line is visible
    // (reframe() will be called in redraw(), but we can set firstVisibleLineIndex here)
    if (selectedLineIndex >= firstVisibleLineIndex + screenBuildNumberOfLines) {
        firstVisibleLineIndex = selectedLineIndex - screenBuildNumberOfLines + 1;
        if (firstVisibleLineIndex < 0) {
            firstVisibleLineIndex = 0;
        }
    }
}


//-------------------------------------------------------------------------------------------------
// BuildView::drawLine
//
// Draw a single line at the given screen row.
//
//-------------------------------------------------------------------------------------------------
void
BuildView::drawLine( int screenRow, int logicalIndex )
{
    int contentLeft  = frame->contentLeft();
    int contentWidth = frame->contentWidth();

    // position cursor at start of content area
    screen->placeCursor(screenRow, contentLeft);

    // if this item exists in the output
    if (logicalIndex < buildOutput->lineCount()) {

        BuildOutputLine *line = buildOutput->lineAt(logicalIndex);
        if (line == NULL) return;

        // get line text
        CxString lineText = line->text;

        // Layout: " X text...padding "
        // where X is indicator for selected, space for unselected
        int indicatorDisplayLen = 3;  // " X " display columns
        int textAreaLen = contentWidth - indicatorDisplayLen - 1;

        // truncate text if needed
        if ((int)lineText.length() > textAreaLen) {
            lineText = lineText.subString(0, textAreaLen - 3);
            lineText += "...";
        }

        // pad text to fill area
        while ((int)lineText.length() < textAreaLen) {
            lineText += " ";
        }

        //---------------------------------------------------------------------------------
        // draw selected item
        //---------------------------------------------------------------------------------
        if (selectedLineIndex == logicalIndex) {

            // set selection colors
            screen->setForegroundColor(programDefaults->statusBarTextColor());
            screen->setBackgroundColor(programDefaults->statusBarBackgroundColor());

            // build the line: " > " + padded_text + " "
            CxString displayLine = " ";
            displayLine += SELECTION_INDICATOR;
            displayLine += " ";
            displayLine += lineText;
            displayLine += " ";

            screen->writeText(displayLine);
            screen->resetColors();

        //---------------------------------------------------------------------------------
        // draw unselected item
        // Note: Color coding by type (error=red, warning=yellow) could be added
        // later by using CxAnsiForegroundColor objects
        //---------------------------------------------------------------------------------
        } else {

            // use modal content colors
            screen->setForegroundColor(programDefaults->modalContentTextColor());
            screen->setBackgroundColor(programDefaults->modalContentBackgroundColor());

            // Prefix with indicator for errors/warnings
            CxString displayLine;
            switch (line->type) {
                case BUILD_LINE_ERROR:
                    displayLine = " ! ";  // error marker
                    break;
                case BUILD_LINE_WARNING:
                    displayLine = " ? ";  // warning marker
                    break;
                default:
                    displayLine = "   ";
                    break;
            }
            displayLine += lineText;
            displayLine += " ";

            screen->writeText(displayLine);
            screen->resetColors();
        }

    //-----------------------------------------------------------------------------------------
    // draw empty line if beyond line count
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


//-------------------------------------------------------------------------------------------------
// BuildView::redraw
//
// Draw centered modal dialog with box frame, title, content, and footer.
//
//-------------------------------------------------------------------------------------------------
void
BuildView::redraw( void )
{
    int cursorRow = 0;

    reframe();

    // get frame content bounds
    int contentLeft  = frame->contentLeft();

    //---------------------------------------------------------------------------------------------
    // set frame colors and draw the frame with title and footer
    //---------------------------------------------------------------------------------------------
    frame->setFrameColor(programDefaults->statusBarTextColor(),
                         programDefaults->statusBarBackgroundColor());

    // build title string based on build state
    CxString title;
    if (buildOutput->isRunning()) {
        title = "Building... ";
        title += BUILD_SPINNER_CHARS[_spinnerIndex];
    } else if (buildOutput->isComplete()) {
        char buf[80];
        int errCount = buildOutput->errorCount();
        int warnCount = buildOutput->warningCount();
        if (errCount == 0 && warnCount == 0) {
            sprintf(buf, "Build Complete (no errors)");
        } else {
            sprintf(buf, "Build: %d error%s, %d warning%s",
                    errCount, errCount == 1 ? "" : "s",
                    warnCount, warnCount == 1 ? "" : "s");
        }
        title = buf;
    } else {
        title = "Build Output";
    }

    // build footer string with keyboard hints
    CxString footer = CxString("[Enter] Goto  [Arrows] Navigate  [Esc] Close");

    frame->drawWithTitleAndFooter(title, footer);

    //---------------------------------------------------------------------------------------------
    // draw the output content
    //---------------------------------------------------------------------------------------------
    for (int c = 0; c < screenBuildNumberOfLines; c++) {

        // get the logical list index
        int logicalItem = firstVisibleLineIndex + c;
        int row = screenBuildFirstListLine + c;

        drawLine(row, logicalItem);

        if (selectedLineIndex == logicalItem) {
            cursorRow = row;
        }
    }

    screen->placeCursor(cursorRow, contentLeft);
    screen->resetColors();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// BuildView::getSelectedLine
//
// Return the currently selected line.
//
//-------------------------------------------------------------------------------------------------
BuildOutputLine *
BuildView::getSelectedLine( void )
{
    return buildOutput->lineAt(selectedLineIndex);
}


//-------------------------------------------------------------------------------------------------
// BuildView::hasSelectedError
//
// Returns 1 if the selected line has file:line info for navigation.
//
//-------------------------------------------------------------------------------------------------
int
BuildView::hasSelectedError( void )
{
    BuildOutputLine *line = buildOutput->lineAt(selectedLineIndex);
    if (line == NULL) {
        return 0;
    }
    return (line->filename.length() > 0 && line->line > 0) ? 1 : 0;
}


//-------------------------------------------------------------------------------------------------
// BuildView::setVisible
//
// Set the visibility state for resize handling.
//
//-------------------------------------------------------------------------------------------------
void
BuildView::setVisible( int visible )
{
    _visible = visible;
}


//-------------------------------------------------------------------------------------------------
// BuildView::routeKeyAction
//
// Keys passed into routeKeyAction are targeted for navigation.
//
//-------------------------------------------------------------------------------------------------
void
BuildView::routeKeyAction( CxKeyAction keyAction )
{
    //---------------------------------------------------------------------------------------------
    // based on the kind of key
    //
    //---------------------------------------------------------------------------------------------
    switch (keyAction.actionType() )
    {
        //-----------------------------------------------------------------------------------------
        // select another line in the list
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
// BuildView::reframe
//
// If selected item isn't visible, move the window.
//
//-------------------------------------------------------------------------------------------------
int
BuildView::reframe( )
{
    int changeMade = 0;

    // safety: ensure firstVisibleLineIndex is never negative
    if (firstVisibleLineIndex < 0) {
        firstVisibleLineIndex = 0;
    }

    // safety: ensure selectedLineIndex is valid
    if (selectedLineIndex < 0) {
        selectedLineIndex = 0;
    }

    while ( selectedLineIndex < firstVisibleLineIndex ) {
        changeMade = 1;
        firstVisibleLineIndex--;
        if (firstVisibleLineIndex < 0) {
            firstVisibleLineIndex = 0;
            break;
        }
    }

    while (selectedLineIndex >= firstVisibleLineIndex + screenBuildNumberOfLines) {
        changeMade = 1;
        firstVisibleLineIndex++;
    }

    if (changeMade) return 1;

    return 0;
}


//-------------------------------------------------------------------------------------------------
// BuildView::handleArrows
//
// Moves the selection according to the arrow direction.
//
//-------------------------------------------------------------------------------------------------
int
BuildView::handleArrows( CxKeyAction keyAction )
{
    if (keyAction.tag() == "<arrow-down>") {

        selectedLineIndex++;

        if (selectedLineIndex >= buildOutput->lineCount()) {
            selectedLineIndex = buildOutput->lineCount() - 1;
            if (selectedLineIndex < 0) {
                selectedLineIndex = 0;
            }
        }

        return 1;

    }

    if (keyAction.tag() == "<arrow-up>") {

        selectedLineIndex--;

        if (selectedLineIndex < 0) {
            selectedLineIndex = 0;
        }

        return 1;
    }

    // Page down
    if (keyAction.tag() == "<page-down>") {
        selectedLineIndex += screenBuildNumberOfLines;

        if (selectedLineIndex >= buildOutput->lineCount()) {
            selectedLineIndex = buildOutput->lineCount() - 1;
            if (selectedLineIndex < 0) {
                selectedLineIndex = 0;
            }
        }

        return 1;
    }

    // Page up
    if (keyAction.tag() == "<page-up>") {
        selectedLineIndex -= screenBuildNumberOfLines;

        if (selectedLineIndex < 0) {
            selectedLineIndex = 0;
        }

        return 1;
    }

    return 0;
}
