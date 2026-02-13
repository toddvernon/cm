//-------------------------------------------------------------------------------------------------
//
//  ProjectView.cpp
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  Modal dialog showing project structure organized by subproject with
//  collapsible headers and file navigation.
//
//-------------------------------------------------------------------------------------------------

#include "ProjectView.h"

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
// ProjectView::ProjectView (constructor)
//
//-------------------------------------------------------------------------------------------------
ProjectView::ProjectView( ProgramDefaults *pd, CmEditBufferList *ebl, Project *proj, CxScreen *screenPtr )
{
    programDefaults = pd;
    editBufferList  = ebl;
    screen          = screenPtr;
    project         = proj;

    // create the box frame for modal display
    frame = new CxBoxFrame(screen);

    // initially not visible
    _visible = 0;

    // "Other Files" section starts expanded
    _otherFilesExpanded = 1;

    // NOTE: No resize callback here - ScreenEditor owns all resize handling

    // build the visible items from project structure
    rebuildVisibleItems();

    // recalc where everything should display
    recalcScreenPlacements();
}


//-------------------------------------------------------------------------------------------------
// ProjectView::rebuildVisibleItems
//
// Rebuild the flat list of visible items from the project subproject structure.
// Called on construction and whenever expand/collapse state changes.
//
//-------------------------------------------------------------------------------------------------
void
ProjectView::rebuildVisibleItems( void )
{
    _visibleItems.clearAndDelete();

    // build "Other Files" section first (non-project buffers at top)
    int openFilesAdded = 0;
    for (int i = 0; i < editBufferList->items(); i++) {
        CmEditBuffer *buf = editBufferList->at(i);
        if (buf == NULL) continue;

        CxString path = buf->getFilePath();
        if (path.length() == 0) continue;

        // skip buffers that belong to a project subproject
        if (project->subprojectCount() > 0 && isProjectFilePath(path)) {
            continue;
        }

        // add header on first non-project buffer
        if (!openFilesAdded) {
            ProjectViewItem *hdrItem = new ProjectViewItem();
            hdrItem->type = PVITEM_OPEN_HEADER;
            hdrItem->subprojectIndex = -1;
            hdrItem->fileIndex = -1;
            hdrItem->bufferIndex = -1;
            _visibleItems.append(hdrItem);
            openFilesAdded = 1;
        }

        // only add file items if expanded
        if (_otherFilesExpanded) {
            ProjectViewItem *openItem = new ProjectViewItem();
            openItem->type = PVITEM_OPEN_FILE;
            openItem->subprojectIndex = -1;
            openItem->fileIndex = -1;
            openItem->bufferIndex = i;
            _visibleItems.append(openItem);
        }
    }

    if (project->subprojectCount() > 0) {

        // add separator between open files and project sections
        if (openFilesAdded) {
            ProjectViewItem *sepItem = new ProjectViewItem();
            sepItem->type = PVITEM_SEPARATOR;
            sepItem->subprojectIndex = -1;
            sepItem->fileIndex = -1;
            sepItem->bufferIndex = -1;
            _visibleItems.append(sepItem);
        }

        // add "All" row
        ProjectViewItem *allItem = new ProjectViewItem();
        allItem->type = PVITEM_ALL;
        allItem->subprojectIndex = -1;
        allItem->fileIndex = -1;
        allItem->bufferIndex = -1;
        _visibleItems.append(allItem);

        // add subprojects and their files
        for (int s = 0; s < project->subprojectCount(); s++) {
            ProjectSubproject *sub = project->subprojectAt(s);

            // add subproject header
            ProjectViewItem *subItem = new ProjectViewItem();
            subItem->type = PVITEM_SUBPROJECT;
            subItem->subprojectIndex = s;
            subItem->fileIndex = -1;
            subItem->bufferIndex = -1;
            _visibleItems.append(subItem);

            // if expanded, add files
            if (sub->isExpanded) {
                for (int f = 0; f < (int)sub->files.entries(); f++) {
                    ProjectViewItem *fileItem = new ProjectViewItem();
                    fileItem->type = PVITEM_FILE;
                    fileItem->subprojectIndex = s;
                    fileItem->fileIndex = f;
                    fileItem->bufferIndex = -1;
                    _visibleItems.append(fileItem);
                }
            }
        }
    }
}


//-------------------------------------------------------------------------------------------------
// ProjectView::isProjectFilePath
//
// Check if a file path belongs to any subproject in the project.
// Returns 1 if the path matches a resolved project file, 0 otherwise.
//
//-------------------------------------------------------------------------------------------------
int
ProjectView::isProjectFilePath( CxString path )
{
    for (int s = 0; s < project->subprojectCount(); s++) {
        ProjectSubproject *sub = project->subprojectAt(s);
        for (int f = 0; f < (int)sub->files.entries(); f++) {
            CxString resolved = project->resolveFilePath(sub, sub->files.at(f));
            if (resolved == path) {
                return 1;
            }
        }
    }
    return 0;
}


//-------------------------------------------------------------------------------------------------
// ProjectView::subprojectHasModifiedFile
//
// Check if any file in the subproject has unsaved changes.
// Returns 1 if at least one file is modified, 0 otherwise.
//
//-------------------------------------------------------------------------------------------------
int
ProjectView::subprojectHasModifiedFile( ProjectSubproject *sub )
{
    for (int f = 0; f < (int)sub->files.entries(); f++) {
        CxString resolved = project->resolveFilePath(sub, sub->files.at(f));
        CmEditBuffer *buf = editBufferList->findPath(resolved);
        if (buf != NULL && buf->isTouched()) {
            return 1;
        }
    }
    return 0;
}


//-------------------------------------------------------------------------------------------------
// ProjectView::recalcScreenPlacements
//
// Calculate centered modal bounds with 15% margins on each side.
// Content height is based on visible item count, clamped to min/max limits.
//
//-------------------------------------------------------------------------------------------------
void
ProjectView::recalcScreenPlacements(void)
{
    // get screen dimensions
    screenNumberOfLines = screen->rows();
    screenNumberOfCols  = screen->cols();

    // calculate horizontal margins (15% on each side)
    int marginCols = (int)(screenNumberOfCols * 0.15);
    int frameLeft  = marginCols;
    int frameRight = screenNumberOfCols - marginCols - 1;

    // ensure minimum width
    int frameWidth = frameRight - frameLeft + 1;
    if (frameWidth < 40) {
        frameLeft  = (screenNumberOfCols - 40) / 2;
        frameRight = frameLeft + 39;
    }

    // calculate content height
    // Frame layout: top border + title + separator + content + separator + footer + bottom border = 6 + content
    int minItems = 5;
    int maxItems = (int)(screenNumberOfLines * 0.6) - 6;  // reserve 6 for frame rows
    if (maxItems < minItems) maxItems = minItems;

    int itemCount = (int)_visibleItems.entries();
    if (itemCount < minItems) {
        screenProjectNumberOfLines = minItems;
    } else if (itemCount > maxItems) {
        screenProjectNumberOfLines = maxItems;
    } else {
        screenProjectNumberOfLines = itemCount;
    }

    // total height = content lines + 6 (top, title, sep, content..., sep, footer, bottom)
    int totalHeight = screenProjectNumberOfLines + 6;

    // vertical centering
    int frameTop    = (screenNumberOfLines - totalHeight) / 2;
    int frameBottom = frameTop + totalHeight - 1;

    // store column info for redraw
    screenProjectNumberOfCols = frameRight - frameLeft - 1;  // content width

    // update the frame bounds
    frame->resize(frameTop, frameLeft, frameBottom, frameRight);

    // content starts after top border, title, and separator (row + 3)
    screenProjectTitleBarLine  = frameTop + 1;  // title is on row 1
    screenProjectFrameLine     = frameTop + 2;  // separator is on row 2
    screenProjectFirstListLine = frameTop + 3;  // content starts on row 3
    screenProjectLastListLine  = frameBottom - 3;  // before footer separator

    // set the first list index visible in the scrolling list
    firstVisibleListIndex = 0;

    // set the selected item (start at first selectable item)
    selectedListItemIndex = 0;
    int totalItems = (int)_visibleItems.entries();
    while (selectedListItemIndex < totalItems) {
        ProjectViewItemType t = _visibleItems.at(selectedListItemIndex)->type;
        if (t != PVITEM_SEPARATOR) break;
        selectedListItemIndex++;
    }
    if (selectedListItemIndex >= totalItems && totalItems > 0) {
        selectedListItemIndex = 0;
    }
}


//-------------------------------------------------------------------------------------------------
// ProjectView::redraw
//
// Draw centered modal dialog with box frame, title, grouped subproject content, and footer.
//
//-------------------------------------------------------------------------------------------------
void
ProjectView::redraw( void )
{
    int cursorRow = 0;

    reframe();

    // get frame content bounds
    int contentLeft  = frame->contentLeft();
    int contentWidth = frame->contentWidth();

    //---------------------------------------------------------------------------------------------
    // set frame colors and draw the frame with title and footer
    //---------------------------------------------------------------------------------------------
    frame->setFrameColor(programDefaults->statusBarTextColor(),
                         programDefaults->statusBarBackgroundColor());

    // build title string
    CxString title;
    CxString projName = project->projectName();
    if (projName.length() > 0) {
        title = CxString("Project: ") + projName;
    } else if (project->subprojectCount() == 0) {
        title = "Other Files";
    } else {
        title = "Project";
    }

    // build context-sensitive footer based on current selection
    CxString footer = getContextFooter();

    frame->drawWithTitleAndFooter(title, footer);

    // tag colors - bright_red for modified, cyan for in-memory
    CxAnsiForegroundColor tagModifiedColor("bright_red");
    CxAnsiForegroundColor tagInMemoryColor("cyan");

    //---------------------------------------------------------------------------------------------
    // draw the visible items
    //---------------------------------------------------------------------------------------------
    for (int c = 0; c < screenProjectNumberOfLines; c++) {

        // get the logical list index
        int logicalItem = firstVisibleListIndex + c;
        int row = screenProjectFirstListLine + c;

        // position cursor at start of content area
        screen->placeCursor(row, contentLeft);

        // if this item exists in the visible list
        if (logicalItem < (int)_visibleItems.entries()) {

            ProjectViewItem *item = _visibleItems.at(logicalItem);

            //-------------------------------------------------------------------------------------
            // build the display text and tag based on item type
            //-------------------------------------------------------------------------------------
            CxString prefix;
            int prefixDisplayWidth;
            CxString text;
            int textExtraBytes = 0;  // bytes beyond display columns for multi-byte chars
            CxString tagMod;    // "/modified" or empty
            CxString tagMem;    // "/in-memory" or empty

            switch (item->type) {

                case PVITEM_ALL:
                    prefix = "   ";
                    prefixDisplayWidth = 3;
#if defined(_LINUX_) || defined(_OSX_)
                    text = "All \xf0\x9f\x8e\xaf";  // 🎯
                    textExtraBytes = 2;  // 4 bytes, 2 display columns
#else
                    text = "All (target)";
#endif
                    break;

                case PVITEM_SUBPROJECT:
                {
                    ProjectSubproject *sub = project->subprojectAt(item->subprojectIndex);
                    prefix = " ";
                    if (sub->isExpanded) {
                        prefix += EXPAND_INDICATOR;
                    } else {
                        prefix += COLLAPSE_INDICATOR;
                    }
                    prefix += " ";
                    prefixDisplayWidth = 3;

                    text = sub->name;
#if defined(_LINUX_) || defined(_OSX_)
                    text += " \xf0\x9f\x8e\xaf";  // 🎯
                    textExtraBytes = 2;
#else
                    text += " (target)";
#endif

                    if (subprojectHasModifiedFile(sub)) {
                        tagMod = "/modified";
                    }
                }
                break;

                case PVITEM_FILE:
                {
                    ProjectSubproject *sub = project->subprojectAt(item->subprojectIndex);
                    prefix = "     ";
                    prefixDisplayWidth = 5;
                    text = sub->files.at(item->fileIndex);

                    CxString resolved = project->resolveFilePath(sub, sub->files.at(item->fileIndex));
                    CmEditBuffer *buf = editBufferList->findPath(resolved);
                    if (buf != NULL) {
                        if (buf->isTouched()) {
                            tagMod = "/modified";
                        }
                        if (buf->isInMemory()) {
                            tagMem = "/in-memory";
                        }
                    }
                }
                break;

                case PVITEM_OPEN_HEADER:
                {
                    prefix = " ";
                    if (_otherFilesExpanded) {
                        prefix += EXPAND_INDICATOR;
                    } else {
                        prefix += COLLAPSE_INDICATOR;
                    }
                    prefix += " ";
                    prefixDisplayWidth = 3;
                    text = "Other Files";
                }
                break;

                case PVITEM_OPEN_FILE:
                {
                    prefix = "     ";
                    prefixDisplayWidth = 5;

                    CmEditBuffer *buf = editBufferList->at(item->bufferIndex);
                    if (buf != NULL) {
                        CxString path = buf->getFilePath();
                        int lastSlash = path.lastChar('/');
                        if (lastSlash >= 0) {
                            text = path.subString(lastSlash + 1, path.length() - lastSlash - 1);
                        } else {
                            text = path;
                        }

                        if (buf->isTouched()) {
                            tagMod = "/modified";
                        }
                        if (buf->isInMemory()) {
                            tagMem = "/in-memory";
                        }
                    } else {
                        text = "(unknown)";
                    }
                }
                break;

                case PVITEM_SEPARATOR:
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
            // compute layout: text area, tag placement
            //-------------------------------------------------------------------------------------
            int textAreaLen = contentWidth - prefixDisplayWidth - 1;

            // total tag display width: each tag plus a space separator between them
            int totalTagLen = 0;
            if (tagMod.length() > 0) totalTagLen += (int)tagMod.length();
            if (tagMem.length() > 0) {
                if (totalTagLen > 0) totalTagLen += 1;  // space between tags
                totalTagLen += (int)tagMem.length();
            }

            int maxTextLen = textAreaLen;
            if (totalTagLen > 0) {
                maxTextLen = textAreaLen - totalTagLen - 1;  // 1 space before tags
            }

            // for separator, skip normal text layout
            if (item->type != PVITEM_SEPARATOR) {
                if ((int)text.length() - textExtraBytes > maxTextLen) {
                    text = text.subString(0, maxTextLen - 3 + textExtraBytes);
                    text += "...";
                }

                while ((int)text.length() < textAreaLen + textExtraBytes) {
                    text += " ";
                }
            }

            //-------------------------------------------------------------------------------------
            // draw with selection highlight or normal colors
            //-------------------------------------------------------------------------------------
            int isSelected = (selectedListItemIndex == logicalItem);
            int isSeparator = (item->type == PVITEM_SEPARATOR);

            if (isSelected && !isSeparator) {
                screen->setForegroundColor(programDefaults->statusBarTextColor());
                screen->setBackgroundColor(programDefaults->statusBarBackgroundColor());
            } else {
                screen->setForegroundColor(programDefaults->modalContentTextColor());
                screen->setBackgroundColor(programDefaults->modalContentBackgroundColor());
            }

            if (totalTagLen > 0 && !isSeparator) {
                // draw prefix + text up to tag position
                int textBeforeTag = textAreaLen - totalTagLen + textExtraBytes;
                CxString textPart = text.subString(0, textBeforeTag);

                CxString line = prefix;
                line += textPart;
                screen->writeText(line);

                // set background for tags
                CxColor *tagBg = isSelected
                    ? programDefaults->statusBarBackgroundColor()
                    : programDefaults->modalContentBackgroundColor();

                // draw /modified tag in red
                if (tagMod.length() > 0) {
                    screen->setForegroundColor(&tagModifiedColor);
                    screen->setBackgroundColor(tagBg);
                    screen->writeText(tagMod);
                }

                // draw /in-memory tag in cyan
                if (tagMem.length() > 0) {
                    if (tagMod.length() > 0) {
                        // space between tags in normal text color
                        if (isSelected) {
                            screen->setForegroundColor(programDefaults->statusBarTextColor());
                        } else {
                            screen->setForegroundColor(programDefaults->modalContentTextColor());
                        }
                        screen->setBackgroundColor(tagBg);
                        screen->writeText(" ");
                    }
                    screen->setForegroundColor(&tagInMemoryColor);
                    screen->setBackgroundColor(tagBg);
                    screen->writeText(tagMem);
                }

                // trailing space in normal colors
                if (isSelected) {
                    screen->setForegroundColor(programDefaults->statusBarTextColor());
                    screen->setBackgroundColor(programDefaults->statusBarBackgroundColor());
                } else {
                    screen->setForegroundColor(programDefaults->modalContentTextColor());
                    screen->setBackgroundColor(programDefaults->modalContentBackgroundColor());
                }
                screen->writeText(" ");
            } else {
                // no tag - simple line
                CxString line = prefix;
                line += text;
                line += " ";
                screen->writeText(line);
            }

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
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// ProjectView::getSelectedItem
//
// Return the resolved file path if a file item is selected, empty string otherwise.
//
//-------------------------------------------------------------------------------------------------
CxString
ProjectView::getSelectedItem( void )
{
    if (selectedListItemIndex < 0 || selectedListItemIndex >= (int)_visibleItems.entries()) {
        return CxString("");
    }

    ProjectViewItem *item = _visibleItems.at(selectedListItemIndex);

    if (item->type == PVITEM_FILE) {
        ProjectSubproject *sub = project->subprojectAt(item->subprojectIndex);
        if (sub == NULL) {
            return CxString("");
        }
        return project->resolveFilePath(sub, sub->files.at(item->fileIndex));
    }

    if (item->type == PVITEM_OPEN_FILE) {
        CmEditBuffer *buf = editBufferList->at(item->bufferIndex);
        if (buf != NULL) {
            return buf->getFilePath();
        }
        return CxString("");
    }

    return CxString("");
}


//-------------------------------------------------------------------------------------------------
// ProjectView::getSelectedItemType
//
// Return the type of the currently selected item.
//
//-------------------------------------------------------------------------------------------------
ProjectViewItemType
ProjectView::getSelectedItemType( void )
{
    if (selectedListItemIndex < 0 || selectedListItemIndex >= (int)_visibleItems.entries()) {
        return PVITEM_ALL;
    }
    return _visibleItems.at(selectedListItemIndex)->type;
}


//-------------------------------------------------------------------------------------------------
// ProjectView::getSelectedSubproject
//
// Return the subproject for the currently selected item.
// Returns NULL if "All" is selected.
//
//-------------------------------------------------------------------------------------------------
ProjectSubproject*
ProjectView::getSelectedSubproject( void )
{
    if (selectedListItemIndex < 0 || selectedListItemIndex >= (int)_visibleItems.entries()) {
        return NULL;
    }

    ProjectViewItem *item = _visibleItems.at(selectedListItemIndex);
    if (item->subprojectIndex < 0) {
        return NULL;
    }

    return project->subprojectAt(item->subprojectIndex);
}


//-------------------------------------------------------------------------------------------------
// ProjectView::getContextFooter
//
// Build a context-sensitive footer string based on the currently selected item.
//
//-------------------------------------------------------------------------------------------------
CxString
ProjectView::getContextFooter( void )
{
    ProjectViewItemType selType = getSelectedItemType();

    // file items (project or non-project)
    if (selType == PVITEM_FILE || selType == PVITEM_OPEN_FILE) {
        CxString filePath = getSelectedItem();
        CmEditBuffer *buf = editBufferList->findPath(filePath);
        if (buf != NULL && buf->isTouched()) {
            return "[Enter] Open  [S] Save  [A] Save All  [Esc] Close";
        }
        return "[Enter] Open  [A] Save All  [Esc] Close";
    }

    // subproject header
    if (selType == PVITEM_SUBPROJECT) {
        ProjectSubproject *sub = getSelectedSubproject();
        if (sub != NULL && subprojectHasModifiedFile(sub)) {
            return "[S] Save All  [M] Make  [C] Clean  [T] Test  [N] New  [Esc] Close";
        }
        return "[M] Make  [C] Clean  [T] Test  [N] New  [Esc] Close";
    }

    // ALL row
    if (selType == PVITEM_ALL) {
        return "[M] Make  [C] Clean  [Esc] Close";
    }

    // open header
    if (selType == PVITEM_OPEN_HEADER) {
        return "[Enter] Expand/Collapse  [N] New  [Esc] Close";
    }

    // separator or no project
    return "[Esc] Close";
}


//-------------------------------------------------------------------------------------------------
// ProjectView::toggleSelectedSubproject
//
// Toggle expand/collapse of the selected subproject header and rebuild visible items.
//
//-------------------------------------------------------------------------------------------------
void
ProjectView::toggleSelectedSubproject( void )
{
    if (selectedListItemIndex < 0 || selectedListItemIndex >= (int)_visibleItems.entries()) {
        return;
    }

    ProjectViewItem *item = _visibleItems.at(selectedListItemIndex);

    if (item->type == PVITEM_OPEN_HEADER) {
        _otherFilesExpanded = !_otherFilesExpanded;
        rebuildVisibleItems();

        // ensure selected index is still valid
        if (selectedListItemIndex >= (int)_visibleItems.entries()) {
            selectedListItemIndex = (int)_visibleItems.entries() - 1;
            if (selectedListItemIndex < 0) {
                selectedListItemIndex = 0;
            }
        }
        return;
    }

    if (item->type != PVITEM_SUBPROJECT) {
        return;
    }

    ProjectSubproject *sub = project->subprojectAt(item->subprojectIndex);
    if (sub == NULL) {
        return;
    }

    sub->isExpanded = !sub->isExpanded;
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
// ProjectView::setVisible
//
// Set the visibility state for resize handling.
//
//-------------------------------------------------------------------------------------------------
void
ProjectView::setVisible( int visible )
{
    _visible = visible;
}


//-------------------------------------------------------------------------------------------------
// ProjectView::routeKeyAction
//
// Handle keyboard actions for navigation.
//
//-------------------------------------------------------------------------------------------------
void
ProjectView::routeKeyAction( CxKeyAction keyAction )
{
	switch (keyAction.actionType() )
	{
		case CxKeyAction::CURSOR:
			{
                if (handleArrows( keyAction )) {
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
// ProjectView::reframe
//
// If selected item isn't visible, adjust the scroll window.
//
//-------------------------------------------------------------------------------------------------
int
ProjectView::reframe( )
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

    while ( selectedListItemIndex < firstVisibleListIndex ) {
        changeMade = true;
        firstVisibleListIndex--;
        if (firstVisibleListIndex < 0) {
            firstVisibleListIndex = 0;
            break;
        }
    }

    while (selectedListItemIndex >= firstVisibleListIndex + screenProjectNumberOfLines) {
        changeMade = true;
        firstVisibleListIndex++;
    }

    if (changeMade) return(true);

    return(false);
}


//-------------------------------------------------------------------------------------------------
// ProjectView::handleArrows
//
// Move selection up/down through the visible items list.
//
//-------------------------------------------------------------------------------------------------
int
ProjectView::handleArrows( CxKeyAction keyAction )
{
    int totalItems = (int)_visibleItems.entries();
    int prevIndex = selectedListItemIndex;

    if (keyAction.tag() == "<arrow-down>") {

        selectedListItemIndex++;

        // skip non-selectable items
        while (selectedListItemIndex < totalItems &&
               _visibleItems.at(selectedListItemIndex)->type == PVITEM_SEPARATOR) {
            selectedListItemIndex++;
        }

        // if we ran off the end, stay put
        if (selectedListItemIndex >= totalItems) {
            selectedListItemIndex = prevIndex;
        }

        return( true );
    }

    if (keyAction.tag() == "<arrow-up>") {

        selectedListItemIndex--;

        // skip non-selectable items
        while (selectedListItemIndex >= 0 &&
               _visibleItems.at(selectedListItemIndex)->type == PVITEM_SEPARATOR) {
            selectedListItemIndex--;
        }

        // if we ran past the top, stay put
        if (selectedListItemIndex < 0) {
            selectedListItemIndex = prevIndex;
        }

        return( true );
    }

    return(false);
}
