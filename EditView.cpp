//-------------------------------------------------------------------------------------------------
//
//  EditView.cpp
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  Text editing view that handles display of the edit buffer on screen, including
//  sizing, resizing, and reframing the visible portion of the buffer.
//
//-------------------------------------------------------------------------------------------------


#include "EditView.h"

#if defined(_LINUX_) || defined(_OSX_)
#include <cx/base/file.h>
#include <cx/base/fileaccess.h>
#endif

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

    // NOTE: No resize callback here - ScreenEditor owns all resize handling

	// set the default for showing line numbers or not
	_showLineNumbers = pd->showLineNumbers();
    
    // set the jumpScroll variable from defaults
    _jumpScroll = pd->jumpScroll();

    // initialize region to full screen (-1 means use screen dimensions)
    _regionStartRow = -1;
    _regionEndRow = -1;


#if defined(_LINUX_) || defined(_OSX_)
    // MCP connection status (updated by ScreenEditor)
    _mcpConnected = 0;

    // block comment state tracking
    _blockCommentState = NULL;
    _blockCommentStateSize = 0;

    // mouse selection state
    _mouseSelectionActive = 0;
#endif

    // colorization cache (all platforms)
    _colorCache = NULL;
    _colorCacheSize = 0;
    _colorCacheGeneration = 1;

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

#if defined(_LINUX_) || defined(_OSX_)
    // update cached git branch for status bar
    updateGitBranch();

    // recompute block comment state for entire buffer
    recomputeBlockCommentState(0);
#endif

    // new buffer content: any previously cached colorization is wrong now
    bumpColorCacheGeneration();

    // pre-size the colorization cache to avoid a mid-session reallocation when
    // the buffer first grows past the current cache bound. One-time cost at
    // buffer load instead of a random future Enter hitting the 256-boundary.
    ensureColorCacheSize((int)editBuffer->numberOfLines() + 256);
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
#if defined(_LINUX_) || defined(_OSX_)
    // Use synchronized output to eliminate flicker on supported terminals
    CxScreen::beginSyncUpdate();
#endif

    CxString text = formatMultipleEditorLines(0,0);
    fputs( text.data() , stdout);
    updateStatusLine();

#if defined(_LINUX_) || defined(_OSX_)
    CxScreen::endSyncUpdate();
#endif

    screen->flush();
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
    recalcForResize();
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


#if defined(_LINUX_) || defined(_OSX_)
//-------------------------------------------------------------------------------------------------
// EditView::setMcpConnected
//
// Set MCP connection status for status bar display
//
//-------------------------------------------------------------------------------------------------
void
EditView::setMcpConnected(int connected)
{
    _mcpConnected = connected;
}
#endif


//-------------------------------------------------------------------------------------------------
// EditView::setRegion
//
// Sets the screen region this view occupies. Use -1 for either parameter to indicate
// "use full screen" behavior. When both are -1, the view uses the entire screen.
// When set to specific values, the view only uses rows from startRow to endRow.
//
//-------------------------------------------------------------------------------------------------
void
EditView::setRegion(int startRow, int endRow)
{
    _regionStartRow = startRow;
    _regionEndRow = endRow;
    recalcScreenPlacements();
    // Recalculate visible buffer range based on new _screenEditNumberOfLines
    recalcVisibleBufferFromTopEditLine(_visibleFirstEditBufferRow);
}




//-------------------------------------------------------------------------------------------------
// EditView::recalcForResize
//
// Public method called by ScreenEditor after screen resize.
// Recalculates screen placements and visible buffer range.
//
//-------------------------------------------------------------------------------------------------
void
EditView::recalcForResize(void)
{
    recalcScreenPlacements();
    recalcVisibleBufferFromTopEditLine(_visibleFirstEditBufferRow);
    reframe();
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
// to the editor. Supports both full-screen mode (region == -1) and split-screen mode
// where this view only occupies a portion of the screen.
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

    // set the visible edit columns. _visibleFirstEditBufferCol and _visibleLastEditBufferCol
    // are *display* columns. The line-number gutter is already excluded by subtracting
    // _lineNumberOffset from _screenEditNumberOfCols, so the visible window covers display
    // columns [_visibleFirstEditBufferCol, _visibleFirstEditBufferCol + _screenEditNumberOfCols).
    _visibleFirstEditBufferCol = 0;
    if (_showLineNumbers == TRUE) {
        _screenEditNumberOfCols = _screenNumberOfCols - _lineNumberOffset;
    } else {
        _screenEditNumberOfCols = _screenNumberOfCols;
    }
    _visibleLastEditBufferCol = _visibleFirstEditBufferCol + _screenEditNumberOfCols;

    // Check if using region mode or full screen mode
    if (_regionStartRow == -1 && _regionEndRow == -1) {
        //-------------------------------------------------------------------------------------
        // Full screen mode - use original calculations exactly
        //-------------------------------------------------------------------------------------

        // set the number of edit lines (subtract 2 for status and command rows)
        _screenEditNumberOfLines = _screenNumberOfLines - 2;

        // if window is too small to support the app, just bail
        if ((screen->rows() < 6) || (_screenNumberOfCols < 10)) {
            _windowTooSmall = true;
            return;
        }

        // upper left is always zero (zero based screen coordinates)
        _screenEditFirstRow = 0;

        // the last edit row is always three up from the bottom of screen
        _screenEditLastRow  = _screenNumberOfLines - 3;

        // the status row is always 2 up
        _screenStatusRow    = _screenNumberOfLines - 2;

        // the command row is always the last row
        _screenCommandRow   = _screenNumberOfLines - 1;

    } else {

        //-------------------------------------------------------------------------------------
        // Region mode - use specified bounds for split screen
        //-------------------------------------------------------------------------------------
        int effectiveStartRow = (_regionStartRow == -1) ? 0 : _regionStartRow;
        int effectiveEndRow = (_regionEndRow == -1) ? (int)_screenNumberOfLines - 2 : _regionEndRow;

        // all rows in region are for editing
        _screenEditNumberOfLines = effectiveEndRow - effectiveStartRow;

        // if region is too small, bail
        if (_screenEditNumberOfLines < 3 || _screenNumberOfCols < 10) {
            _windowTooSmall = true;
            return;
        }

        // first edit row is start of region
        _screenEditFirstRow = effectiveStartRow;

        // last edit row is end of region minus 1
        _screenEditLastRow = effectiveEndRow - 1;

        // status line is at end of region (each EditView owns its status bar)
        _screenStatusRow = effectiveEndRow;

        // command line is still at global position
        _screenCommandRow = _screenNumberOfLines - 1;
    }

    // Screen Locations key (full screen mode)
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
#if defined(_LINUX_) || defined(_OSX_)
    // populate search matches for highlight-all on modern platforms
    findAllSearchMatches(findString);
#endif

    // try and find the string starting at loc
	int result = editBuffer->findString( findString );

    CxEditBufferPosition loc = editBuffer->cursor;
    cursorGotoPosition( loc );

#if defined(_LINUX_) || defined(_OSX_)
    // force full redraw to show all highlights
    reframeAndUpdateScreen();
#endif

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
#if defined(_LINUX_) || defined(_OSX_)
    // repopulate search matches if pattern changed
    int patternChanged = (_searchPattern != findString);
    if (patternChanged) {
        findAllSearchMatches(findString);
    }
#endif

    int result = editBuffer->findAgain( findString, TRUE );

    CxEditBufferPosition loc = editBuffer->cursor;
    cursorGotoPosition( loc );

#if defined(_LINUX_) || defined(_OSX_)
    // force full redraw to show all highlights if pattern changed
    if (patternChanged) {
        reframeAndUpdateScreen();
    }
#endif

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

    // invalidate cache: multi-line replaces shift rows, single-line changes one row
    if (findString.firstChar('\n') != -1 || replaceString.firstChar('\n') != -1) {
        bumpColorCacheGeneration();
    } else {
        invalidateColorCache((int)loc.row);
    }

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

    // invalidate cache: multi-line replaces shift rows, single-line changes one row
    if (findString.firstChar('\n') != -1 || replaceString.firstChar('\n') != -1) {
        bumpColorCacheGeneration();
    } else {
        invalidateColorCache((int)loc.row);
    }

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

    if ( rowVisible(editBuffer->cursor.row) && colVisible()) {

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
	
    if ( !rowVisible(editBuffer->cursor.row) || !colVisible()) {
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
    // get the cursor's relative position within this view's region
    unsigned long screenRowOfCursor = bufferRowToScreenRow(editBuffer->cursor.row) - _screenEditFirstRow;

    // compute the new logical row number (use _screenEditNumberOfLines for region-aware paging)
    unsigned long newBufferRow = editBuffer->cursor.row + _screenEditNumberOfLines;

    // compute the new logical row plus the offset of the cursor screen row
    unsigned long newBufferRowWithOffset = newBufferRow + (_screenEditNumberOfLines - screenRowOfCursor);

    // jump in the logical buffer ahead to to new cursor location plus the offset
    editBuffer->cursorGotoLine( newBufferRowWithOffset );

    // update the screen
    if ( !rowVisible(editBuffer->cursor.row) || !colVisible()) {
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
    bumpColorCacheGeneration();
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
    // get the cursor's relative position within this view's region
    unsigned long screenRowOfCursor = bufferRowToScreenRow(editBuffer->cursor.row) - _screenEditFirstRow;

    // compute the new logical row number, and make sure we can still page up
    // use _screenEditNumberOfLines for region-aware paging
    unsigned long newBufferRow = 0;
    if (editBuffer->cursor.row <= _screenEditNumberOfLines ) {

        // we are on the first page, so just jump the cursor to the top of it
        editBuffer->cursorGotoLine( newBufferRow );
        return;
    }

    // compute the new buffer row number
    newBufferRow = editBuffer->cursor.row - _screenEditNumberOfLines;

    // compute a new jump with offset for the current cursor line
    unsigned long newBufferRowWithOffset = 0;
    if ( newBufferRow >= screenRowOfCursor) {
        newBufferRowWithOffset = newBufferRow - screenRowOfCursor;
    }

    // jump in the logical buffer back to to new cursor location minus the offset
    editBuffer->cursorGotoLine( newBufferRowWithOffset );

    // update the screen
    if ( !rowVisible(editBuffer->cursor.row) || !colVisible()) {
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
    invalidateColorCache((int)editBuffer->cursor.row);
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
    bumpColorCacheGeneration();
#if defined(_OSX_) || defined(_LINUX_)
    // if a selection is active, paste replaces it
    if (_mouseSelectionActive && editBuffer != NULL) {
        editBuffer->cutToMark();
        _mouseSelectionActive = 0;
    }
#endif
    editBuffer->pasteFromCutBuffer( text );
    updateScreen();
}


#ifdef CM_UTF8_SUPPORT
//-------------------------------------------------------------------------------------------------
// EditView::pasteText (CxUTFString version)
//
// Paste pre-parsed UTF-8 text then update the screen
//
//-------------------------------------------------------------------------------------------------
void
EditView::pasteText( CxUTFString &text )
{
    bumpColorCacheGeneration();
#if defined(_OSX_) || defined(_LINUX_)
    // if a selection is active, paste replaces it
    if (_mouseSelectionActive && editBuffer != NULL) {
        editBuffer->cutToMark();
        _mouseSelectionActive = 0;
    }
#endif
    editBuffer->pasteFromCutBuffer( text );
    updateScreen();
}
#endif


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
    // cut may span multiple lines — bump generation invalidates in O(1)
    bumpColorCacheGeneration();
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
    // when cursor is at end-of-line, this call cuts the virtual CR — joining the
    // next line up. that's a multi-line edit: every row from cursor+1 down shifts
    // up by one. detect by comparing line count before/after.
    unsigned long beforeLines = editBuffer->numberOfLines();
    unsigned long joinedRow   = editBuffer->cursor.row;
    int oldOffset             = (int)_lineNumberOffset;

    CxString text = editBuffer->cutTextToEndOfLine();

    int isLineJoin = (editBuffer->numberOfLines() < beforeLines);

    if (isLineJoin) {
        recalcLineNumberDigits();
#if defined(_LINUX_) || defined(_OSX_)
        clearSearchMatches();
        recomputeBlockCommentState((int)joinedRow);
#endif
        // line removed — every row from joinedRow+1 down shifts; cache invalid
        bumpColorCacheGeneration();

        if (oldOffset != (int)_lineNumberOffset) {
            // line-number column width changed; full repaint required
            reframe();
            updateScreen();
        } else if (!reframe()) {
            // no scrolling needed - try terminal delete-line optimization
            if (!terminalDeleteLineAndDraw(joinedRow)) {
                updateScreen();   // optimization unavailable - fall back
            }
        } else {
            // scrolling was needed - normal full update
            updateScreen();
        }
    } else {
        // single-line cut — only the cursor row's color cache is stale
        invalidateColorCache((int)joinedRow);
        updateScreen();
    }

    return( text );
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


#if defined(_LINUX_) || defined(_OSX_)
//-------------------------------------------------------------------------------------------------
// EditView::updateGitBranch
//
// Read .git/HEAD to get the current branch name. Walks up directories from the
// file's location to find the repository root. Fails silently if not in a git repo.
//
//-------------------------------------------------------------------------------------------------
void
EditView::updateGitBranch(void)
{
    _gitBranch = "";

    if (editBuffer == NULL) return;

    CxString filePath = editBuffer->getFilePath();
    if (filePath.length() == 0) return;

    // extract directory from file path
    CxString dir = filePath;
    int lastSlash = dir.lastChar('/');
    if (lastSlash < 0) return;
    dir = dir.subString(0, lastSlash);

    // walk up directories looking for .git/HEAD
    while (dir.length() > 0) {
        CxString gitHead = dir + "/.git/HEAD";

        CxFileAccess::status stat = CxFileAccess::checkStatus(gitHead);
        if (stat == CxFileAccess::FOUND_R || stat == CxFileAccess::FOUND_RW) {
            // found .git/HEAD - read it
            CxFile file;
            if (file.open(gitHead, "r")) {
                CxString content = file.getUntil('\n');
                file.close();

                // parse "ref: refs/heads/branchname"
                // check if content starts with "ref: refs/heads/"
                CxString prefix = "ref: refs/heads/";
                if (content.index(prefix) == 0) {
                    // extract branch name (skip 16 chars of prefix)
                    _gitBranch = content.subString(16, content.length() - 16);
                    // trim any trailing whitespace
                    _gitBranch.stripTrailing(" \t\r\n");
                }
                // detached HEAD or other format - leave _gitBranch empty
            }
            return;
        }

        // move up one directory
        lastSlash = dir.lastChar('/');
        if (lastSlash <= 0) break;  // hit root
        dir = dir.subString(0, lastSlash);
    }
}


//-------------------------------------------------------------------------------------------------
// EditView::findAllSearchMatches
//
// Scan the entire buffer for all occurrences of the search pattern.
// Called when a search is initiated to populate the highlight list.
//
//-------------------------------------------------------------------------------------------------
void
EditView::findAllSearchMatches(CxString pattern)
{
    // clear existing matches
    clearSearchMatches();

    if (pattern.length() == 0) return;
    if (editBuffer == NULL) return;

    _searchPattern = pattern;
    int patternLen = (int)pattern.length();

    // scan every line in the buffer
    unsigned long lineCount = editBuffer->numberOfLines();
    for (unsigned long lineNum = 0; lineNum < lineCount; lineNum++) {
#ifdef CM_UTF8_SUPPORT
        CxUTFString *utfLine = editBuffer->line(lineNum);
        if (utfLine == NULL) continue;
        CxString lineText = utfLine->toBytes();
#else
        CxString *linePtr = editBuffer->line(lineNum);
        if (linePtr == NULL) continue;
        CxString lineText = *linePtr;
#endif
        int pos = 0;

        // find all occurrences in this line
        while (pos < (int)lineText.length()) {
            int found = lineText.index(pattern, pos);
            if (found < 0) break;

            SearchMatch *match = new SearchMatch();
            match->line = (int)lineNum;
            match->startCol = found;
            match->endCol = found + patternLen;
            _searchMatches.append(match);

            pos = found + 1;  // advance past this match to find overlapping matches
        }
    }
}


//-------------------------------------------------------------------------------------------------
// EditView::clearSearchMatches
//
// Clear all search matches. Called on edit or when search pattern changes.
//
//-------------------------------------------------------------------------------------------------
void
EditView::clearSearchMatches(void)
{
    _searchMatches.clearAndDelete();
    _searchPattern = "";
}


//-------------------------------------------------------------------------------------------------
// EditView::applySearchHighlights
//
// Given a line of text (already syntax-colored), apply search highlight background
// to any matches that fall within the visible portion.
//
// The input text contains ANSI escape codes from syntax highlighting. We need to
// insert reverse video codes at the correct DISPLAY column positions, which means
// skipping over escape sequences when counting columns.
//
// Parameters:
//   text           - the colorized text (with ANSI escape codes)
//   bufferRow      - which buffer line this is
//   visibleStartCol - the first visible column (for horizontal scroll offset)
//
// Returns the text with search highlights applied.
//
//-------------------------------------------------------------------------------------------------
CxString
EditView::applySearchHighlights(CxString text, int bufferRow, int visibleStartCol)
{
    if (_searchPattern.length() == 0) return text;
    if (_searchMatches.entries() == 0) return text;

    // collect matches for this line that are visible
    // match columns are in BUFFER coordinates, we need to convert to visible coordinates
    int visibleEndCol = visibleStartCol + (int)_screenEditNumberOfCols;
    int matchCount = 0;

    // build arrays of start/end positions in VISIBLE coordinates (0 = first visible column)
    // limit to reasonable number of matches per line
    int matchStarts[32];
    int matchEnds[32];

    for (int i = 0; i < (int)_searchMatches.entries() && matchCount < 32; i++) {
        SearchMatch *m = _searchMatches.at(i);
        if (m->line == bufferRow) {
            // check if any part is visible
            if (m->endCol > visibleStartCol && m->startCol < visibleEndCol) {
                // convert to visible coordinates (0-based from visible start)
                int visStart = m->startCol - visibleStartCol;
                int visEnd = m->endCol - visibleStartCol;
                if (visStart < 0) visStart = 0;
                if (visEnd > (int)_screenEditNumberOfCols) visEnd = (int)_screenEditNumberOfCols;
                matchStarts[matchCount] = visStart;
                matchEnds[matchCount] = visEnd;
                matchCount++;
            }
        }
    }

    if (matchCount == 0) return text;

    // reverse video escape codes
    const char *highlightOn = "\033[7m";
    const char *highlightOff = "\033[27m";

    // scan through the colorized text, tracking display column
    // insert highlight codes at the right positions
    CxString result;
    int displayCol = 0;  // current display column (0 = first visible column)
    int inHighlight = 0;
    int textLen = (int)text.length();

    for (int i = 0; i < textLen; i++) {
        char c = text.charAt(i);

        // check for ANSI escape sequence (ESC [ ... m)
        if (c == '\033' && i + 1 < textLen && text.charAt(i + 1) == '[') {
            // copy the escape sequence as-is
            result += c;
            i++;
            while (i < textLen) {
                c = text.charAt(i);
                result += c;
                if (c == 'm') break;  // end of SGR sequence
                i++;
            }
            continue;
        }

        // before outputting this character, check if we need to toggle highlight
        // check if entering a match
        if (!inHighlight) {
            for (int mi = 0; mi < matchCount; mi++) {
                if (displayCol >= matchStarts[mi] && displayCol < matchEnds[mi]) {
                    result += highlightOn;
                    inHighlight = 1;
                    break;
                }
            }
        }

        // output the character
        result += c;
        displayCol++;

        // check if exiting all matches
        if (inHighlight) {
            int stillInMatch = 0;
            for (int mi = 0; mi < matchCount; mi++) {
                if (displayCol >= matchStarts[mi] && displayCol < matchEnds[mi]) {
                    stillInMatch = 1;
                    break;
                }
            }
            if (!stillInMatch) {
                result += highlightOff;
                inHighlight = 0;
            }
        }
    }

    // close any open highlight
    if (inHighlight) {
        result += highlightOff;
    }

    return result;
}


//-------------------------------------------------------------------------------------------------
// EditView::ensureBlockCommentStateSize
//
// Make sure the block comment state array is large enough for the buffer.
//
//-------------------------------------------------------------------------------------------------
void
EditView::ensureBlockCommentStateSize(int lineCount)
{
    if (lineCount <= _blockCommentStateSize) return;

    // allocate with some extra room to avoid frequent reallocation
    int newSize = lineCount + 256;
    int *newState = new int[newSize];

    // copy existing state if any
    if (_blockCommentState != NULL) {
        for (int i = 0; i < _blockCommentStateSize; i++) {
            newState[i] = _blockCommentState[i];
        }
        delete[] _blockCommentState;
    }

    // initialize new entries to 0
    for (int i = _blockCommentStateSize; i < newSize; i++) {
        newState[i] = 0;
    }

    _blockCommentState = newState;
    _blockCommentStateSize = newSize;
}


//-------------------------------------------------------------------------------------------------
// EditView::recomputeBlockCommentState
//
// Scan the buffer from the given line forward, tracking /* and */ to determine
// which lines are inside block comments.
//
// The state array stores whether each line ENDS inside a block comment.
// To check if a line STARTS inside a block comment, look at the previous line's state.
//
//-------------------------------------------------------------------------------------------------
void
EditView::recomputeBlockCommentState(int fromLine)
{
    if (editBuffer == NULL) return;

    // Skip block comment scanning for languages that don't use /* */ comments.
    // These languages either have no block comments or use different syntax.
    LanguageMode lang = markUp->getLanguageMode();
    if (lang == LANG_MARKDOWN || lang == LANG_SHELL || lang == LANG_MAKEFILE ||
        lang == LANG_PYTHON || lang == LANG_NONE) {
        // Clear any existing state to avoid stale data from previous file
        for (int i = 0; i < _blockCommentStateSize; i++) {
            _blockCommentState[i] = 0;
        }
        return;
    }

    int lineCount = (int)editBuffer->numberOfLines();
    if (lineCount == 0) return;

    ensureBlockCommentStateSize(lineCount);

    // get the state at the start of fromLine (from previous line's end state)
    int inComment = 0;
    if (fromLine > 0 && fromLine <= _blockCommentStateSize) {
        inComment = _blockCommentState[fromLine - 1];
    }

    // scan from fromLine to end of buffer
    for (int lineNum = fromLine; lineNum < lineCount; lineNum++) {
#ifdef CM_UTF8_SUPPORT
        CxUTFString *utfLine = editBuffer->line(lineNum);
        if (utfLine == NULL) {
            _blockCommentState[lineNum] = inComment;
            continue;
        }
        CxString lineText = utfLine->toBytes();
#else
        CxString *linePtr = editBuffer->line(lineNum);
        if (linePtr == NULL) {
            _blockCommentState[lineNum] = inComment;
            continue;
        }
        CxString lineText = *linePtr;
#endif

        // scan for /* and */ in this line
        int len = (int)lineText.length();
        for (int i = 0; i < len - 1; i++) {
            char c = lineText.charAt(i);
            char c2 = lineText.charAt(i + 1);

            if (!inComment && c == '/' && c2 == '*') {
                inComment = 1;
                i++;  // skip the *
            } else if (inComment && c == '*' && c2 == '/') {
                inComment = 0;
                i++;  // skip the /
            }
        }

        // Early exit for incremental rescans (fromLine > 0): if the recomputed
        // state equals what was already stored, no later line's state can
        // have changed, so we can stop. Full scans (fromLine == 0) always run
        // to the end because the array is zero-initialized and "matches" the
        // freshly-computed state spuriously on the first pass.
        int previousState = _blockCommentState[lineNum];
        if (previousState != inComment) {
            // block-comment boundary moved on this line — colorization of this
            // line (and the continuation of the walk) depends on the new state
            invalidateColorCache(lineNum);
        }
        _blockCommentState[lineNum] = inComment;
        if (fromLine > 0 && lineNum > fromLine && previousState == inComment) {
            break;
        }
    }
}


#endif  // close the modern-only block — the colorization cache helpers below
        // are built on all platforms

//-------------------------------------------------------------------------------------------------
// EditView::ensureColorCacheSize
//
// Grow the per-line colorization cache array to cover at least lineCount rows.
//-------------------------------------------------------------------------------------------------
void
EditView::ensureColorCacheSize(int lineCount)
{
    if (lineCount <= _colorCacheSize) return;

    int newSize = lineCount + 256;
    ColorCacheEntry *newCache = new ColorCacheEntry[newSize];

    // copy existing entries
    if (_colorCache != NULL) {
        for (int i = 0; i < _colorCacheSize; i++) {
            newCache[i] = _colorCache[i];
        }
        delete[] _colorCache;
    }

    // initialize new entries
    for (int i = _colorCacheSize; i < newSize; i++) {
        newCache[i].startCol = 0;
        newCache[i].visCols = 0;
        newCache[i].blockState = 0;
        newCache[i].endsState = 0;
        newCache[i].generation = 0;
        newCache[i].valid = 0;
    }

    _colorCache = newCache;
    _colorCacheSize = newSize;
}


//-------------------------------------------------------------------------------------------------
// EditView::invalidateColorCache
//
// Mark a single row's cached colorization invalid.
//-------------------------------------------------------------------------------------------------
void
EditView::invalidateColorCache(int row)
{
    if (_colorCache == NULL) return;
    if (row < 0 || row >= _colorCacheSize) return;
    _colorCache[row].valid = 0;
}


//-------------------------------------------------------------------------------------------------
// EditView::bumpColorCacheGeneration
//
// Invalidate every cached entry in one step by advancing the generation counter.
// Used on language-mode change, palette change, or colorize-syntax toggle.
//-------------------------------------------------------------------------------------------------
void
EditView::bumpColorCacheGeneration(void)
{
    _colorCacheGeneration++;
}


#if defined(_LINUX_) || defined(_OSX_)  // resume modern-only block

//-------------------------------------------------------------------------------------------------
// EditView::isLineInsideBlockComment
//
// Returns 1 if the given line starts inside a block comment.
// This checks the PREVIOUS line's end state.
//
//-------------------------------------------------------------------------------------------------
int
EditView::isLineInsideBlockComment(int lineNum)
{
    if (lineNum <= 0) return 0;
    if (_blockCommentState == NULL) return 0;
    if (lineNum > _blockCommentStateSize) return 0;

    return _blockCommentState[lineNum - 1];
}
#endif

