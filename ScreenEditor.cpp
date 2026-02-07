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
#include <sys/types.h>

#include "EditView.h"
#include "CommandLineView.h"
#include "ScreenEditor.h"
#include "Project.h"

#ifdef CM_UTF8_SUPPORT
#include "UTFSymbols.h"
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
    commandLineView = NULL;
    editBufferList = NULL;
    fileListView = NULL;
    helpTextView = NULL;
    project = NULL;
    _activeCompleter = NULL;
    _currentCommand = NULL;
    _quitRequested = FALSE;
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

    // if (dbg) { fprintf(dbg, "14: about to create FileListView\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // create a file list view for the project
    //
    //---------------------------------------------------------------------------------------------

    fileListView = new FileListView(programDefaults,
                                    editBufferList,
                                    project,
                                    screen
                                    );

    // if (dbg) { fprintf(dbg, "15: FileListView created\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // create a help view for editor help
    //
    //---------------------------------------------------------------------------------------------

  	helpTextView = new HelpTextView(programDefaults,
									project,
									screen);

    // if (dbg) { fprintf(dbg, "16: HelpTextView created\n"); fflush(dbg); }

    helpTextView->loadHelpText("./cm.txt");

    // if (dbg) { fprintf(dbg, "17: help text loaded\n"); fflush(dbg); }

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

            editView->reframeAndUpdateScreen();
            editView->placeCursor();
            screen->flush();
        }
    }
}
#endif


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
        editView->placeCursor();
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
    CxEditBufferPosition loc = editView->cursorPosition();
    char buffer[200];
    sprintf(buffer, "loc=(%lu,%lu)", loc.row, loc.col);
    setMessage(prefix + " " + CxString(buffer));
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::showFileListView
//
// Helper: recalculates, redraws and switches to the file list view.
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::showFileListView(void)
{
    fileListView->recalcScreenPlacements();
    fileListView->redraw();
    programMode = FILELIST;
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
    helpTextView->recalcScreenPlacements();
    helpTextView->redraw();
    programMode = HELPVIEW;
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

#if defined(_LINUX_) || defined(_OSX_)
        //-----------------------------------------------------------------------------------------
        // check if MCP handler modified buffers and needs a redraw
        //-----------------------------------------------------------------------------------------
        if (_mcpHandler != NULL && _mcpHandler->needsRedraw()) {
            _mcpHandler->clearNeedsRedraw();
            editView->reframeAndUpdateScreen();
            editView->placeCursor();
            screen->flush();
        }
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
            case ScreenEditor::FILELIST:
            {
                focusFilelist( keyAction );
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
        // handle escape key - enter new command input mode
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::COMMAND:
        {
            resetPrompt();
            programMode = ScreenEditor::COMMANDLINE;

            //-------------------------------------------------------------------------------------
            // enter the new command input mode with tab completion
            //-------------------------------------------------------------------------------------
            enterCommandMode();
            commandLineView->placeCursor();
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
            editView->placeCursor();
        }
        break;
            
        //-----------------------------------------------------------------------------------------
        // handle all other keys
        //-----------------------------------------------------------------------------------------
        default:
        {
            resetPrompt();
            editView->routeKeyAction( keyAction );
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
// ScreenEditor::focusFilelist
//
// When the filelist is the focus this handles the routing of keys.  Most keys are passed through
// to the file list view but a few are intercepted.  newline selects a new file to edit and esc
// dismisses the window without loading a new buffer (like a cancel button)
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::focusFilelist( CxKeyAction keyAction )
{
    switch (keyAction.actionType() )
    {
        //-----------------------------------------------------------------------------------------
        // handle escape key
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::COMMAND:
        {
            programMode = ScreenEditor::EDIT;
            editView->updateScreen();
        }
        break;
                            
        //-----------------------------------------------------------------------------------------
        // handle a newline
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::NEWLINE:
        {
            programMode = ScreenEditor::EDIT;
            CxString pathName = fileListView->getSelectedItem();
            loadNewFile( pathName, TRUE );
            editView->updateScreen();
        }
        break;
            
            
        //-----------------------------------------------------------------------------------------
        // normal input characters
        //
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::LOWERCASE_ALPHA:
        case CxKeyAction::UPPERCASE_ALPHA:
        {
            
            // load the highlighted file
            if ( keyAction.tag() == 'l')
            {
                programMode = ScreenEditor::EDIT;
                CxString pathName = fileListView->getSelectedItem();
                loadNewFile( pathName, TRUE );
                editView->updateScreen();
            }
            
            

        }
        break;
            
        //-----------------------------------------------------------------------------------------
        // handle all other keys
        //-----------------------------------------------------------------------------------------
        default:
        {
            fileListView->routeKeyAction( keyAction );
        }
        break;
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::focusFilelist
//
// When the filelist is the focus this handles the routing of keys.  Most keys are passed through
// to the file list view but a few are intercepted.  newline selects a new file to edit and esc
// dismisses the window without loading a new buffer (like a cancel button)
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::focusHelpView( CxKeyAction keyAction )
{
    switch (keyAction.actionType() )
    {
        //-----------------------------------------------------------------------------------------
        // handle escape key
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::COMMAND:
        {
            programMode = ScreenEditor::EDIT;
            editView->updateScreen();
        }
        break;
                            
        //-----------------------------------------------------------------------------------------
        // handle a newline
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::NEWLINE:
        {
            programMode = ScreenEditor::EDIT;
//            CxString pathName = fileListView->getSelectedItem();
  //          loadNewFile( pathName, TRUE );
            editView->updateScreen();
        }
        break;
            
            
        //-----------------------------------------------------------------------------------------
        // normal input characters
        //
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::LOWERCASE_ALPHA:
        case CxKeyAction::UPPERCASE_ALPHA:
        {
            
            // load the highlighted file
            if ( keyAction.tag() == 'l')
            {
                programMode = ScreenEditor::EDIT;
      //          CxString pathName = fileListView->getSelectedItem();
    //            loadNewFile( pathName, TRUE );
                editView->updateScreen();
            }
            
            

        }
        break;
            
        //-----------------------------------------------------------------------------------------
        // handle all other keys
        //-----------------------------------------------------------------------------------------
        default:
        {
            helpTextView->routeKeyAction( keyAction );
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
    programMode = ScreenEditor::EDIT;

    commandLineView->setText("");
    commandLineView->setPrompt("");
    commandLineView->updateScreen();
    editView->placeCursor();
    screen->flush();
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

    renderCommandLine( prefix, _argBuffer, prefix.length() + _argBuffer.length() );
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
        programMode = ScreenEditor::EDIT;
        return;
    }

    // check if handler exists
    if (_currentCommand->handler == NULL) {
        setMessage( "(command not implemented)" );
        resetCommandInputState();
        programMode = ScreenEditor::EDIT;
        editView->placeCursor();
        screen->flush();
        return;
    }

    // call the handler
    CommandHandler handler = _currentCommand->handler;
    (this->*handler)( _argBuffer );

    // clear internal command input state
    resetCommandInputState();

    // only return to edit mode if handler didn't switch to a different mode
    // (e.g., CMD_Help switches to HELPVIEW, CMD_BufferList switches to FILELIST)
    if (programMode == ScreenEditor::COMMANDLINE) {
        programMode = ScreenEditor::EDIT;
        editView->placeCursor();
        screen->flush();
    }
}

