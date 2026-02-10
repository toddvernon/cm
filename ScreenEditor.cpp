//-------------------------------------------------------------------------------------------------
//
//  ScreenEditor.cpp
//  cmacs
//
//  Created by Todd Vernon on 6/24/22.
//  Copyright © 2022 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

#include "EditView.h"
#include "CommandLineView.h"
#include "ScreenEditor.h"
#include "Project.h"

#ifdef CM_UTF8_SUPPORT
#include "UTFSymbols.h"
#endif


//-------------------------------------------------------------------------------------------------
// Status line fill character - use UTF-8 box drawing on Unix/Mac, '=' elsewhere
//-------------------------------------------------------------------------------------------------
#if defined(_LINUX_) || defined(_OSX_)
static const char *STATUS_FILL = "\xe2\x94\x80";  // ─ (U+2500 BOX DRAWINGS LIGHT HORIZONTAL)
#else
static const char *STATUS_FILL = "=";
#endif


//-------------------------------------------------------------------------------------------------
// ScreenEditor::ScreenEditor (constructor)
//
// Creates all the important parts of the program, the editview, the commandline, they keyboard
// input object.
//
//-------------------------------------------------------------------------------------------------
ScreenEditor::ScreenEditor( CxScreen *scr, CxKeyboard *key, CxString filePath )
{
    // DEBUG: open log file for startup diagnostics (commented out)
    // FILE *dbg = fopen("/tmp/cm_debug.log", "w");
    // if (dbg) { fprintf(dbg, "1: constructor start\n"); fflush(dbg); }

    // Block SIGWINCH during construction to prevent callbacks on partially-constructed objects
    sigset_t blockSet, oldSet;
    sigemptyset(&blockSet);
    sigaddset(&blockSet, SIGWINCH);
    sigprocmask(SIG_BLOCK, &blockSet, &oldSet);

    // if (dbg) { fprintf(dbg, "2: signals blocked\n"); fflush(dbg); }

    // initialize command input state FIRST to ensure it's IDLE during all initialization
    _cmdInputState = CMD_INPUT_IDLE;

    // initialize all pointers to NULL first to avoid undefined behavior
    programDefaults = NULL;
    screen = NULL;
    keyboard = NULL;
    editView = NULL;
    editViewBottom = NULL;
    commandLineView = NULL;
    editBufferList = NULL;
    projectView = NULL;
    helpView = NULL;
    buildView = NULL;
    buildOutput = NULL;
    project = NULL;
    _activeCompleter = NULL;
    _currentCommand = NULL;
    _quitRequested = FALSE;
    _activeBuildSubproject = NULL;
    _buildAllIndex = -1;
    _newFileSubproject = NULL;
    _newFileFromProjectView = 0;

    // initialize split screen state
    _splitMode = 0;     // single view mode
    _splitRow = 0;      // no split
    _activeView = 0;    // top/only view is active
#if defined(_LINUX_) || defined(_OSX_)
    _mcpHandler = NULL;
#endif

    programMode = ScreenEditor::EDIT;

    // if (dbg) { fprintf(dbg, "3: pointers initialized\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // load the default setting file - check current directory first, then home directory
    //
    //---------------------------------------------------------------------------------------------
    programDefaults = new ProgramDefaults();
    // if (dbg) { fprintf(dbg, "4: ProgramDefaults created\n"); fflush(dbg); }

    CxString configPath = ".cmrc";
    CxFile testFile;

    // try current directory first
    if (!testFile.open(configPath, "r")) {
        // fall back to home directory
        CxString homeDir = getenv("HOME");
        if (homeDir.length()) {
            configPath = homeDir + "/.cmrc";
        }
    } else {
        testFile.close();
    }

    if (configPath.length()) {
        programDefaults->loadDefaults(configPath);
    }
    
    //---------------------------------------------------------------------------------------------
    // init the cut buffer and replace buffer
    //
    //---------------------------------------------------------------------------------------------
    _cutBuffer = "";
    _findString = "";
    _replaceString = "";

    //---------------------------------------------------------------------------------------------
    // init the command completers and command input state
    //
    //---------------------------------------------------------------------------------------------
    _activeCompleter = &_commandCompleter;
    _cmdInputState = CMD_INPUT_IDLE;
    _cmdBuffer = "";
    _argBuffer = "";
    _currentCommand = NULL;
    initCommandCompleters();

    // if (dbg) { fprintf(dbg, "5: command completers created\n"); fflush(dbg); }

    editBufferList = new CmEditBufferList();
    // if (dbg) { fprintf(dbg, "6: editBufferList created\n"); fflush(dbg); }
    
    //---------------------------------------------------------------------------------------------
    // create a screen object
    //
    //---------------------------------------------------------------------------------------------
    screen = scr;
    
    //---------------------------------------------------------------------------------------------
    // create a keyboard object where input is gathered from
    //
    //------------------------------------------------------------------------------
    keyboard = key;
    
    // if (dbg) { fprintf(dbg, "7: about to create CommandLineView\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // make a command line view where prompts and messages appear
    //
    //---------------------------------------------------------------------------------------------
    commandLineView = new CommandLineView(
                                          programDefaults,
                                          screen,
                                          editView,
                                          screen->rows()-1,
                                          1,
                                          screen->cols()-1);

    // if (dbg) { fprintf(dbg, "8: CommandLineView created\n"); fflush(dbg); }

    commandLineView->setPrompt("");



    screen->clearScreen();

    // if (dbg) { fprintf(dbg, "9: screen cleared\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // make an edit window where the editing happens
    //
    //---------------------------------------------------------------------------------------------
    editView = new EditView( programDefaults, screen );

    // if (dbg) { fprintf(dbg, "10: EditView created\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // create a project object (regardless if there is a project)
    //
    //---------------------------------------------------------------------------------------------
    project = new Project();
    // if (dbg) { fprintf(dbg, "11: Project created\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // Check to see if we are loading a project, if the filename ends in .project then its a
    // project file
    //
    //---------------------------------------------------------------------------------------------
    int isProjectFile = FALSE;
    int index = filePath.index(".project");
    if (index == filePath.length() - 8 ) {
        isProjectFile = TRUE;
    }

    if ( isProjectFile ) {
        
        CxString firstFilePath = filePath;
    
        //-----------------------------------------------------------------------------------------
        // file is a project definition, so load the project, and load the project file into
        // a buffer.
        //-----------------------------------------------------------------------------------------
        project->load( filePath );
        loadNewFile( filePath, TRUE );

        int numberOfFiles = project->numberOfFiles();
        for (int c=0; c<numberOfFiles; c++) {
            CxString nextFile = project->fileAt( c );
            loadNewFile( nextFile, FALSE );
			if ( c == 0) {
				firstFilePath = nextFile;
			}
        }
        
        char buffer[200];
        sprintf(buffer, "(%d project files loaded from project %s)",
                numberOfFiles,
                project->projectName().data() );
        
        loadNewFile( firstFilePath, TRUE );

        setMessage( buffer );
 
    } else {

        // if (dbg) { fprintf(dbg, "12: about to loadNewFile\n"); fflush(dbg); }

        //-----------------------------------------------------------------------------------------
        // not a project file so just load the referenced file
        //
        //-----------------------------------------------------------------------------------------
        loadNewFile( filePath, TRUE );

        // if (dbg) { fprintf(dbg, "13: loadNewFile complete\n"); fflush(dbg); }
    }

    // if (dbg) { fprintf(dbg, "14: about to create ProjectView\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // create a file list view for the project
    //
    //---------------------------------------------------------------------------------------------

    projectView = new ProjectView(programDefaults,
                                    editBufferList,
                                    project,
                                    screen
                                    );

    // if (dbg) { fprintf(dbg, "15: ProjectView created\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // create a help view for editor help
    //
    //---------------------------------------------------------------------------------------------

  	helpView = new HelpView(programDefaults, screen);

    // if (dbg) { fprintf(dbg, "16: HelpView created\n"); fflush(dbg); }

    // if (dbg) { fprintf(dbg, "17: help text loaded\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // create the build output system
    //
    //---------------------------------------------------------------------------------------------
    buildOutput = new BuildOutput();
    buildView = new BuildView(programDefaults, screen, buildOutput);

    // Register idle callback for polling build output
    keyboard->addIdleCallback( CxDeferCall( this, &ScreenEditor::buildIdleCallback ));

    //---------------------------------------------------------------------------------------------
    // update the screen and place the cursor
    //
    //---------------------------------------------------------------------------------------------
    // if (dbg) { fprintf(dbg, "17a: editView ptr = %p\n", (void*)editView); fflush(dbg); }
    // if (dbg) { fprintf(dbg, "17b: about to call editView->updateScreen()\n"); fflush(dbg); }

	editView->updateScreen();

    // if (dbg) { fprintf(dbg, "18: editView->updateScreen complete\n"); fflush(dbg); }

    editView->placeCursor();

    // if (dbg) { fprintf(dbg, "19: editView->placeCursor complete\n"); fflush(dbg); }

#if 0 // MCP disabled for debugging
#if defined(_LINUX_) || defined(_OSX_)
    //---------------------------------------------------------------------------------------------
    // Start MCP handler thread for Claude Desktop integration
    //
    //---------------------------------------------------------------------------------------------
    _mcpHandler = new MCPHandler(this);
    _mcpHandler->start();

    // Register idle callback to check for MCP screen updates (~100ms intervals)
    keyboard->addIdleCallback( CxDeferCall( this, &ScreenEditor::mcpIdleCallback ));
#endif
#endif

    //---------------------------------------------------------------------------------------------
    // Register ScreenEditor's resize callback LAST so it fires after EditView/ProjectView
    // This allows ScreenEditor to coordinate final redraw based on programMode
    //
    //---------------------------------------------------------------------------------------------
    screen->addScreenSizeCallback( CxDeferCall( this, &ScreenEditor::screenResizeCallback ));

    // Unblock SIGWINCH now that construction is complete
    sigprocmask(SIG_SETMASK, &oldSet, NULL);

    // if (dbg) { fprintf(dbg, "20: constructor complete, signals unblocked\n"); fclose(dbg); }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::initCommandCompleters
//
// Populate the Completer objects from the command table and UTF symbol table.
// The command completer gets all commands as candidates. Commands with
// CMD_FLAG_SYMBOL_ARG get child completers for their symbol tables.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::initCommandCompleters( void )
{
    // populate child completers first so they're ready when referenced
#ifdef CM_UTF8_SUPPORT
    for (int i = 0; UTFSymbols::symbolAt(i) != NULL; i++) {
        UTFSymbolEntry *sym = UTFSymbols::symbolAt(i);
        CxString symName = sym->name;

        // symbols starting with "box-" go to box completer (strip prefix)
        if (symName.length() > 4 && symName.subString(0, 4) == "box-") {
            CxString shortName = symName.subString(4, symName.length() - 4);
            _boxSymbolCompleter.addCandidate( shortName, NULL, (void*)sym );
        }
        // symbols starting with "sym-" go to symbol completer (strip prefix)
        else if (symName.length() > 4 && symName.subString(0, 4) == "sym-") {
            CxString shortName = symName.subString(4, symName.length() - 4);
            _symSymbolCompleter.addCandidate( shortName, NULL, (void*)sym );
        }
    }
#endif

    // populate command completer from the command table
    for (int i = 0; commandTable[i].name != NULL; i++) {
        CommandEntry *entry = &commandTable[i];
        Completer *child = NULL;

#ifdef CM_UTF8_SUPPORT
        // link symbol commands to their child completers
        if (entry->flags & CMD_FLAG_SYMBOL_ARG) {
            if (entry->symbolFilter != NULL) {
                CxString filter = entry->symbolFilter;
                if (filter == "box-") {
                    child = &_boxSymbolCompleter;
                }
                else if (filter == "sym-") {
                    child = &_symSymbolCompleter;
                }
            }
        }
#endif

        _commandCompleter.addCandidate( entry->name, child, (void*)entry );
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::~ScreenEditor (destructor)
//
// cleans up all the dynamic objects
//
//-------------------------------------------------------------------------------------------------
ScreenEditor::~ScreenEditor(void)
{
#if defined(_LINUX_) || defined(_OSX_)
    // Shutdown MCP handler thread
    if (_mcpHandler != NULL) {
        _mcpHandler->shutdown();
        _mcpHandler->join();
        delete _mcpHandler;
        _mcpHandler = NULL;
    }
#endif

    delete programDefaults;
    delete commandLineView;
    delete editView;
    if (editViewBottom != NULL) {
        delete editViewBottom;
    }
    // Note: screen and keyboard are owned by main(), not deleted here
}


#if defined(_LINUX_) || defined(_OSX_)
//-------------------------------------------------------------------------------------------------
// ScreenEditor::mcpIdleCallback
//
// Called during keyboard idle (~100ms intervals) to check if MCP handler modified buffers
// and needs a screen refresh. This allows the screen to update without waiting for a keypress.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::mcpIdleCallback(void)
{
    if (_mcpHandler != NULL) {
        // Process any pending MCP request on the main thread
        // This ensures thread safety - editor data is only accessed from main thread
        _mcpHandler->processPendingRequest();

        // Update MCP connection status for status bar
        editView->setMcpConnected(_mcpHandler->isConnected());

        if (_mcpHandler->needsRedraw()) {
            _mcpHandler->clearNeedsRedraw();

            // Display any status message from MCP command
            CxString statusMsg = _mcpHandler->getStatusMessage();
            if (statusMsg.length() > 0) {
                setMessage(statusMsg);
                _mcpHandler->clearStatusMessage();
            }

            // Update active view only
            activeEditView()->reframeAndUpdateScreen();
            activeEditView()->placeCursor();
            screen->flush();
        }
    }
}
#endif


//-------------------------------------------------------------------------------------------------
// ScreenEditor::activeEditView
//
// Returns the currently active EditView. In single view mode, returns editView.
// In split mode, returns editView (top) or editViewBottom based on _activeView.
//
//-------------------------------------------------------------------------------------------------
EditView*
ScreenEditor::activeEditView(void)
{
    if (_splitMode == 0 || editViewBottom == NULL) {
        return editView;
    }
    return (_activeView == 0) ? editView : editViewBottom;
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::splitHorizontal
//
// Splits the screen horizontally, creating a bottom view. The top view keeps the current
// buffer, the bottom view gets the *build* buffer or current buffer if *build* doesn't exist.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::splitHorizontal(void)
{
    if (_splitMode == 1) {
        // already split
        return;
    }

    // calculate split position (middle of screen, leaving room for status/command)
    int totalRows = screen->rows();
    // Divider row is in the middle of the editable area
    // Leave 2 rows at bottom for status+command
    int editableRows = totalRows - 2;
    _splitRow = editableRows / 2;

    // create bottom view if it doesn't exist
    if (editViewBottom == NULL) {
        editViewBottom = new EditView(programDefaults, screen);
    }

    // set regions for both views
    // top view: row 0 to splitRow (status line of top view)
    editView->setRegion(0, _splitRow);

    // bottom view: row splitRow+1 to totalRows-2 (before command line)
    editViewBottom->setRegion(_splitRow + 1, totalRows - 2);

    // Bottom view gets a different buffer if available, otherwise an empty buffer
    // Skip .project files since they're just metadata
    CmEditBuffer *bottomBuffer = NULL;
    int numBuffers = editBufferList->items();

    if (numBuffers > 1) {
        int currentIdx = editBufferList->currentItemIndex();
        // Search for a suitable buffer (not current, not .project)
        for (int i = 1; i < numBuffers; i++) {
            int idx = (currentIdx + i) % numBuffers;
            CmEditBuffer *candidate = editBufferList->at(idx);
            if (candidate != NULL) {
                CxString path = candidate->getFilePath();
                // Skip .project files
                int projIdx = path.index(".project");
                if (projIdx >= 0 && projIdx == (int)path.length() - 8) {
                    continue;  // Skip this one
                }
                bottomBuffer = candidate;
                break;
            }
        }
    }

    if (bottomBuffer != NULL) {
        if (!bottomBuffer->isInMemory()) {
            bottomBuffer->loadText(bottomBuffer->getFilePath(), TRUE);
        }
        editViewBottom->setEditBuffer(bottomBuffer);
    } else {
        // No suitable buffer found, create an empty one
        CmEditBuffer *emptyBuffer = new CmEditBuffer();
        emptyBuffer->setFilePath("*scratch*");
        editBufferList->add(emptyBuffer);
        editViewBottom->setEditBuffer(emptyBuffer);
    }

    _splitMode = 1;
    _activeView = 0;  // top view is active

    // redraw everything
    screen->clearScreen();
    editView->updateScreen();
    editViewBottom->updateScreen();
    activeEditView()->placeCursor();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::unsplit
//
// Returns to single view mode, destroying the bottom view.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::unsplit(void)
{
    if (_splitMode == 0) {
        // not split
        return;
    }

    _splitMode = 0;
    _splitRow = 0;
    _activeView = 0;

    // reset top view to full screen
    editView->setRegion(-1, -1);

    // redraw
    screen->clearScreen();
    editView->reframeAndUpdateScreen();
    editView->placeCursor();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::switchActiveView
//
// Toggles focus between top and bottom views in split mode.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::switchActiveView(void)
{
    if (_splitMode == 0 || editViewBottom == NULL) {
        // not split, nothing to switch
        return;
    }

    _activeView = (_activeView == 0) ? 1 : 0;

    // Update the inactive view first (no cursor placement needed)
    if (_activeView == 0) {
        editViewBottom->updateScreen();
    } else {
        editView->updateScreen();
    }

    // Update and reframe the active view (ensures cursor is visible and correctly placed)
    activeEditView()->reframeAndUpdateScreen();

    // Final explicit cursor placement in the active view
    activeEditView()->placeCursor();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::recalcSplitRegions
//
// Recalculates the regions for both views after a terminal resize.
// Called from the screen resize callback.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::recalcSplitRegions(void)
{
    if (_splitMode == 0) {
        return;
    }

    // recalculate split position
    int totalRows = screen->rows();
    _splitRow = (totalRows - 2) / 2;

    // update regions
    editView->setRegion(0, _splitRow);
    if (editViewBottom != NULL) {
        editViewBottom->setRegion(_splitRow + 1, totalRows - 2);
    }

    // redraw
    screen->clearScreen();
    editView->updateScreen();
    if (editViewBottom != NULL) {
        editViewBottom->updateScreen();
    }
    activeEditView()->placeCursor();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::screenResizeCallback
//
// THE SINGLE resize callback for the entire application.
// ScreenEditor owns all resize handling and coordinates redrawing in correct order.
//
// Architecture: ALL region math first, THEN all drawing in correct z-order.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::screenResizeCallback(void)
{
    //=============================================================================================
    // PHASE 1: ALL RECALC / REGION MATH
    //=============================================================================================

    // 1a. Split regions (if split mode)
    if (_splitMode == 1) {
        int totalRows = screen->rows();
        _splitRow = (totalRows - 2) / 2;
        editView->setRegion(0, _splitRow);
        if (editViewBottom != NULL) {
            editViewBottom->setRegion(_splitRow + 1, totalRows - 2);
        }
    }

    // 1b. EditView recalc (handles screen size change + reframe)
    editView->recalcForResize();

    // 1c. EditViewBottom recalc (if split)
    if (_splitMode == 1 && editViewBottom != NULL) {
        editViewBottom->recalcForResize();
    }

    // 1d. CommandLineView recalc
    commandLineView->recalcScreenPlacements();

    // 1e. ProjectView recalc (if in PROJECTVIEW mode)
    if (programMode == PROJECTVIEW) {
        projectView->recalcScreenPlacements();
    }

    // 1f. HelpView recalc (if in HELPVIEW mode)
    if (programMode == HELPVIEW) {
        helpView->recalcScreenPlacements();
    }

    // 1g. BuildView recalc (if in BUILDVIEW mode)
    if (programMode == BUILDVIEW) {
        buildView->recalcScreenPlacements();
    }

    //=============================================================================================
    // PHASE 2: ALL DRAWING IN CORRECT Z-ORDER
    //=============================================================================================

    // 2a. Clear screen
    screen->clearScreen();

    // 2b. Draw editView (top/only editor - each EditView draws its own status bar)
    editView->updateScreen();

    // 2c. Draw editViewBottom (if split)
    if (_splitMode == 1 && editViewBottom != NULL) {
        editViewBottom->updateScreen();
    }

    // 2e. Draw commandLineView
    commandLineView->updateScreen();

    // 2f. Draw modal on top (if applicable)
    if (programMode == PROJECTVIEW) {
        screen->hideCursor();
        projectView->redraw();
        // Note: projectView->redraw() includes flush
        return;
    }

    if (programMode == HELPVIEW) {
        helpView->redraw();
        return;
    }

    if (programMode == BUILDVIEW) {
        buildView->redraw();
        return;
    }

    // 2g. Place cursor (EDIT or COMMANDLINE mode)
    if (programMode == COMMANDLINE) {
        commandLineView->placeCursor();
    } else {
        activeEditView()->placeCursor();
    }

    // 2h. Flush
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::resetPrompt
//
// this method resets the prompt to the empty string.  Many commands set the prompt text to
// a result string so the user knows something happend.  Any keystroke after calls resetPrompt
// to erase the message.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::resetPrompt(void)
{
    if (commandLineView->getPrompt() != "") {
        commandLineView->setPrompt("");
        commandLineView->setText("");
        commandLineView->updateScreen();
        activeEditView()->placeCursor();
        screen->flush();
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::setMessage
//
// this method resets the prompt to the empty string.  Many commands set the prompt text to
// a result string so the user knows something happend.  Any keystroke after calls resetPrompt
// to erase the message.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::setMessage(CxString message)
{
    commandLineView->setPrompt(message);
    commandLineView->setText("");
    commandLineView->updateScreen();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::setMessageWithLocation
//
// Helper: formats a message with the current cursor location and displays it.
// Used by find/replace handlers to show "(prefix) loc=(row,col)".
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::setMessageWithLocation(CxString prefix)
{
    CxEditBufferPosition loc = activeEditView()->cursorPosition();
    char buffer[200];
    sprintf(buffer, "loc=(%lu,%lu)", loc.row, loc.col);
    setMessage(prefix + " " + CxString(buffer));
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::showProjectView
//
// Helper: recalculates, redraws and switches to the file list view.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::showProjectView(void)
{
    // flush any pending output before drawing modal
    screen->flush();

    screen->hideCursor();
    projectView->setVisible(1);
    projectView->rebuildVisibleItems();    // refresh for current buffers
    projectView->recalcScreenPlacements();
    projectView->redraw();  // draws modal on top of existing screen content
    programMode = PROJECTVIEW;
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::showHelpView
//
// Helper: recalculates, redraws and switches to the help text view.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::showHelpView(void)
{
    FILE *dbg = fopen("/tmp/helpview_debug.log", "a");
    if (dbg) { fprintf(dbg, "showHelpView: start\n"); fflush(dbg); }

    screen->flush();
    screen->hideCursor();
    helpView->setVisible(1);
    helpView->rebuildVisibleItems();
    helpView->recalcScreenPlacements();

    if (dbg) { fprintf(dbg, "showHelpView: about to redraw\n"); fflush(dbg); }

    helpView->redraw();

    if (dbg) { fprintf(dbg, "showHelpView: redraw done, setting mode to HELPVIEW\n"); fflush(dbg); fclose(dbg); }

    programMode = HELPVIEW;
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::showBuildView
//
// Helper: recalculates, redraws and switches to the build output view.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::showBuildView(void)
{
    // flush any pending output before drawing modal
    screen->flush();

    screen->hideCursor();
    buildView->setVisible(1);
    buildView->recalcScreenPlacements();
    buildView->redraw();  // draws modal on top of existing screen content
    programMode = BUILDVIEW;
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::startBuild
//
// Start a build for a specific subproject with an optional make target.
// Builds the command: cd <makeDir> && make [target]
// Sets build context on BuildOutput for jump-to-error path resolution.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::startBuild(ProjectSubproject *sub, CxString makeTarget)
{
    _activeBuildSubproject = sub;

    // Get make directory
    CxString makeDir = project->getMakeDirectory(sub);

    // Build command: cd <dir> && make [target]
    CxString command = "cd ";
    command += makeDir;
    command += " && make";
    if (makeTarget.length() > 0) {
        command += " ";
        command += makeTarget;
    }

    // Set build context for error path resolution
    buildOutput->setBuildContext(makeDir, sub->name);

    // Start the build
    buildOutput->start(command);

    // Set the build status prefix for command line
    CxString prefix = "(Building ";
    prefix += sub->name;
    prefix += "...)";
    _buildStatusPrefix = prefix;
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::startBuildAll
//
// Start building all subprojects in sequence. Clears previous output,
// sets up build-all state, and kicks off the first subproject.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::startBuildAll(CxString makeTarget)
{
    if (project == NULL || project->buildOrderCount() == 0) {
        setMessage("(no subprojects to build)");
        return;
    }

    _buildAllIndex = 0;
    _buildAllTarget = makeTarget;

    // Clear previous build output
    buildOutput->clear();

    // Start the first subproject
    continueBuildAll();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::continueBuildAll
//
// Continue the build-all sequence by starting the next subproject.
// Appends a separator line and starts the build without clearing output.
// Called by startBuildAll() for the first subproject and by buildIdleCallback()
// for subsequent subprojects.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::continueBuildAll(void)
{
    if (_buildAllIndex < 0 || _buildAllIndex >= project->buildOrderCount()) {
        _buildAllIndex = -1;
        return;
    }

    ProjectSubproject *sub = project->buildOrderAt(_buildAllIndex);
    _activeBuildSubproject = sub;

    // Append separator line
    CxString sep = "Building ";
    sep += sub->name;
    buildOutput->appendSeparator(sep);

    // Get make directory and build command
    CxString makeDir = project->getMakeDirectory(sub);

    CxString command = "cd ";
    command += makeDir;
    command += " && make";
    if (_buildAllTarget.length() > 0) {
        command += " ";
        command += _buildAllTarget;
    }

    // Set build context for error path resolution
    buildOutput->setBuildContext(makeDir, sub->name);

    // Start without clearing (preserves accumulated output)
    buildOutput->startNext(command);

    // Set status prefix
    CxString prefix = "(Building ";
    prefix += sub->name;
    prefix += "...)";
    _buildStatusPrefix = prefix;
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::buildIdleCallback
//
// Called during keyboard idle (~100ms intervals) to poll build output.
// Updates the build view if new output is available.
// Updates command line status when build completes.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::buildIdleCallback(void)
{
    if (buildOutput == NULL) {
        return;
    }

    if (buildOutput->isRunning()) {
        if (buildOutput->poll()) {
            // new lines available - scroll to end and redraw
            if (programMode == BUILDVIEW) {
                buildView->advanceSpinner();
                buildView->scrollToEnd();
                buildView->redraw();
            }
        } else {
            // no new lines, but still running - advance spinner
            if (programMode == BUILDVIEW) {
                buildView->advanceSpinner();
                buildView->redraw();
            }
        }
    } else if (buildOutput->isComplete() && _buildStatusPrefix.length() > 0) {
        // Build just finished

        // Check if this is part of a build-all sequence
        if (_buildAllIndex >= 0) {
            if (buildOutput->exitCode() == 0) {
                _buildAllIndex++;
                if (_buildAllIndex < project->buildOrderCount()) {
                    // More subprojects to build - continue
                    continueBuildAll();
                    if (programMode == BUILDVIEW) {
                        buildView->scrollToEnd();
                        buildView->redraw();
                    }
                    return;
                }
            }
            // Error or last subproject - done with build-all
            _buildAllIndex = -1;
        }

        // Update status message
        int errCount = buildOutput->errorCount();
        int warnCount = buildOutput->warningCount();
        char msg[100];
        if (errCount == 0 && warnCount == 0) {
            sprintf(msg, "(Build Done - no errors)");
        } else {
            sprintf(msg, "(Build Done - %d error%s, %d warning%s)",
                    errCount, errCount == 1 ? "" : "s",
                    warnCount, warnCount == 1 ? "" : "s");
        }
        setMessage(msg);
        _buildStatusPrefix = "";  // clear prefix, final message set

        // Scroll to end to show "Build Done" line if view is open
        if (programMode == BUILDVIEW) {
            buildView->scrollToEnd();
            buildView->redraw();
        }
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::returnToEditMode
//
// Helper: dismisses any modal view and redraws all editors.
// Handles split screen mode correctly.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::returnToEditMode(void)
{
    screen->showCursor();
    programMode = ScreenEditor::EDIT;

    screen->clearScreen();
    editView->updateScreen();
    if (_splitMode == 1 && editViewBottom != NULL) {
        editViewBottom->updateScreen();
    }
    activeEditView()->placeCursor();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::enterCommandLineMode
//
// Helper: transitions from EDIT mode to COMMANDLINE mode.
// Sets up the command input state and places cursor on command line.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::enterCommandLineMode(void)
{
    resetPrompt();
    programMode = ScreenEditor::COMMANDLINE;
    enterCommandMode();
    commandLineView->placeCursor();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::exitCommandLineMode
//
// Helper: transitions from COMMANDLINE mode back to EDIT mode.
// Places cursor back on the edit view and flushes screen.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::exitCommandLineMode(void)
{
    programMode = ScreenEditor::EDIT;
    activeEditView()->placeCursor();
    screen->flush();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::resetCommandInputState
//
// Helper: resets all command input state variables to idle.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::resetCommandInputState(void)
{
    _cmdInputState = CMD_INPUT_IDLE;
    _activeCompleter = &_commandCompleter;
    _currentCommand = NULL;
    _cmdBuffer = "";
    _argBuffer = "";
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::run
//
// This is the main keyloop for the program.  Key presses are gathered and then dispatched
// to the edit window or the command window depending on the current mode of application.
// Pressing a
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::run(void)
{
    
    // starting mode is the editor
    programMode = ScreenEditor::EDIT;
    
    while(1) {
        
        //-----------------------------------------------------------------------------------------
        // get the next key
        //-----------------------------------------------------------------------------------------
        CxKeyAction keyAction = keyboard->getAction();

#if 0 // MCP disabled for debugging
#if defined(_LINUX_) || defined(_OSX_)
        //-----------------------------------------------------------------------------------------
        // check if MCP handler modified buffers and needs a redraw
        //-----------------------------------------------------------------------------------------
        if (_mcpHandler != NULL && _mcpHandler->needsRedraw()) {
            _mcpHandler->clearNeedsRedraw();
            activeEditView()->reframeAndUpdateScreen();
            activeEditView()->placeCursor();
            screen->flush();
        }
#endif
#endif

        //-----------------------------------------------------------------------------------------
        // based on the edit mode call a different focus routine
        //-----------------------------------------------------------------------------------------
        switch( programMode ) {
                
            //-------------------------------------------------------------------------------------
            // current focus is the editor
            //-------------------------------------------------------------------------------------
            case ScreenEditor::EDIT:
            {
                if (focusEditor( keyAction )) {
                    
                    //-----------------------------------------------------------------------------
                    // the editor was quit
                    //-----------------------------------------------------------------------------
                    
                    return;
                }
            }
            break;
                
            //-------------------------------------------------------------------------------------
            // current focus is the commandline
            //-------------------------------------------------------------------------------------
            case ScreenEditor::COMMANDLINE:
            {
                focusCommandPrompt( keyAction );
            }
			break;
                
            //-------------------------------------------------------------------------------------
            // current focus is the project file list
            //-------------------------------------------------------------------------------------
            case ScreenEditor::PROJECTVIEW:
            {
                focusProjectView( keyAction );
            }
            break;

            //-------------------------------------------------------------------------------------
            // current focus is the project file list
            //-------------------------------------------------------------------------------------
            case ScreenEditor::HELPVIEW:
            {
                focusHelpView( keyAction );
            }
            break;

            //-------------------------------------------------------------------------------------
            // current focus is the build output view
            //-------------------------------------------------------------------------------------
            case ScreenEditor::BUILDVIEW:
            {
                focusBuildView( keyAction );
            }
            break;
        }

        // check if quit was requested via ESC command
        if (_quitRequested) {
            return;
        }
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::focusEditor
//
// When the editor is the focus this handles the routing of keys, most keys go directly to
// the editView routeKeyAction routine. The only key intercepted are the ESC (command) key that
// puts the editor into command mode, and the CONTROL key the performs action in the editview.
//
//-------------------------------------------------------------------------------------------------
int
ScreenEditor::focusEditor( CxKeyAction keyAction)
{
    switch (keyAction.actionType() )
    {
        //-----------------------------------------------------------------------------------------
        // handle escape key - enter command input mode
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::COMMAND:
        {
            enterCommandLineMode();
        }
        break;
            
        //-----------------------------------------------------------------------------------------
        // handle control key sequence
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::CONTROL:
        {
            if (handleControl( keyAction )) {
                return(1);
            }
            activeEditView()->placeCursor();
        }
        break;

        //-----------------------------------------------------------------------------------------
        // handle all other keys
        //-----------------------------------------------------------------------------------------
        default:
        {
            resetPrompt();
            activeEditView()->routeKeyAction( keyAction );
        }
        break;

    }

    return(0);
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::focusCommandPrompt
//
// When the command prompt is the focus this handles the routing of keys.  A comment sequence is
// started from the editor focus when a ESC key is pressed.  After that key all the keys will
// be routed to here until a NEWLINE or ESC key is pressed that executes the command on
// the commandline.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::focusCommandPrompt( CxKeyAction keyAction )
{
    handleCommandInput( keyAction );
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::focusProjectView
//
// When the project view is focused this handles key routing.  Enter on a file opens it,
// Enter on a subproject header toggles expand/collapse, ESC dismisses.
// S saves the selected file, A saves all modified files.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::focusProjectView( CxKeyAction keyAction )
{
    switch (keyAction.actionType() )
    {
        //-----------------------------------------------------------------------------------------
        // handle escape key - dismiss without loading
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::COMMAND:
        {
            projectView->setVisible(0);
            returnToEditMode();
        }
        break;

        //-----------------------------------------------------------------------------------------
        // handle enter - context-sensitive based on item type
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::NEWLINE:
        {
            ProjectViewItemType itemType = projectView->getSelectedItemType();

            if (itemType == PVITEM_FILE || itemType == PVITEM_OPEN_FILE) {
                CxString filePath = projectView->getSelectedItem();
                if (filePath.length() > 0) {
                    projectView->setVisible(0);
                    loadNewFile(filePath, TRUE);
                    returnToEditMode();
                }
            } else if (itemType == PVITEM_SUBPROJECT || itemType == PVITEM_OPEN_HEADER) {
                projectView->toggleSelectedSubproject();
                projectView->redraw();
            }
            // PVITEM_ALL: no action on Enter
        }
        break;

        //-----------------------------------------------------------------------------------------
        // normal input characters
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::LOWERCASE_ALPHA:
        case CxKeyAction::UPPERCASE_ALPHA:
        {
            ProjectViewItemType selType = projectView->getSelectedItemType();

            // make (build) - only on ALL or SUBPROJECT
            if (keyAction.tag() == 'm' || keyAction.tag() == 'M')
            {
                if (selType == PVITEM_ALL || selType == PVITEM_SUBPROJECT) {
                    if (buildOutput->isRunning()) {
                        setMessage("(build already running)");
                    } else if (project != NULL && project->subprojectCount() > 0) {
                        ProjectSubproject *sub = projectView->getSelectedSubproject();
                        if (sub == NULL) {
                            startBuildAll("");
                        } else {
                            startBuild(sub, "");
                        }
                        projectView->setVisible(0);
                        showBuildView();
                        return;
                    }
                }
            }

            // clean - only on ALL or SUBPROJECT
            if (keyAction.tag() == 'c' || keyAction.tag() == 'C')
            {
                if (selType == PVITEM_ALL || selType == PVITEM_SUBPROJECT) {
                    if (buildOutput->isRunning()) {
                        setMessage("(build already running)");
                    } else if (project != NULL && project->subprojectCount() > 0) {
                        ProjectSubproject *sub = projectView->getSelectedSubproject();
                        if (sub == NULL) {
                            startBuildAll("clean");
                        } else {
                            startBuild(sub, "clean");
                        }
                        projectView->setVisible(0);
                        showBuildView();
                        return;
                    }
                }
            }

            // test - only on ALL or SUBPROJECT
            if (keyAction.tag() == 't' || keyAction.tag() == 'T')
            {
                if (selType == PVITEM_ALL || selType == PVITEM_SUBPROJECT) {
                    if (buildOutput->isRunning()) {
                        setMessage("(build already running)");
                    } else if (project != NULL && project->subprojectCount() > 0) {
                        ProjectSubproject *sub = projectView->getSelectedSubproject();
                        if (sub == NULL) {
                            startBuildAll("test");
                        } else {
                            startBuild(sub, "test");
                        }
                        projectView->setVisible(0);
                        showBuildView();
                        return;
                    }
                }
            }

            // save - context-sensitive
            if (keyAction.tag() == 's' || keyAction.tag() == 'S')
            {
                if (selType == PVITEM_FILE || selType == PVITEM_OPEN_FILE) {
                    // save single file
                    CxString filePath = projectView->getSelectedItem();
                    CmEditBuffer *buffer = editBufferList->findPath(filePath);
                    if (buffer != NULL && buffer->isTouched()) {
                        buffer->saveText(buffer->getFilePath());
                        setMessage("(Saved)");
                    }
                } else if (selType == PVITEM_SUBPROJECT) {
                    // save all modified files in this subproject
                    ProjectSubproject *sub = projectView->getSelectedSubproject();
                    if (sub != NULL) {
                        int savedCount = 0;
                        for (int f = 0; f < (int)sub->files.entries(); f++) {
                            CxString resolved = project->resolveFilePath(sub, sub->files.at(f));
                            CmEditBuffer *buffer = editBufferList->findPath(resolved);
                            if (buffer != NULL && buffer->isTouched()) {
                                buffer->saveText(buffer->getFilePath());
                                savedCount++;
                            }
                        }
                        if (savedCount > 0) {
                            CxString msg = CxString("(Saved ") + CxString(savedCount) + CxString(" files)");
                            setMessage(msg);
                        } else {
                            setMessage("(No modified files)");
                        }
                    }
                }
                projectView->redraw();
            }

            // save all modified files
            if (keyAction.tag() == 'a' || keyAction.tag() == 'A')
            {
                int savedCount = 0;
                for (int i = 0; i < editBufferList->items(); i++) {
                    CmEditBuffer *buffer = editBufferList->at(i);
                    if (buffer != NULL && buffer->isTouched()) {
                        buffer->saveText(buffer->getFilePath());
                        savedCount++;
                    }
                }
                if (savedCount > 0) {
                    CxString msg = CxString("(Saved ") + CxString(savedCount) + CxString(" files)");
                    setMessage(msg);
                } else {
                    setMessage("(No modified files to save)");
                }
                projectView->redraw();
            }

            // new file - only on SUBPROJECT or OPEN_HEADER
            if (keyAction.tag() == 'n' || keyAction.tag() == 'N')
            {
                if (selType == PVITEM_SUBPROJECT || selType == PVITEM_OPEN_HEADER) {
                    // store context for CMD_NewBuffer
                    _newFileSubproject = projectView->getSelectedSubproject();  // NULL for Other Files
                    _newFileFromProjectView = 1;

                    // dismiss project view
                    projectView->setVisible(0);
                    returnToEditMode();

                    // build pre-filled path
                    CxString dirPath;
                    if (_newFileSubproject != NULL) {
                        dirPath = project->getMakeDirectory(_newFileSubproject);
                    } else {
                        char cwd[1024];
                        getcwd(cwd, sizeof(cwd));
                        dirPath = CxString(cwd);
                    }
                    dirPath += "/";

                    // enter command line mode and select file-new
                    programMode = ScreenEditor::COMMANDLINE;
                    _cmdInputState = CMD_INPUT_COMMAND;
                    _cmdBuffer = "";
                    _argBuffer = "";
                    _currentCommand = NULL;
                    _activeCompleter = &_commandCompleter;

                    // find and select the file-new command entry
                    for (int i = 0; commandTable[i].name != NULL; i++) {
                        if (CxString(commandTable[i].name) == "file-new") {
                            selectCommand(&commandTable[i]);
                            break;
                        }
                    }

                    // pre-fill argument with directory path
                    _argBuffer = dirPath;
                    updateArgumentDisplay();
                    commandLineView->placeCursor();
                }
            }
        }
        break;

        //-----------------------------------------------------------------------------------------
        // handle all other keys
        //-----------------------------------------------------------------------------------------
        default:
        {
            projectView->routeKeyAction( keyAction );
        }
        break;
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::focusHelpView
//
// When the help view is the focus this handles the routing of keys. ESC or ENTER dismisses
// the help view and returns to editing.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::focusHelpView( CxKeyAction keyAction )
{
    switch (keyAction.actionType() )
    {
        //-----------------------------------------------------------------------------------------
        // handle escape key - dismiss help view
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::COMMAND:
        {
            helpView->setVisible(0);
            returnToEditMode();
        }
        break;

        //-----------------------------------------------------------------------------------------
        // handle enter key - toggle section or dismiss
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::NEWLINE:
        {
            HelpViewItemType itemType = helpView->getSelectedItemType();
            if (itemType == HELPITEM_SECTION) {
                helpView->toggleSelectedSection();
                helpView->redraw();
            } else {
                // dismiss on non-section items
                helpView->setVisible(0);
                returnToEditMode();
            }
        }
        break;

        //-----------------------------------------------------------------------------------------
        // handle all other keys (arrows)
        //-----------------------------------------------------------------------------------------
        default:
        {
            helpView->routeKeyAction( keyAction );
        }
        break;
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::focusBuildView
//
// When the build view is the focus this handles the routing of keys.
// ESC dismisses the dialog (build continues in background).
// Enter on an error line navigates to the file:line.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::focusBuildView( CxKeyAction keyAction )
{
    switch (keyAction.actionType() )
    {
        //-----------------------------------------------------------------------------------------
        // handle escape key - dismiss build view (build continues in background)
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::COMMAND:
        {
            buildView->setVisible(0);
            returnToEditMode();
        }
        break;

        //-----------------------------------------------------------------------------------------
        // handle enter - goto file:line if on error
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::NEWLINE:
        {
            if (buildView->hasSelectedError()) {
                BuildOutputLine *line = buildView->getSelectedLine();
                if (line != NULL && line->filename.length() > 0) {
                    // Get file and line info
                    CxString filename = line->filename;
                    int lineNum = line->line;
                    int colNum = line->column;

                    // Resolve path using build directory context
                    CxString buildDir = buildOutput->getBuildDirectory();
                    if (buildDir.length() > 0) {
                        filename = buildDir;
                        filename += "/";
                        filename += line->filename;
                    }

                    // Dismiss build view first
                    buildView->setVisible(0);
                    returnToEditMode();

                    // Check if file is already open
                    CmEditBuffer *targetBuffer = editBufferList->findPath(filename);

                    if (targetBuffer == NULL) {
                        // Load the file
                        int loaded = loadNewFile(filename, 1);
                        if (!loaded) {
                            setMessage(CxString("(cannot open file: ") + filename + ")");
                            return;
                        }
                        targetBuffer = activeEditView()->getEditBuffer();
                    } else {
                        // Switch to the buffer
                        activeEditView()->setEditBuffer(targetBuffer);
                    }

                    // Go to the line (convert 1-based to 0-based)
                    unsigned long targetLine = (lineNum > 0) ? lineNum - 1 : 0;
                    unsigned long targetCol = (colNum > 0) ? colNum - 1 : 0;

                    targetBuffer->cursorGotoRequest(targetLine, targetCol);
                    activeEditView()->reframeAndUpdateScreen();

                    char msg[120];
                    sprintf(msg, "(%s:%d)", filename.data(), lineNum);
                    setMessage(msg);
                }
            }
        }
        break;

        //-----------------------------------------------------------------------------------------
        // handle all other keys - pass to build view
        //-----------------------------------------------------------------------------------------
        default:
        {
            buildView->routeKeyAction( keyAction );
        }
        break;
    }
}


//-------------------------------------------------------------------------------------------------
//
// ESC COMMAND INPUT SYSTEM
//
// Overview:
//   When the user presses ESC, the editor enters a command input mode that provides
//   command completion similar to modern IDE command palettes. The user types
//   a command name, optionally followed by an argument, and the system provides
//   real-time completion hints.
//
// User Experience:
//   1. Press ESC to enter command mode - shows "command> " prompt
//   2. Type characters - matching commands appear as hints (e.g., "f  | find | find-all")
//   3. Press TAB to complete the common prefix
//   4. Press ENTER to execute when a unique match exists
//   5. Press SPACE to transition to argument input if command takes arguments
//   6. Press ESC again to cancel and return to editing
//
// Smart Completion Behavior:
//   - Single match: auto-completes the command name
//   - Commands with arguments: automatically shows argument prompt
//   - Commands without arguments: completes name, waits for ENTER to execute
//   - Exact match hides redundant completion hint (typing "quit" shows "quit", not "quit | quit")
//
// State Machine:
//   CMD_INPUT_IDLE ──(ESC)──> CMD_INPUT_COMMAND ──(match+args)──> CMD_INPUT_ARGUMENT
//        ^                          │                                    │
//        │                          │ (ESC or error)                     │ (ESC)
//        └──────────────────────────┴────────────────────────────────────┘
//        │                          │
//        │                          │ (ENTER on no-arg command)
//        └──────────────────────────┘
//
// Key State Variables:
//   _cmdInputState  - current state (IDLE, COMMAND, or ARGUMENT)
//   _cmdBuffer      - command name being typed
//   _argBuffer      - argument being typed (in ARGUMENT state)
//   _currentCommand - selected CommandEntry (set when transitioning to ARGUMENT or executing)
//
// Key Functions:
//   enterCommandMode()      - entry point from ESC key
//   handleCommandInput()    - dispatcher based on current state
//   handleCommandModeInput()   - handles typing command name
//   handleArgumentModeInput()  - handles typing argument
//   selectCommand()         - handles matched command (args -> prompt, no args -> complete)
//   cancelCommandInput()    - cleanup and return to edit mode
//   executeCurrentCommand() - run the command handler
//
// Command Table:
//   Commands are defined in CommandTable.cpp. Each entry has:
//   - name: the command name (e.g., "find", "goto-line")
//   - argHint: hint text for argument (e.g., "<pattern>", "<line>")
//   - description: help text
//   - flags: CMD_FLAG_NEEDS_ARG, CMD_FLAG_OPTIONAL_ARG
//   - handler: function pointer to ScreenEditor method
//
//   The Completer library provides literal prefix matching, so typing
//   "goto" matches "goto-line" and "buf" matches all "buffer-*" commands.
//
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// ScreenEditor::enterCommandMode
//
// Entry point for ESC command input. Called when user presses ESC in edit mode.
//
// Initializes all command input state variables and displays the "command> " prompt
// with a list of all available commands as completion hints.
//
// State transition: any state -> CMD_INPUT_COMMAND
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::enterCommandMode( void )
{
    _cmdInputState = CMD_INPUT_COMMAND;
    _cmdBuffer = "";
    _argBuffer = "";
    _currentCommand = NULL;
    _activeCompleter = &_commandCompleter;

    // reset project view new-file state so direct ESC usage doesn't inherit stale context
    _newFileFromProjectView = 0;
    _newFileSubproject = NULL;

    updateCommandDisplay();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::cancelCommandInput
//
// Cancels command input and returns to edit mode. This is the single cleanup point
// for all exit paths from command input (ESC pressed, error conditions, etc.).
//
// Resets all state variables:
//   _cmdInputState  -> CMD_INPUT_IDLE
//   _cmdBuffer      -> ""
//   _argBuffer      -> ""
//   _currentCommand -> NULL
//   programMode     -> EDIT
//
// Also clears the command line display and restores cursor to edit view.
//
// State transition: CMD_INPUT_COMMAND or CMD_INPUT_ARGUMENT -> CMD_INPUT_IDLE
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::cancelCommandInput( void )
{
    resetCommandInputState();

    commandLineView->setText("");
    commandLineView->setPrompt("");
    commandLineView->updateScreen();

    exitCommandLineMode();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::selectCommand
//
// Handles a matched command during typing or TAB completion. This function determines
// what to do based on whether the command takes arguments:
//
//   Command takes arguments (CMD_FLAG_NEEDS_ARG or CMD_FLAG_OPTIONAL_ARG):
//     -> Transition to CMD_INPUT_ARGUMENT state
//     -> Show argument prompt: "command-name <hint>: "
//     -> Set _currentCommand so executeCurrentCommand knows what to run
//
//   Command takes NO arguments:
//     -> Stay in CMD_INPUT_COMMAND state
//     -> Auto-complete the command name in _cmdBuffer
//     -> User must press ENTER to execute (prevents accidental execution)
//
// Note: This function is called from typing/TAB/SPACE, NOT from ENTER.
// The ENTER handler calls executeCurrentCommand() directly for no-arg commands.
//
// State transition:
//   - With args: CMD_INPUT_COMMAND -> CMD_INPUT_ARGUMENT
//   - Without args: stays in CMD_INPUT_COMMAND (name completed, awaiting ENTER)
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::selectCommand( CommandEntry *cmd )
{
    int takesSymbolArg = (cmd->flags & CMD_FLAG_SYMBOL_ARG);
    int takesFreeformArg = (cmd->flags & (CMD_FLAG_NEEDS_ARG | CMD_FLAG_OPTIONAL_ARG))
                           && !takesSymbolArg;

    if (takesFreeformArg) {
        // go to freeform argument input mode
        _currentCommand = cmd;
        _cmdInputState = CMD_INPUT_ARGUMENT;
        _argBuffer = "";
        updateArgumentDisplay();
    }
    else {
        // no-arg command or symbol command (handled by child completer transition)
        // complete the name but wait for ENTER to execute
        _cmdBuffer = cmd->name;
        updateCommandDisplay();
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::updateCommandDisplay
//
// Renders the command line during CMD_INPUT_COMMAND state. Shows the typed text
// followed by matching command hints.
//
// Display format:
//   "command> typed  | match1 <arg> | match2 <arg> | ..."
//
// Examples:
//   Empty input:    "command>   | find <pattern> | find-all <pattern> | goto-line <line> | ..."
//   Partial input:  "command> f  | find <pattern> | find-all <pattern>"
//   Exact match:    "command> quit" (no hints shown when typed text equals command name)
//
// The cursor is positioned after the typed text (not after the hints), so the user
// can continue typing naturally.
//
// Called whenever _cmdBuffer changes during command name input.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::renderCommandLine( CxString prefix, CxString display, unsigned long cursorOffset )
{
    CxString fullLine = prefix;
    fullLine += display;

    unsigned long targetWidth = screen->cols();
    if (targetWidth > 2) targetWidth -= 2;
    if (fullLine.length() > targetWidth) {
        fullLine = fullLine.subString(0, targetWidth);
    }
    while (fullLine.length() < targetWidth) {
        fullLine += " ";
    }

    screen->resetColors();
    screen->writeTextAt( screen->rows() - 1, 1, fullLine, true );

    screen->placeCursor( screen->rows() - 1, 1 + cursorOffset );
    screen->flush();
}


void
ScreenEditor::updateCommandDisplayForSymbol( void )
{
    CxString prefix = "";
    if (_currentCommand != NULL) {
        prefix = _currentCommand->name;
        prefix += " ";
        if (_currentCommand->argHint != NULL) {
            prefix += _currentCommand->argHint;
            prefix += ": ";
        }
    }

    CxString names[16];
    int count = _activeCompleter->findMatches( _cmdBuffer, names, 16 );

    int isExactMatch = (count == 1 && _cmdBuffer == names[0]);

    CxString symDisplay = _cmdBuffer;

    if (count > 0 && !isExactMatch) {
        symDisplay += "  ";
        for (int i = 0; i < count && i < 6; i++) {
            symDisplay += "| ";
            symDisplay += names[i];
            symDisplay += " ";
        }
        if (count > 6) {
            symDisplay += "...";
        }
    }

    renderCommandLine( prefix, symDisplay, prefix.length() + _cmdBuffer.length() );
}


void
ScreenEditor::updateCommandDisplay( void )
{
    if (_activeCompleter != &_commandCompleter) {
        updateCommandDisplayForSymbol();
        return;
    }

    // command level - show matching commands with arg hints
    CxString display = _cmdBuffer;

    if (_cmdBuffer.length() == 0) {
        // No input yet: show category prefixes for discoverability
        CompleterCandidate *allMatches[32];
        int allCount = _activeCompleter->findMatchesFull( _cmdBuffer, allMatches, 32 );

        CxString categories[16];
        int categoryCount = 0;

        for (int i = 0; i < allCount; i++) {
            CxString name = allMatches[i]->name;
            int dashIdx = name.index("-");
            CxString cat;
            if (dashIdx > 0) {
                cat = name.subString(0, dashIdx + 1);
            } else {
                cat = name;
            }

            int found = 0;
            for (int j = 0; j < categoryCount; j++) {
                if (categories[j] == cat) {
                    found = 1;
                    break;
                }
            }
            if (!found && categoryCount < 16) {
                categories[categoryCount++] = cat;
            }
        }

        display += "  ";
        for (int i = 0; i < categoryCount; i++) {
            display += "| ";
            display += categories[i];
            display += " ";
        }
    } else {
        CompleterCandidate *matches[16];
        int count = _activeCompleter->findMatchesFull( _cmdBuffer, matches, 16 );

        // don't show hint if typed text exactly matches a command
        int isExactMatch = (count == 1 && _cmdBuffer == matches[0]->name);

        if (count > 0 && !isExactMatch) {
            display += "  ";
            for (int i = 0; i < count && i < 8; i++) {
                display += "| ";
                display += matches[i]->name;
                CommandEntry *entry = (CommandEntry *)matches[i]->userData;
                if (entry != NULL && entry->argHint != NULL) {
                    display += " ";
                    display += entry->argHint;
                }
                display += " ";
            }
            if (count > 8) {
                display += "...";
            }
        }
    }

    renderCommandLine( "command> ", display, 9 + _cmdBuffer.length() );
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::handleCommandInput
//
// Main dispatcher for command input mode. Routes keystrokes to the appropriate
// handler based on the current state:
//
//   CMD_INPUT_COMMAND  -> handleCommandModeInput()  (typing command name)
//   CMD_INPUT_ARGUMENT -> handleArgumentModeInput() (typing argument)
//
// Called from the main event loop when programMode indicates command input is active.
// This simple dispatcher keeps the state machine logic clean and each handler focused.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::handleCommandInput( CxKeyAction keyAction )
{
    if (_cmdInputState == CMD_INPUT_COMMAND) {
        handleCommandModeInput( keyAction );
    }
    else if (_cmdInputState == CMD_INPUT_ARGUMENT) {
        handleArgumentModeInput( keyAction );
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::handleCommandModeInput
//
// Handles keystrokes while in CMD_INPUT_COMMAND state (typing the command name).
//
// Key behaviors:
//
//   ESC       - Cancel command input, return to edit mode
//
//   BACKSPACE - Delete last character from _cmdBuffer, update display
//
//   ENTER     - Execute if there's a match:
//               * Exact match preferred (e.g., "find" matches "find" not "find-all")
//               * Unique prefix match accepted (e.g., "qui" matches "quit")
//               * Ambiguous -> error message, cancel
//               * No match -> error message, cancel
//               * For commands with args: transition to argument input
//               * For commands without args: execute immediately
//
//   TAB       - Complete the common prefix among matches:
//               * Single match -> complete name, then selectCommand()
//               * Multiple matches -> complete common prefix, show hints
//               * No matches -> error message
//
//   SPACE     - If typed text is an exact command match, transition to argument input
//               (allows "find " to go to argument mode even with "find-all" existing)
//
//   Printable - Add character to _cmdBuffer:
//               * Single match -> call selectCommand() (may auto-transition)
//               * Multiple/no matches -> update display with hints
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::handleCommandEnter( void )
{
    CompleterResult result = _activeCompleter->processEnter( _cmdBuffer );

    switch (result.getStatus()) {
        case COMPLETER_SELECTED:
        {
            if (_activeCompleter == &_commandCompleter) {
                // command level - selected a command
                _currentCommand = (CommandEntry *)result.getSelectedData();
                _cmdBuffer = result.getInput();

                // if it has a child completer, transition to child level
                if (result.getNextLevel() != NULL) {
                    _activeCompleter = result.getNextLevel();
                    _cmdBuffer = "";
                    updateCommandDisplay();
                }
                else if (_currentCommand->flags & (CMD_FLAG_NEEDS_ARG | CMD_FLAG_OPTIONAL_ARG)) {
                    selectCommand( _currentCommand );
                }
                else {
                    executeCurrentCommand();
                }
            }
            else {
                // child completer level - selected a symbol
                // _currentCommand was set during NEXT_LEVEL transition
                _argBuffer = result.getSelectedName();
                executeCurrentCommand();
            }
        }
        break;

        case COMPLETER_NEXT_LEVEL:
        {
            _currentCommand = (CommandEntry *)result.getSelectedData();
            _activeCompleter = result.getNextLevel();
            _cmdBuffer = "";
            updateCommandDisplay();
        }
        break;

        case COMPLETER_MULTIPLE:
            setMessage( "(ambiguous command)" );
            cancelCommandInput();
            break;

        default:
            setMessage( "(unknown command)" );
            cancelCommandInput();
            break;
    }
}


void
ScreenEditor::handleCommandTab( void )
{
    CompleterResult result = _activeCompleter->processTab( _cmdBuffer );
    _cmdBuffer = result.getInput();

    switch (result.getStatus()) {
        case COMPLETER_UNIQUE:
        {
            CommandEntry *cmd = (CommandEntry *)result.getSelectedData();
            if (_activeCompleter == &_commandCompleter) {
                selectCommand( cmd );
            } else {
                // child level - complete symbol name, wait for ENTER
                updateCommandDisplay();
            }
        }
        break;

        case COMPLETER_NEXT_LEVEL:
        {
            _currentCommand = (CommandEntry *)result.getSelectedData();
            _activeCompleter = result.getNextLevel();
            _cmdBuffer = "";
            updateCommandDisplay();
        }
        break;

        case COMPLETER_PARTIAL:
        case COMPLETER_MULTIPLE:
            updateCommandDisplay();
            break;

        case COMPLETER_NO_MATCH:
            setMessage( "(no match)" );
            break;

        default:
            break;
    }
}


void
ScreenEditor::handleCommandChar( CxKeyAction keyAction )
{
    char c = (char)keyAction.tag().charAt(0);
    CompleterResult result = _activeCompleter->processChar( _cmdBuffer, c );

    switch (result.getStatus()) {
        case COMPLETER_NEXT_LEVEL:
        {
            _currentCommand = (CommandEntry *)result.getSelectedData();
            _activeCompleter = result.getNextLevel();
            _cmdBuffer = "";
            updateCommandDisplay();
        }
        break;

        case COMPLETER_UNIQUE:
        {
            _cmdBuffer = result.getInput();
            if (_activeCompleter == &_commandCompleter) {
                CommandEntry *cmd = (CommandEntry *)result.getSelectedData();
                selectCommand( cmd );
            } else {
                // child level - symbol auto-completed, show it
                updateCommandDisplay();
            }
        }
        break;

        case COMPLETER_PARTIAL:
        case COMPLETER_MULTIPLE:
            _cmdBuffer = result.getInput();
            updateCommandDisplay();
            break;

        case COMPLETER_NO_MATCH:
            // reject the character - don't update _cmdBuffer
            break;

        default:
            _cmdBuffer = result.getInput();
            updateCommandDisplay();
            break;
    }
}


void
ScreenEditor::handleCommandModeInput( CxKeyAction keyAction )
{
    // ESCAPE - cancel
    if (keyAction.actionType() == CxKeyAction::COMMAND) {
        cancelCommandInput();
        return;
    }

    // BACKSPACE - delete last character
    if (keyAction.actionType() == CxKeyAction::BACKSPACE) {
        if (_cmdBuffer.length() > 0) {
            _cmdBuffer = _cmdBuffer.subString( 0, _cmdBuffer.length() - 1 );
            updateCommandDisplay();
        }
        return;
    }

    // RETURN - execute via Completer
    if (keyAction.actionType() == CxKeyAction::NEWLINE) {
        handleCommandEnter();
        return;
    }

    // TAB - complete via Completer
    if (keyAction.actionType() == CxKeyAction::TAB) {
        handleCommandTab();
        return;
    }

    // SPACE - transition to argument if at command level with exact match
    if (keyAction.tag() == ' ') {
        if (_activeCompleter == &_commandCompleter) {
            CompleterResult result = _activeCompleter->processEnter( _cmdBuffer );
            if (result.getStatus() == COMPLETER_SELECTED) {
                CommandEntry *cmd = (CommandEntry *)result.getSelectedData();
                selectCommand( cmd );
            }
        }
        return;
    }

    // Printable character - process via Completer
    if (keyAction.actionType() == CxKeyAction::LOWERCASE_ALPHA ||
        keyAction.actionType() == CxKeyAction::UPPERCASE_ALPHA ||
        keyAction.actionType() == CxKeyAction::NUMBER ||
        keyAction.actionType() == CxKeyAction::SYMBOL) {

        handleCommandChar( keyAction );
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::handleArgumentModeInput
//
// Handles keystrokes while in CMD_INPUT_ARGUMENT state (typing the command argument).
// At this point, _currentCommand is set to the selected command.
//
// Key behaviors:
//
//   ENTER     - Execute the command with _argBuffer as the argument.
//               For optional arguments, empty _argBuffer is valid.
//
//   ESC       - Cancel command input, return to edit mode without executing.
//
//   BACKSPACE - Delete last character from _argBuffer, update display.
//
//   Printable - Add character to _argBuffer, update display.
//
// The display shows: "command-name <hint>: argument-text"
// For example: "find <pattern>: hello"
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::handleArgumentModeInput( CxKeyAction keyAction )
{
    // RETURN - execute command
    if (keyAction.actionType() == CxKeyAction::NEWLINE) {
        executeCurrentCommand();
        return;
    }

    // ESCAPE - cancel
    if (keyAction.actionType() == CxKeyAction::COMMAND) {
        cancelCommandInput();
        return;
    }

    // BACKSPACE - delete last character
    if (keyAction.actionType() == CxKeyAction::BACKSPACE) {
        if (_argBuffer.length() > 0) {
            _argBuffer = _argBuffer.subString( 0, _argBuffer.length() - 1 );
            updateArgumentDisplay();
        }
        return;
    }

    // Printable character - add to argument (freeform text, no completion)
    if (keyAction.actionType() == CxKeyAction::LOWERCASE_ALPHA ||
        keyAction.actionType() == CxKeyAction::UPPERCASE_ALPHA ||
        keyAction.actionType() == CxKeyAction::NUMBER ||
        keyAction.actionType() == CxKeyAction::SYMBOL) {

        _argBuffer += keyAction.tag();
        updateArgumentDisplay();
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::updateArgumentDisplay
//
// Renders the command line during CMD_INPUT_ARGUMENT state. Shows the command name,
// argument hint, and typed argument text.
//
// Display format:
//   "command-name <hint>: typed-argument"
//
// Examples:
//   "find <pattern>: "           (no argument typed yet)
//   "find <pattern>: hello"      (user typed "hello")
//   "goto-line <line>: 42"       (user typed "42")
//   "save [filename]: "          (optional argument, hint shows brackets)
//
// The cursor is positioned after the typed argument text.
//
// Called whenever _argBuffer changes during argument input.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::updateArgumentDisplay( void )
{
    // build prompt: "command <hint>: "
    CxString prefix = _currentCommand->name;
    if (_currentCommand->argHint != NULL) {
        prefix += " ";
        prefix += _currentCommand->argHint;
    }
    prefix += ": ";

    CxString display = _argBuffer;

    renderCommandLine( prefix, display, prefix.length() + _argBuffer.length() );
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::executeCurrentCommand
//
// Executes the currently selected command (_currentCommand) with _argBuffer as
// the argument. This is the final step in the command input flow.
//
// Preconditions:
//   - _currentCommand must be set to a valid CommandEntry
//   - _argBuffer contains the argument (may be empty for no-arg or optional-arg commands)
//
// Execution flow:
//   1. Validate _currentCommand is not NULL
//   2. Check if handler is implemented (handler != NULL)
//   3. Call the handler method via function pointer: (this->*handler)(_argBuffer)
//   4. Reset all command input state
//   5. Return to EDIT mode with cursor in edit view
//
// If the command handler is NULL (not implemented), displays "(command not implemented)"
// and returns to edit mode without executing.
//
// State transition: CMD_INPUT_COMMAND or CMD_INPUT_ARGUMENT -> CMD_INPUT_IDLE
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::executeCurrentCommand( void )
{
    if (_currentCommand == NULL) {
        setMessage( "(no command)" );
        resetCommandInputState();
        exitCommandLineMode();
        return;
    }

    // check if handler exists
    if (_currentCommand->handler == NULL) {
        setMessage( "(command not implemented)" );
        resetCommandInputState();
        exitCommandLineMode();
        return;
    }

    // call the handler
    CommandHandler handler = _currentCommand->handler;
    (this->*handler)( _argBuffer );

    // clear internal command input state
    resetCommandInputState();

    // only return to edit mode if handler didn't switch to a different mode
    // (e.g., CMD_Help switches to HELPVIEW, CMD_BufferList switches to PROJECTVIEW)
    if (programMode == ScreenEditor::COMMANDLINE) {
        exitCommandLineMode();
    }
}

