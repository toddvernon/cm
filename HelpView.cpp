//-------------------------------------------------------------------------------------------------
//
//  HelpView.cpp
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  Modal dialog showing help content organized by collapsible sections.
//
//-------------------------------------------------------------------------------------------------

#include "HelpView.h"
#include "CmVersion.h"
#include <stdlib.h>

#if defined(_LINUX_) || defined(_OSX_)
#include <cx/base/utfstring.h>
#endif

//-------------------------------------------------------------------------------------------------
// Platform-conditional expand/collapse indicators
//-------------------------------------------------------------------------------------------------
#if defined(_LINUX_) || defined(_OSX_)
static const char *EXPAND_INDICATOR   = "\xe2\x96\xbc";  // ▼ (UTF-8)
static const char *COLLAPSE_INDICATOR = "\xe2\x96\xb6";  // ▶ (UTF-8)
#else
static const char *EXPAND_INDICATOR   = "v";
static const char *COLLAPSE_INDICATOR = ">";
#endif


//-------------------------------------------------------------------------------------------------
// HelpView::HelpView (constructor)
//
//-------------------------------------------------------------------------------------------------
HelpView::HelpView( ProgramDefaults *pd, CxScreen *screenPtr )
{
    programDefaults = pd;
    screen          = screenPtr;

    // create the box frame for modal display
    frame = new CxBoxFrame(screen);

    // initially not visible
    _visible = 0;
    _helpFileLoaded = 0;

    // NOTE: No resize callback here - ScreenEditor owns all resize handling

    // try to load help file
    loadHelpFile();

    // recalc where everything should display
    recalcScreenPlacements();
}


//-------------------------------------------------------------------------------------------------
// HelpView::findHelpFile
//
// Search for help file in standard locations. Returns TRUE and sets outPath if found.
//
//-------------------------------------------------------------------------------------------------
int
HelpView::findHelpFile( CxString *outPath )
{
    CxString path;

#if defined(_LINUX_) || defined(_OSX_)
    // Modern platforms: try .md first, then .txt

    // 1. CWD - development
    path = "./cm_help.md";
    if (CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_R ||
        CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_RW) {
        *outPath = path;
        return TRUE;
    }
    path = "./cm_help.txt";
    if (CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_R ||
        CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_RW) {
        *outPath = path;
        return TRUE;
    }

    // 2. $HOME/.cm/
    char *home = getenv("HOME");
    if (home != NULL) {
        path = CxString(home) + "/.cm/cm_help.md";
        if (CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_R ||
            CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_RW) {
            *outPath = path;
            return TRUE;
        }
        path = CxString(home) + "/.cm/cm_help.txt";
        if (CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_R ||
            CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_RW) {
            *outPath = path;
            return TRUE;
        }
    }

    // 3. /usr/local/share/cm/
    path = "/usr/local/share/cm/cm_help.md";
    if (CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_R ||
        CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_RW) {
        *outPath = path;
        return TRUE;
    }
    path = "/usr/local/share/cm/cm_help.txt";
    if (CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_R ||
        CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_RW) {
        *outPath = path;
        return TRUE;
    }

#else
    // Old platforms: only try .txt

    // 1. CWD
    path = "./cm_help.txt";
    if (CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_R ||
        CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_RW) {
        *outPath = path;
        return TRUE;
    }

    // 2. $HOME/.cm/
    char *home = getenv("HOME");
    if (home != NULL) {
        path = CxString(home) + "/.cm/cm_help.txt";
        if (CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_R ||
            CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_RW) {
            *outPath = path;
            return TRUE;
        }
    }

    // 3. /usr/local/share/cm/
    path = "/usr/local/share/cm/cm_help.txt";
    if (CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_R ||
        CxFileAccess::checkStatus(path) == CxFileAccess::FOUND_RW) {
        *outPath = path;
        return TRUE;
    }

#endif

    return FALSE;
}


//-------------------------------------------------------------------------------------------------
// HelpView::loadHelpFile
//
// Find and load the help file, parse into sections.
//
//-------------------------------------------------------------------------------------------------
int
HelpView::loadHelpFile( void )
{
    FILE *dbg = fopen("/tmp/helpview_debug.log", "a");
    if (dbg) { fprintf(dbg, "loadHelpFile: start\n"); fflush(dbg); }

    CxString path;

    if (!findHelpFile(&path)) {
        if (dbg) { fprintf(dbg, "loadHelpFile: findHelpFile returned FALSE\n"); fflush(dbg); fclose(dbg); }
        _helpFileLoaded = 0;
        return FALSE;
    }

    if (dbg) { fprintf(dbg, "loadHelpFile: found file at '%s'\n", path.data()); fflush(dbg); }

    parseMarkdown(path);

    if (dbg) { fprintf(dbg, "loadHelpFile: parsed %d sections\n", (int)_sections.entries()); fflush(dbg); }

    rebuildVisibleItems();

    if (dbg) { fprintf(dbg, "loadHelpFile: rebuilt %d visible items\n", (int)_visibleItems.entries()); fflush(dbg); fclose(dbg); }

    _helpFileLoaded = 1;

    return TRUE;
}


//-------------------------------------------------------------------------------------------------
// HelpView::clearSections
//
// Free all section data.
//
//-------------------------------------------------------------------------------------------------
void
HelpView::clearSections( void )
{
    for (int i = 0; i < (int)_sections.entries(); i++) {
        HelpSection *sec = _sections.at(i);
        if (sec != NULL) {
            sec->lines.clearAndDelete();
            delete sec;
        }
    }
    _sections.clear();
}


//-------------------------------------------------------------------------------------------------
// HelpView::parseMarkdown
//
// Parse a markdown file into sections.
// Lines starting with ## become section headers.
// Lines starting with # (single) are skipped (document title).
// All other lines are content within the current section.
// Text before the first ## creates an auto "Overview" section.
//
//-------------------------------------------------------------------------------------------------
void
HelpView::parseMarkdown( CxString filePath )
{
    clearSections();

    CxFile file;
    if (!file.open(filePath, "r")) {
        return;
    }

    HelpSection *currentSection = NULL;
    int hasSeenFirstSection = 0;

    while (!file.eof()) {
        CxString line = file.getUntil('\n');

        // strip trailing \r\n
        line = line.stripTrailing("\r\n");

        // check for ## header (section)
        if (line.length() >= 3 &&
            line.data()[0] == '#' && line.data()[1] == '#' && line.data()[2] == ' ') {

            // create new section
            currentSection = new HelpSection();
            currentSection->title = line.subString(3, line.length() - 3);
            currentSection->isExpanded = 1;  // expanded by default
            _sections.append(currentSection);
            hasSeenFirstSection = 1;
            continue;
        }

        // check for # header (document title) - skip
        if (line.length() >= 2 &&
            line.data()[0] == '#' && line.data()[1] == ' ') {
            continue;
        }

        // content line
        if (!hasSeenFirstSection && line.length() > 0) {
            // auto-create "Overview" section for content before first ##
            currentSection = new HelpSection();
            currentSection->title = "Overview";
            currentSection->isExpanded = 1;  // expanded by default
            _sections.append(currentSection);
            hasSeenFirstSection = 1;
        }

        if (currentSection != NULL) {
            CxString *lineCopy = new CxString(line);
            currentSection->lines.append(lineCopy);
        }
    }
}


//-------------------------------------------------------------------------------------------------
// HelpView::rebuildVisibleItems
//
// Rebuild the flat list of visible items from the section structure.
// Called on construction and whenever expand/collapse state changes.
//
//-------------------------------------------------------------------------------------------------
void
HelpView::rebuildVisibleItems( void )
{
    _visibleItems.clearAndDelete();

    int sectionCount = (int)_sections.entries();

    for (int s = 0; s < sectionCount; s++) {
        HelpSection *sec = _sections.at(s);

        // add section header (always visible)
        HelpViewItem *secItem = new HelpViewItem();
        secItem->type = HELPITEM_SECTION;
        secItem->sectionIndex = s;
        secItem->lineIndex = -1;
        _visibleItems.append(secItem);

        // if expanded, add content lines
        if (sec->isExpanded) {
            for (int ln = 0; ln < (int)sec->lines.entries(); ln++) {
                CxString *lineText = sec->lines.at(ln);

                HelpViewItem *lineItem = new HelpViewItem();
                if (lineText->length() == 0) {
                    lineItem->type = HELPITEM_BLANK;
                } else {
                    lineItem->type = HELPITEM_LINE;
                }
                lineItem->sectionIndex = s;
                lineItem->lineIndex = ln;
                _visibleItems.append(lineItem);
            }
        } else {
            // add blank line after collapsed section (except for last section)
            if (s < sectionCount - 1) {
                HelpViewItem *blankItem = new HelpViewItem();
                blankItem->type = HELPITEM_BLANK;
                blankItem->sectionIndex = -1;
                blankItem->lineIndex = -1;
                _visibleItems.append(blankItem);
            }
        }

    }
}


//-------------------------------------------------------------------------------------------------
// HelpView::recalcScreenPlacements
//
// Calculate centered modal bounds with 10% margins on each side.
// Content height is based on visible item count, clamped to min/max limits.
//
//-------------------------------------------------------------------------------------------------
void
HelpView::recalcScreenPlacements( void )
{
    // get screen dimensions
    screenNumberOfLines = screen->rows();
    screenNumberOfCols  = screen->cols();

    // calculate horizontal margins (10% on each side for wider help text)
    int marginCols = (int)(screenNumberOfCols * 0.10);
    int frameLeft  = marginCols;
    int frameRight = screenNumberOfCols - marginCols - 1;

    // ensure minimum width
    int frameWidth = frameRight - frameLeft + 1;
    if (frameWidth < 40) {
        frameLeft  = (screenNumberOfCols - 40) / 2;
        frameRight = frameLeft + 39;
    }

    // calculate content height - always 80% of terminal height
    // Frame layout: top border + title + separator + content + separator + footer + bottom border = 6 + content
    screenHelpNumberOfLines = (int)(screenNumberOfLines * 0.80) - 6;  // reserve 6 for frame rows
    if (screenHelpNumberOfLines < 5) {
        screenHelpNumberOfLines = 5;
    }

    // total height = content lines + 6 (top, title, sep, content..., sep, footer, bottom)
    int totalHeight = screenHelpNumberOfLines + 6;

    // vertical centering
    int frameTop    = (screenNumberOfLines - totalHeight) / 2;
    int frameBottom = frameTop + totalHeight - 1;

    // store column info for redraw
    screenHelpNumberOfCols = frameRight - frameLeft - 1;  // content width

    // update the frame bounds
    frame->resize(frameTop, frameLeft, frameBottom, frameRight);

    // content starts after top border, title, and separator (row + 3)
    screenHelpTitleBarLine  = frameTop + 1;  // title is on row 1
    screenHelpFrameLine     = frameTop + 2;  // separator is on row 2
    screenHelpFirstListLine = frameTop + 3;  // content starts on row 3
    screenHelpLastListLine  = frameBottom - 3;  // before footer separator

    // set the first list index visible in the scrolling list
    firstVisibleListIndex = 0;

    // set the selected item (start at first selectable item)
    selectedListItemIndex = 0;
    int totalItems = (int)_visibleItems.entries();
    while (selectedListItemIndex < totalItems) {
        HelpViewItemType t = _visibleItems.at(selectedListItemIndex)->type;
        if (t != HELPITEM_SEPARATOR) break;
        selectedListItemIndex++;
    }
    if (selectedListItemIndex >= totalItems && totalItems > 0) {
        selectedListItemIndex = 0;
    }
}


//-------------------------------------------------------------------------------------------------
// HelpView::redraw
//
// Draw centered modal dialog with box frame, title, section content, and footer.
//
//-------------------------------------------------------------------------------------------------
void
HelpView::redraw( void )
{
    FILE *dbg = fopen("/tmp/helpview_debug.log", "a");
    if (dbg) { fprintf(dbg, "redraw: start, screenHelpNumberOfLines=%d, visibleItems=%d\n",
               screenHelpNumberOfLines, (int)_visibleItems.entries()); fflush(dbg); }

    int cursorRow = 0;

    reframe();

    // get frame content bounds
    int contentLeft  = frame->contentLeft();
    int contentWidth = frame->contentWidth();

    if (dbg) { fprintf(dbg, "redraw: contentLeft=%d, contentWidth=%d\n", contentLeft, contentWidth); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // set frame colors and draw the frame with title and footer
    //---------------------------------------------------------------------------------------------
    frame->setFrameColor(programDefaults->statusBarTextColor(),
                         programDefaults->statusBarBackgroundColor());

    // build context-sensitive footer based on current selection
    CxString footer = getContextFooter();

    CxString title = "cmacs ";
    title += CM_VERSION;
    frame->drawWithTitleAndFooter(title, footer);

    //---------------------------------------------------------------------------------------------
    // draw the visible items
    //---------------------------------------------------------------------------------------------
    for (int c = 0; c < screenHelpNumberOfLines; c++) {

        // get the logical list index
        int logicalItem = firstVisibleListIndex + c;
        int row = screenHelpFirstListLine + c;

        // position cursor at start of content area
        screen->placeCursor(row, contentLeft);

        // if this item exists in the visible list
        if (logicalItem < (int)_visibleItems.entries()) {

            HelpViewItem *item = _visibleItems.at(logicalItem);

            //-------------------------------------------------------------------------------------
            // build the display text based on item type
            //-------------------------------------------------------------------------------------
            CxString prefix;
            int prefixDisplayWidth = 0;
            CxString text;
            int textExtraBytes = 0;  // bytes beyond display columns for multi-byte chars

            switch (item->type) {

                case HELPITEM_SECTION:
                {
                    HelpSection *sec = _sections.at(item->sectionIndex);
                    prefix = " ";
                    if (sec->isExpanded) {
                        prefix += EXPAND_INDICATOR;
                    } else {
                        prefix += COLLAPSE_INDICATOR;
                    }
                    prefix += " ";
                    prefixDisplayWidth = 3;
                    // Note: UTF-8 indicator is in prefix, not text, so textExtraBytes stays 0
                    text = sec->title;
                }
                break;

                case HELPITEM_LINE:
                {
                    HelpSection *sec = _sections.at(item->sectionIndex);
                    prefix = "   ";
                    prefixDisplayWidth = 3;
                    text = *(sec->lines.at(item->lineIndex));
                }
                break;

                case HELPITEM_BLANK:
                {
                    prefix = "";
                    prefixDisplayWidth = 0;
                    text = "";
                }
                break;

                case HELPITEM_SEPARATOR:
                {
                    prefix = "";
                    prefixDisplayWidth = 0;
                    text = "";
#if defined(_LINUX_) || defined(_OSX_)
                    for (int i = 0; i < contentWidth; i++) {
                        text += "\xe2\x94\x80";  // ─ (U+2500)
                    }
#else
                    for (int i = 0; i < contentWidth; i++) {
                        text += "-";
                    }
#endif
                }
                break;
            }

            //-------------------------------------------------------------------------------------
            // compute layout
            //-------------------------------------------------------------------------------------
            int textAreaLen = contentWidth - prefixDisplayWidth - 1;

            // for separator, skip normal text layout; blank lines just pad with spaces
            if (item->type == HELPITEM_BLANK) {
                // fill blank line with spaces for proper selection highlighting
                for (int i = 0; i < contentWidth - 1; i++) {
                    text += " ";
                }
            } else if (item->type != HELPITEM_SEPARATOR) {
#if defined(_LINUX_) || defined(_OSX_)
                // UTF-8 aware: use display width for layout, not byte length
                CxUTFString utfText;
                utfText.fromCxString(text, 1);
                int displayWidth = utfText.displayWidth();

                if (displayWidth > textAreaLen) {
                    // truncate to textAreaLen - 3 display columns, then add "..."
                    int targetCols = textAreaLen - 3;
                    int cols = 0;
                    int bytePos = 0;
                    for (int ci = 0; ci < utfText.charCount(); ci++) {
                        const CxUTFCharacter *ch = utfText.at(ci);
                        int w = ch->displayWidth();
                        if (cols + w > targetCols) break;
                        cols += w;
                        bytePos += ch->isTab() ? 1 : ch->byteCount();
                    }
                    text = text.subString(0, bytePos);
                    text += "...";
                    displayWidth = cols + 3;
                }

                while (displayWidth < textAreaLen) {
                    text += " ";
                    displayWidth++;
                }
#else
                if ((int)text.length() - textExtraBytes > textAreaLen) {
                    text = text.subString(0, textAreaLen - 3 + textExtraBytes);
                    text += "...";
                }

                while ((int)text.length() < textAreaLen + textExtraBytes) {
                    text += " ";
                }
#endif
            }

            //-------------------------------------------------------------------------------------
            // draw with selection highlight or normal colors
            //-------------------------------------------------------------------------------------
            int isSelected = (selectedListItemIndex == logicalItem);
            int isSeparator = (item->type == HELPITEM_SEPARATOR);
            int isBlank = (item->type == HELPITEM_BLANK);
            int isSection = (item->type == HELPITEM_SECTION);

            if (isSelected && !isSeparator) {
                screen->setForegroundColor(programDefaults->statusBarTextColor());
                screen->setBackgroundColor(programDefaults->statusBarBackgroundColor());
            } else if (isSection) {
                // use markdown keyword color for section titles
                screen->setForegroundColor(programDefaults->keywordTextColor(14));
                screen->setBackgroundColor(programDefaults->modalContentBackgroundColor());
            } else {
                screen->setForegroundColor(programDefaults->modalContentTextColor());
                screen->setBackgroundColor(programDefaults->modalContentBackgroundColor());
            }

            // draw the line
            CxString line = prefix;
            line += text;
            if (!isSeparator) {
                line += " ";
            }
            screen->writeText(line);

            screen->resetColors();

            if (isSelected && !isSeparator) {
                cursorRow = row;
            }

        //-----------------------------------------------------------------------------------------
        // draw empty line if beyond visible items
        //-----------------------------------------------------------------------------------------
        } else {
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

    if (dbg) { fprintf(dbg, "redraw: done, flushing screen\n"); fflush(dbg); fclose(dbg); }

    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// HelpView::getSelectedItemType
//
// Return the type of the currently selected item.
//
//-------------------------------------------------------------------------------------------------
HelpViewItemType
HelpView::getSelectedItemType( void )
{
    if (selectedListItemIndex < 0 || selectedListItemIndex >= (int)_visibleItems.entries()) {
        return HELPITEM_SECTION;
    }
    return _visibleItems.at(selectedListItemIndex)->type;
}


//-------------------------------------------------------------------------------------------------
// HelpView::getContextFooter
//
// Build a context-sensitive footer string based on the currently selected item.
//
//-------------------------------------------------------------------------------------------------
CxString
HelpView::getContextFooter( void )
{
    HelpViewItemType selType = getSelectedItemType();

    // section header
    if (selType == HELPITEM_SECTION) {
        return "[Enter] Expand/Collapse  [Esc] Close";
    }

    // content line or other
    return "[Esc] Close";
}


//-------------------------------------------------------------------------------------------------
// HelpView::toggleSelectedSection
//
// Toggle expand/collapse of the selected section header and rebuild visible items.
//
//-------------------------------------------------------------------------------------------------
void
HelpView::toggleSelectedSection( void )
{
    if (selectedListItemIndex < 0 || selectedListItemIndex >= (int)_visibleItems.entries()) {
        return;
    }

    HelpViewItem *item = _visibleItems.at(selectedListItemIndex);

    if (item->type != HELPITEM_SECTION) {
        return;
    }

    HelpSection *sec = _sections.at(item->sectionIndex);
    if (sec == NULL) {
        return;
    }

    sec->isExpanded = !sec->isExpanded;
    rebuildVisibleItems();

    // ensure selected index is still valid
    if (selectedListItemIndex >= (int)_visibleItems.entries()) {
        selectedListItemIndex = (int)_visibleItems.entries() - 1;
        if (selectedListItemIndex < 0) {
            selectedListItemIndex = 0;
        }
    }
}


//-------------------------------------------------------------------------------------------------
// HelpView::setVisible
//
// Set the visibility state for resize handling.
//
//-------------------------------------------------------------------------------------------------
void
HelpView::setVisible( int visible )
{
    _visible = visible;
}


//-------------------------------------------------------------------------------------------------
// HelpView::routeKeyAction
//
// Handle keyboard actions for navigation.
//
//-------------------------------------------------------------------------------------------------
void
HelpView::routeKeyAction( CxKeyAction keyAction )
{
    switch (keyAction.actionType())
    {
        case CxKeyAction::CURSOR:
        {
            if (handleArrows(keyAction)) {
                redraw();
            }

            screen->flush();
        }
        break;

        default:
            break;
    }

    return;
}


//-------------------------------------------------------------------------------------------------
// HelpView::reframe
//
// If selected item isn't visible, adjust the scroll window.
//
//-------------------------------------------------------------------------------------------------
int
HelpView::reframe( void )
{
    int changeMade = false;

    // safety: ensure firstVisibleListIndex is never negative
    if (firstVisibleListIndex < 0) {
        firstVisibleListIndex = 0;
    }

    // safety: ensure selectedListItemIndex is valid
    if (selectedListItemIndex < 0) {
        selectedListItemIndex = 0;
    }

    while (selectedListItemIndex < firstVisibleListIndex) {
        changeMade = true;
        firstVisibleListIndex--;
        if (firstVisibleListIndex < 0) {
            firstVisibleListIndex = 0;
            break;
        }
    }

    while (selectedListItemIndex >= firstVisibleListIndex + screenHelpNumberOfLines) {
        changeMade = true;
        firstVisibleListIndex++;
    }

    if (changeMade) return(true);

    return(false);
}


//-------------------------------------------------------------------------------------------------
// HelpView::handleArrows
//
// Move selection up/down through the visible items list.
// Skips non-selectable items (SEPARATOR, BLANK).
//
//-------------------------------------------------------------------------------------------------
int
HelpView::handleArrows( CxKeyAction keyAction )
{
    int totalItems = (int)_visibleItems.entries();
    int prevIndex = selectedListItemIndex;

    if (keyAction.tag() == "<arrow-down>") {

        selectedListItemIndex++;

        // skip non-selectable items (only SEPARATOR is non-selectable)
        while (selectedListItemIndex < totalItems) {
            HelpViewItemType t = _visibleItems.at(selectedListItemIndex)->type;
            if (t != HELPITEM_SEPARATOR) break;
            selectedListItemIndex++;
        }

        // if we ran off the end, stay put
        if (selectedListItemIndex >= totalItems) {
            selectedListItemIndex = prevIndex;
        }

        return(true);
    }

    if (keyAction.tag() == "<arrow-up>") {

        selectedListItemIndex--;

        // skip non-selectable items (only SEPARATOR is non-selectable)
        while (selectedListItemIndex >= 0) {
            HelpViewItemType t = _visibleItems.at(selectedListItemIndex)->type;
            if (t != HELPITEM_SEPARATOR) break;
            selectedListItemIndex--;
        }

        // if we ran past the top, stay put
        if (selectedListItemIndex < 0) {
            selectedListItemIndex = prevIndex;
        }

        return(true);
    }

    return(false);
}
