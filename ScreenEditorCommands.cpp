//-------------------------------------------------------------------------------------------------
//
//  ScreenEditorCommands.cpp
//  cmacs
//
//  CMD_*, CTRL_*, and CONTROL_* command handler implementations for ScreenEditor.
//  Extracted from ScreenEditor.cpp to reduce file size.
//
//-------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <sys/types.h>

#include "EditView.h"
#include "CommandLineView.h"
#include "ScreenEditor.h"
#include "Project.h"

#include <cx/process/process.h>

#ifdef CM_UTF8_SUPPORT
#include "UTFSymbols.h"
#endif


//-------------------------------------------------------------------------------------------------
// copyToSystemClipboard (macOS and Linux)
//
// Copies text to the system clipboard so it can be pasted into other applications.
// Uses pbcopy on macOS, xclip on Linux.
//
//-------------------------------------------------------------------------------------------------
#if defined(_OSX_) || defined(_LINUX_)
static void copyToSystemClipboard( CxString text )
{
    if (text.length() == 0) return;

#ifdef _OSX_
    FILE *pipe = popen("pbcopy", "w");
#else
    FILE *pipe = popen("xclip -selection clipboard", "w");
#endif
    if (pipe) {
        fwrite(text.data(), 1, text.length(), pipe);
        pclose(pipe);
    }
}
#endif


//-------------------------------------------------------------------------------------------------
// pasteFromSystemClipboard (macOS and Linux)
//
// Reads text from the system clipboard.
// Uses pbpaste on macOS, xclip on Linux.
//
//-------------------------------------------------------------------------------------------------
#if defined(_OSX_) || defined(_LINUX_)
static CxString pasteFromSystemClipboard( void )
{
    CxString result = "";

#ifdef _OSX_
    FILE *pipe = popen("pbpaste", "r");
#else
    FILE *pipe = popen("xclip -selection clipboard -o", "r");
#endif
    if (pipe) {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
    }

    return result;
}
#endif


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CONTROL_ReplaceAgain
//
// Performs a secondary replace command using the preveous (stored) find string and the previous
// (stored) replace string.
//
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CONTROL_ReplaceAgain( void )
{
    if (editView->replaceAgain( _findString, _replaceString )  == TRUE ) {
        setMessageWithLocation("(replace again found)");
    } else {
        setMessageWithLocation("(replace again not found)");
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CONTROL_ToggleLineNumbers
//
// turn line numbers on or off
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CONTROL_ToggleLineNumbers( void )
{
    editView->toggleLineNumbers();
    setMessage("(toggled line numbers)");
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CONTROL_ToggleJumpScroll
//
// turn jump scrolling on and off
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CONTROL_ToggleJumpScroll( void )
{
    editView->toggleJumpScroll();
    setMessage("(toggled jump scrolling)");
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CONTROL_FindAgain
//
// Repeat the last find command finding the last find string beyond the current cursor
// position
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CONTROL_FindAgain( void )
{
    if (editView->findAgain( _findString ) == TRUE ) {
        setMessageWithLocation("(found)");
    } else {
        setMessageWithLocation("(not found)");
    }

    editView->placeCursor();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_NewBuffer:
//
// Insert a comment block
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_NewBuffer( CxString commandLine )
{
    CxString fileName = commandLine.nextToken(" \t\n");

    char buffer[200];
    sprintf(buffer, "(new buffer %s loaded)", fileName.data() );

    setMessage( CxString(buffer));

    editView->updateScreen();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_CommentBlock:
//
// Insert a comment block
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_CommentBlock( CxString commandLine )
{
    CxString numberString = commandLine.nextToken(" \t\n");
    unsigned long lastCol = numberString.toUnsignedLong();

    char buffer[200];
    sprintf(buffer, "(comment block to column %lu inserted)", lastCol);

    setMessage( CxString(buffer));
    editView->insertCommentBlock( lastCol );
    editView->updateScreen();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_PasteText:
//
// Set the mark used in cut commands
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_PasteText( CxString commandLine )
{
    commandLineView->setText("");
    commandLineView->setPrompt("(text pasted)");
    editView->pasteText( _cutBuffer );
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_SystemPaste:
//
// Paste text from the system clipboard (macOS/Linux only)
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_SystemPaste( CxString commandLine )
{
#if defined(_OSX_) || defined(_LINUX_)
    CxString clipboardText = pasteFromSystemClipboard();
    if (clipboardText.length() > 0) {
        editView->pasteText( clipboardText );
        setMessage("(pasted from system clipboard)");
    } else {
        setMessage("(system clipboard empty)");
    }
#else
    setMessage("(system clipboard not available on this platform)");
#endif
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_CutToMark:
//
// Cuts text from the cursor position to the current mark (if there is one)
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_CutToMark( CxString commandLine )
{
    commandLineView->setText("");
    commandLineView->setPrompt("(text cut)");
    _cutBuffer = editView->cutToMark();
#if defined(_OSX_) || defined(_LINUX_)
    copyToSystemClipboard(_cutBuffer);
#endif
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_SetMark:
//
// Set the mark used in cut commands
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_SetMark( CxString commandLine )
{
    commandLineView->setText("");
    commandLineView->setPrompt("(mark set)");
    editView->setMark();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_GotoLine:
//
// Move cursor to entered line number
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_GotoLine( CxString commandLine )
{
    CxString numberString = commandLine.nextToken(" \t\n");
    unsigned long lineNumber = numberString.toUnsignedLong();

    if (lineNumber == 0) lineNumber = 1;

    char buffer[200];
    sprintf(buffer, "(goto-line %lu)", lineNumber);

    setMessage( CxString(buffer));
    editView->cursorGotoLine( lineNumber - 1 );
}



//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_LoadFile( CxString commandLine )
//
// load a file into a newly created buffer
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_LoadFile( CxString commandLine )
{
    CxString newFileName = commandLine.nextToken(" \t\n");

    //CmEditBuffer *editBuffer = editBufferList->current();
    loadNewFile( newFileName, TRUE );

    // redraw the edit view
    editView->reframeAndUpdateScreen();

    char buffer[200];
    sprintf( buffer, "(load %s)", newFileName.data());
    setMessage( buffer );
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_SaveFile:
//
// Save the current buffer to the filename specified.  If the file name is different than the
// file path that is held in the editbuffer, the name is changed in the editbuffer and the file
// is written as a new file.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_SaveFile(CxString commandLine)
{
    CxString fileName = commandLine.nextToken(" \t\n");
    fileName = fileName.stripLeading(" \t\n\r");
    fileName = fileName.stripTrailing(" \t\n\r");

    CmEditBuffer *editBuffer = editBufferList->current();
    editBuffer->saveText( fileName );

    setMessage("(file saved)");
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_BufferNext:
//
// Switch to next buffer (ESC command wrapper)
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_BufferNext( CxString commandLine )
{
    nextBuffer();
    setMessage("(next buffer)");
    editView->updateScreen();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_BufferPrev:
//
// Switch to previous buffer (ESC command wrapper)
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_BufferPrev( CxString commandLine )
{
    previousBuffer();
    setMessage("(previous buffer)");
    editView->updateScreen();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_BufferList:
//
// Show project/buffer list (ESC command wrapper)
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_BufferList( CxString commandLine )
{
    showFileListView();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_ListProjectFiles:
//
// Show project file list (ESC command wrapper)
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_ListProjectFiles( CxString commandLine )
{
    showFileListView();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_Quit:
//
// Quit editor (ESC command wrapper)
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_Quit( CxString commandLine )
{
    setMessage("(quit)");
    if (programDefaults->autoSaveOnBufferChange()) {
        saveCurrentEditBufferOnSwitch();
    }

    // ensure screen is in clean state before exit
    screen->resetColors();
    screen->flush();
    fflush(stdout);

    _quitRequested = TRUE;
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_Help
//
// Show help view (ESC command wrapper)
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_Help( CxString commandLine )
{
    showHelpView();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_Count
//
// Count lines and characters in the current buffer
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_Count( CxString commandLine )
{
    CmEditBuffer *editBuffer = editView->getEditBuffer();

    unsigned long lineCount = editBuffer->numberOfLines();
    unsigned long charCount = editBuffer->characterCount();

    char buffer[200];
    sprintf(buffer, "(%lu lines, %lu characters)", lineCount, charCount);
    setMessage(buffer);
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_Entab
//
// Convert leading spaces to tabs in entire buffer
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_Entab( CxString commandLine )
{
    CmEditBuffer *editBuffer = editView->getEditBuffer();
    editBuffer->entab();
    editView->reframeAndUpdateScreen();
    setMessage("(entab complete)");
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_Detab
//
// Convert tabs to spaces in entire buffer
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_Detab( CxString commandLine )
{
    CmEditBuffer *editBuffer = editView->getEditBuffer();
    editBuffer->detab();
    editView->reframeAndUpdateScreen();
    setMessage("(detab complete)");
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_TrimTrailing
//
// Remove trailing whitespace from all lines in buffer
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_TrimTrailing( CxString commandLine )
{
    CmEditBuffer *editBuffer = editView->getEditBuffer();
    int removed = editBuffer->trimTrailing();
    editView->reframeAndUpdateScreen();

    char msg[80];
    sprintf(msg, "(%d trailing character%s removed)", removed, removed == 1 ? "" : "s");
    setMessage(msg);
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_Make
//
// Run make (or make <target>) and capture output to *build* buffer.
// The buffer is created if it doesn't exist, or reused if it does.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_Make( CxString commandLine )
{
    // Build the command - "make" or "make <target>"
    CxString command = "make";
    if (commandLine.length() > 0) {
        command += " ";
        command += commandLine;
    }

    setMessage(CxString("(running: ") + command + ")");
    screen->flush();

    // Run the command
    CxProcess proc;
    int result = proc.run(command);

    if (result != 0) {
        setMessage("(make: failed to execute command)");
        return;
    }

    CxString output = proc.getOutput();

    // Find or create the *build* buffer
    CmEditBuffer *buildBuffer = editBufferList->findPath("*build*");

    if (buildBuffer == NULL) {
        // Create new buffer
        buildBuffer = new CmEditBuffer();
        buildBuffer->setFilePath("*build*");
        editBufferList->add(buildBuffer);
    } else {
        // Clear existing buffer and reload with new output
        buildBuffer->reset();
    }

    // Load output into buffer
    buildBuffer->loadTextFromString(output);

    // Switch to build buffer
    editView->setEditBuffer(buildBuffer);
    buildBuffer->cursorGotoRequest(0, 0);
    editView->reframeAndUpdateScreen();

    // Report result
    int exitCode = proc.getExitCode();
    char msg[80];
    if (exitCode == 0) {
        sprintf(msg, "(make: success, %lu lines)", buildBuffer->numberOfLines());
    } else {
        sprintf(msg, "(make: exit code %d, %lu lines - use goto-error to jump)",
                exitCode, buildBuffer->numberOfLines());
    }
    setMessage(msg);
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_GotoError
//
// Parse the current line for a file:line: pattern and jump to that location.
// Loads the file if not already open.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_GotoError( CxString commandLine )
{
    CmEditBuffer *editBuffer = editView->getEditBuffer();

    // Get current line
    unsigned long cursorRow = editBuffer->cursor.row;
    if (cursorRow >= editBuffer->numberOfLines()) {
        setMessage("(no error pattern found)");
        return;
    }

#ifdef CM_UTF8_SUPPORT
    CxUTFString *utfLine = editBuffer->line(cursorRow);
    if (utfLine == NULL) {
        setMessage("(no error pattern found)");
        return;
    }
    CxString line = utfLine->toBytes();
#else
    CxString *linePtr = editBuffer->line(cursorRow);
    if (linePtr == NULL) {
        setMessage("(no error pattern found)");
        return;
    }
    CxString line = *linePtr;
#endif

    // Parse the line for error pattern
    CxBuildError err = CxProcess::parseBuildError(line);

    if (!err.valid) {
        setMessage("(no error pattern found on this line)");
        return;
    }

    // Check if file is already open
    CmEditBuffer *targetBuffer = editBufferList->findPath(err.filename);

    if (targetBuffer == NULL) {
        // Load the file
        int loaded = loadNewFile(err.filename, 1);
        if (!loaded) {
            setMessage(CxString("(cannot open file: ") + err.filename + ")");
            return;
        }
        targetBuffer = editView->getEditBuffer();
    } else {
        // Switch to the buffer
        editView->setEditBuffer(targetBuffer);
    }

    // Go to the line (convert 1-based to 0-based)
    unsigned long targetLine = (err.line > 0) ? err.line - 1 : 0;
    unsigned long targetCol = (err.column > 0) ? err.column - 1 : 0;

    targetBuffer->cursorGotoRequest(targetLine, targetCol);
    editView->reframeAndUpdateScreen();

    char msg[120];
    sprintf(msg, "(%s:%d)", err.filename.data(), err.line);
    setMessage(msg);
}


#ifdef CM_UTF8_SUPPORT
//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_InsertUTFBox
//
// Insert a box drawing UTF-8 symbol at the cursor position
// The argument is a short name like "upper-left" which becomes "box-upper-left"
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_InsertUTFBox( CxString commandLine )
{
    insertUTFSymbolHelper( commandLine, "box" );
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::insertUTFSymbolHelper
//
// Common implementation for UTF symbol insertion commands.
// Uses _currentCommand->symbolFilter to determine the filter prefix.
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::insertUTFSymbolHelper( CxString commandLine, const char *symbolType )
{
    CxString shortName = commandLine;
    shortName = shortName.stripLeading(" \t\n\r");
    shortName = shortName.stripTrailing(" \t\n\r");

    if (shortName.length() == 0) {
        setMessage("(no symbol specified)");
        return;
    }

    // get filter from current command entry
    CxString filter = "";
    if (_currentCommand != NULL && _currentCommand->symbolFilter != NULL) {
        filter = _currentCommand->symbolFilter;
    }

    // prepend filter to get full symbol name
    CxString symbolName = filter;
    symbolName += shortName;

    UTFSymbolEntry *symbol = UTFSymbols::findExact( symbolName );

    if (symbol == NULL) {
        char buffer[200];
        sprintf(buffer, "(unknown %s symbol: %s)", symbolType, shortName.data());
        setMessage(buffer);
        return;
    }

    // insert the UTF-8 character at cursor (as a single character)
    CmEditBuffer *editBuffer = editView->getEditBuffer();
    editBuffer->addCharacter( CxString(symbol->utf8) );

    editView->reframeAndUpdateScreen();

    char buffer[200];
    sprintf(buffer, "(inserted %s)", symbol->name);
    setMessage(buffer);
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_InsertUTFSymbol
//
// Insert a common UTF-8 symbol at the cursor position
// The argument is a short name like "bullet" which becomes "sym-bullet"
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_InsertUTFSymbol( CxString commandLine )
{
    insertUTFSymbolHelper( commandLine, "symbol" );
}
#endif


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_Find:
//
// Find a string in the buffer beyond the current current position
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_Find( CxString commandLine )
{
//	_findString = commandLine.nextToken(" \t\n");
    _findString = commandLine;
    _findString = _findString.stripLeading(" \t\n\r");
    _findString = _findString.stripTrailing(" \t\n\r");
    _findString.replaceAll( CxString("/n"), CxString("\n") );

    if (editView->findString( _findString ) == TRUE ) {
        setMessageWithLocation("(found)");
    } else {
        setMessage("(not found)");
    }
}

//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_Replace
//
// Performs an initial replace command using the preveous (stored) find string and replacing
// with the stated replace string.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_Replace(CxString commandLine)
{
//    _replaceString = commandLine.nextToken(" \t\n");
    _replaceString = commandLine;

    _replaceString = _replaceString.stripLeading(" \t\n\r");
    _replaceString = _replaceString.stripTrailing(" \t\n\r");
    _replaceString.replaceAll( CxString("/n"), CxString("\n") );

    if (editView->replaceString( _findString, _replaceString ) == TRUE ) {
        setMessageWithLocation("(replace found)");
    } else {
        setMessageWithLocation("(replace not found)");
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::CMD_ReplaceAll
//
// Replace all occurrences of the previous find string with the replacement string.
// User must have done a find (ESC f) first to set the find pattern.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::CMD_ReplaceAll(CxString commandLine)
{
    // check if we have a find pattern
    if (_findString.length() == 0) {
        setMessage("(no find pattern - use ESC f first)");
        return;
    }

    // parse and store the replacement string
    _replaceString = commandLine;
    _replaceString = _replaceString.stripLeading(" \t\n\r");
    _replaceString = _replaceString.stripTrailing(" \t\n\r");
    _replaceString.replaceAll( CxString("/n"), CxString("\n") );

    // get direct access to editBuffer to avoid per-replacement screen updates
    CmEditBuffer *editBuffer = editView->getEditBuffer();

    // move cursor to beginning of buffer
    editBuffer->cursorGotoRequest(0, 0);

    // replace all occurrences - call editBuffer directly to skip screen updates
    int count = 0;
    while (editBuffer->replaceAgain(_findString, _replaceString) == TRUE) {
        count++;
    }

    // single screen refresh after all replacements
    editView->reframeAndUpdateScreen();

    // report results
    char buffer[200];
    if (count == 0) {
        sprintf(buffer, "(no occurrences of '%s' found)", _findString.data());
    } else if (count == 1) {
        sprintf(buffer, "(1 replacement)");
    } else {
        sprintf(buffer, "(%d replacements)", count);
    }
    setMessage(buffer);
}


//-------------------------------------------------------------------------------------------------
// Control command handlers - small focused methods called from dispatch table
//-------------------------------------------------------------------------------------------------

void ScreenEditor::CTRL_Cut(void)
{
    _cutBuffer = editView->cutToMark();
#if defined(_OSX_) || defined(_LINUX_)
    copyToSystemClipboard(_cutBuffer);
#endif
}

void ScreenEditor::CTRL_Paste(void)
{
    editView->pasteText(_cutBuffer);
}

void ScreenEditor::CTRL_CutToEndOfLine(void)
{
    _cutBuffer = editView->cutTextToEndOfLine();
#if defined(_OSX_) || defined(_LINUX_)
    copyToSystemClipboard(_cutBuffer);
#endif
}

void ScreenEditor::CTRL_PageDown(void)
{
    editView->pageDown();
}

void ScreenEditor::CTRL_PageUp(void)
{
    editView->pageUp();
}

void ScreenEditor::CTRL_NextBuffer(void)
{
    nextBuffer();
}

void ScreenEditor::CTRL_ProjectList(void)
{
    showFileListView();
}

void ScreenEditor::CTRL_UpdateScreen(void)
{
    editView->updateScreen();
}

void ScreenEditor::CTRL_Help(void)
{
    showHelpView();
}

void ScreenEditor::CTRLX_Save(void)
{
    CmEditBuffer *editBuffer = editBufferList->current();
    CxString filePath = editBuffer->getFilePath();

    if (filePath.length()) {
        CMD_SaveFile(filePath);
    } else {
        setMessage("(there is no current filename, use ESC s)");
    }
}

void ScreenEditor::CTRLX_Quit(void)
{
    setMessage("(quit)");
    if (programDefaults->autoSaveOnBufferChange()) {
        saveCurrentEditBufferOnSwitch();
    }
}
