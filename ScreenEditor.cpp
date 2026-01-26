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


//-------------------------------------------------------------------------------------------------
// Dispatch tables for control commands
//
// Each entry maps a control key tag to a handler method and optional message.
// The table is terminated by a NULL tag entry.
//-------------------------------------------------------------------------------------------------
ScreenEditor::ControlCmd ScreenEditor::_controlCommands[] = {
    { "J",    &ScreenEditor::CONTROL_ToggleJumpScroll,  NULL },
    { "F",    &ScreenEditor::CONTROL_FindAgain,         NULL },
    { "R",    &ScreenEditor::CONTROL_ReplaceAgain,      NULL },
    { "L",    &ScreenEditor::CONTROL_ToggleLineNumbers, NULL },
    { "W",    &ScreenEditor::CTRL_Cut,                  "(text cut)" },
    { "V",    &ScreenEditor::CTRL_PageDown,             "(paged down)" },
    { "Z",    &ScreenEditor::CTRL_PageUp,               "(paged up)" },
    { "K",    &ScreenEditor::CTRL_CutToEndOfLine,       "(text cut to end of line)" },
    { "Y",    &ScreenEditor::CTRL_Paste,                "(text pasted)" },
    { "N",    &ScreenEditor::CTRL_NextBuffer,           "(next buffer)" },
    { "P",    &ScreenEditor::CTRL_ProjectList,          "(Project List)" },
    { "U",    &ScreenEditor::CTRL_UpdateScreen,         "(Update Screen)" },
    { "<US>", &ScreenEditor::CTRL_Help,                 "(Help)" },
    { NULL,   NULL,                                      NULL }
};

ScreenEditor::ControlCmd ScreenEditor::_ctrlXCommands[] = {
    { "S",    &ScreenEditor::CTRLX_Save,  NULL },
    { "C",    &ScreenEditor::CTRLX_Quit,  NULL },
    { NULL,   NULL,                        NULL }
};


//-------------------------------------------------------------------------------------------------
// ScreenEditor::ScreenEditor (constructor)
//
// Creates all the important parts of the program, the editview, the commandline, they keyboard
// input object.
//
//-------------------------------------------------------------------------------------------------
ScreenEditor::ScreenEditor( CxScreen *scr, CxKeyboard *key, CxString filePath )
{
    // DEBUG: open log file for startup diagnostics
    FILE *dbg = fopen("/tmp/cm_debug.log", "w");
    if (dbg) { fprintf(dbg, "1: constructor start\n"); fflush(dbg); }

    // Block SIGWINCH during construction to prevent callbacks on partially-constructed objects
    sigset_t blockSet, oldSet;
    sigemptyset(&blockSet);
    sigaddset(&blockSet, SIGWINCH);
    sigprocmask(SIG_BLOCK, &blockSet, &oldSet);

    if (dbg) { fprintf(dbg, "2: signals blocked\n"); fflush(dbg); }

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
    _commandRegistry = NULL;
    _currentCommand = NULL;
    _quitRequested = FALSE;

    programMode = ScreenEditor::EDIT;

    if (dbg) { fprintf(dbg, "3: pointers initialized\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // load the default setting file - check current directory first, then home directory
    //
    //---------------------------------------------------------------------------------------------
    programDefaults = new ProgramDefaults();
    if (dbg) { fprintf(dbg, "4: ProgramDefaults created\n"); fflush(dbg); }

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
    // init the command registry and command input state
    //
    //---------------------------------------------------------------------------------------------
    _commandRegistry = new CommandRegistry();
    _cmdInputState = CMD_INPUT_IDLE;
    _cmdBuffer = "";
    _argBuffer = "";
    _currentCommand = NULL;

    if (dbg) { fprintf(dbg, "5: CommandRegistry created\n"); fflush(dbg); }

    editBufferList = new CxEditBufferList();
    if (dbg) { fprintf(dbg, "6: editBufferList created\n"); fflush(dbg); }
    
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
    
    if (dbg) { fprintf(dbg, "7: about to create CommandLineView\n"); fflush(dbg); }

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

    if (dbg) { fprintf(dbg, "8: CommandLineView created\n"); fflush(dbg); }

    commandLineView->setPrompt("");



    screen->clearScreen();

    if (dbg) { fprintf(dbg, "9: screen cleared\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // make an edit window where the editing happens
    //
    //---------------------------------------------------------------------------------------------
    editView = new EditView( programDefaults, screen );

    if (dbg) { fprintf(dbg, "10: EditView created\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // create a project object (regardless if there is a project)
    //
    //---------------------------------------------------------------------------------------------
    project = new Project();
    if (dbg) { fprintf(dbg, "11: Project created\n"); fflush(dbg); }

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

        if (dbg) { fprintf(dbg, "12: about to loadNewFile\n"); fflush(dbg); }

        //-----------------------------------------------------------------------------------------
        // not a project file so just load the referenced file
        //
        //-----------------------------------------------------------------------------------------
        loadNewFile( filePath, TRUE );

        if (dbg) { fprintf(dbg, "13: loadNewFile complete\n"); fflush(dbg); }
    }

    if (dbg) { fprintf(dbg, "14: about to create FileListView\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // create a file list view for the project
    //
    //---------------------------------------------------------------------------------------------

    fileListView = new FileListView(programDefaults,
                                    editBufferList,
                                    project,
                                    screen
                                    );

    if (dbg) { fprintf(dbg, "15: FileListView created\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // create a help view for editor help
    //
    //---------------------------------------------------------------------------------------------

  	helpTextView = new HelpTextView(programDefaults,
									project,
									screen);

    if (dbg) { fprintf(dbg, "16: HelpTextView created\n"); fflush(dbg); }

    helpTextView->loadHelpText("./cm.txt");

    if (dbg) { fprintf(dbg, "17: help text loaded\n"); fflush(dbg); }

    //---------------------------------------------------------------------------------------------
    // update the screen and place the cursor
    //
    //---------------------------------------------------------------------------------------------
    if (dbg) { fprintf(dbg, "17a: editView ptr = %p\n", (void*)editView); fflush(dbg); }
    if (dbg) { fprintf(dbg, "17b: about to call editView->updateScreen()\n"); fflush(dbg); }

	editView->updateScreen();

    if (dbg) { fprintf(dbg, "18: editView->updateScreen complete\n"); fflush(dbg); }

    editView->placeCursor();

    if (dbg) { fprintf(dbg, "19: editView->placeCursor complete\n"); fflush(dbg); }

    // Unblock SIGWINCH now that construction is complete
    sigprocmask(SIG_SETMASK, &oldSet, NULL);

    if (dbg) { fprintf(dbg, "20: constructor complete, signals unblocked\n"); fclose(dbg); }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::~ScreenEditor (destructor)
//
// cleans up all the dynamic objects
//
//-------------------------------------------------------------------------------------------------
ScreenEditor::~ScreenEditor(void)
{
    delete programDefaults;
    delete commandLineView;
    delete editView;
    // Note: screen and keyboard are owned by main(), not deleted here
}



//-------------------------------------------------------------------------------------------------
// ScreenEditor::loadHelpText()
//
//
//-------------------------------------------------------------------------------------------------
CxSList< CxString >
ScreenEditor::loadHelpText(void)
{
	CxSList< CxString > textList;
 	CxFile inFile;
    CxString data;

    CxString filePath = "cm.txt";

    // open the file
    if (!inFile.open( filePath, "r")) {

        // return false indicating a file was not found
        return( textList );
    }

	// read each line of text        
	do {
	
		data = inFile.getUntil('\n');
		textList.append(data);

	} while (!inFile.eof());

    // close the file
    inFile.close();

	return( textList );
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::loadNewFile
//
// load a new file into a new buffer and make it the viewable buffer
//
//-------------------------------------------------------------------------------------------------
int
ScreenEditor::loadNewFile( CxString filePath, int preload )
{
    //printf("loading %s\n", filePath.data());
 
    filePath = filePath.stripLeading(" \t\r\n");
    filePath = filePath.stripTrailing(" \t\r\n");
    
    // if the file path is actually some text
    if (filePath.length()) {
        
        // if the file is readable path is readable, readwriteable, new but writeable
        if ( checkFile( filePath ) == 0) {

            // return the editbuffer already in the list
            CxEditBuffer *editBuffer = editBufferList->findPath( filePath );
            
            // we did find an edit buffer already loaded so just make it the current one
            if (editBuffer != NULL) {
                
                // if the user wants autosave  on buffer changes
                if (programDefaults->autoSaveOnBufferChange()) {
                    saveCurrentEditBufferOnSwitch();
                }
                
                // check if this buffer has been loaded into memory
                if (!editBuffer->isInMemory()) {

                    // show loading status and flush immediately
                    setMessage(CxString("(Loading ") + filePath + "...)");
                    commandLineView->updateScreen();
                    screen->flush();

                    // load the text
                    editBuffer->loadText( filePath, TRUE );

                    // show completion
                    setMessage(CxString("(Loaded ") + filePath + ")");
                }
                
                // set the edit buffer in the edit view
                editView->setEditBuffer( editBuffer );
                                
            // we did not find an edit buffer in the list, so create a new one
            // referencing that file path
            } else {
                
                // if the user wants autosave on buffer changes
                if (programDefaults->autoSaveOnBufferChange()) {
                    saveCurrentEditBufferOnSwitch();
                }
                
                // create a new edit buffer
                editBuffer = new CxEditBuffer( );

                // show loading status and flush immediately
                setMessage(CxString("(Loading ") + filePath + "...)");
                commandLineView->updateScreen();
                screen->flush();

                // load the text into it
                editBuffer->loadText( filePath, preload );

                // show completion
                setMessage(CxString("(Loaded ") + filePath + ")");

                // add it to the list of edit buffers
                editBufferList->add( editBuffer );
                
                // set teh editbuffer in the edit view
                editView->setEditBuffer( editBuffer );
                
            }
        }
    }

    commandLineView->updateScreen();
    
    return(0);
}

//-------------------------------------------------------------------------------------------------
// ScreenEditor::saveCurrentEditBufferOnSwitch
//
// Save the current buffer if the user pref says to and the file is changed
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::saveCurrentEditBufferOnSwitch(void)
{
   // get the current buffer
    CxEditBuffer *currentEditBuffer = editBufferList->current();
    if (currentEditBuffer != NULL) {
        
        //  get the current buffer file name
        CxString currentFilePath = currentEditBuffer->getFilePath();
        
        // if the buffer is loaded into memory
        if (currentEditBuffer->isInMemory()) {
            
            // and if the buffer has been touched
            if (currentEditBuffer->isTouched()) {
                
                // save the file
                currentEditBuffer->saveText( currentFilePath );
            }
        }
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::nextBuffer
//
// Change to the next buffer, saving the old one if required
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::nextBuffer(void)
{
    // if the user wants autosave  on buffer changes
    if (programDefaults->autoSaveOnBufferChange()) {
        saveCurrentEditBufferOnSwitch();
    }
    
    // create a new edit buffer
    CxEditBuffer *editBuffer = editBufferList->next();
    
    // see if the buffer is in memory
    if (!editBuffer->isInMemory()) {
        
        // if not then load the text
        editBuffer->loadText( editBuffer->getFilePath() , TRUE );
    }

    // hand it to the edit view to display
    editView->setEditBuffer( editBuffer );
    
    // redraw the edit view
    editView->reframeAndUpdateScreen();
    
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::previousBuffer
//
// Change to the preveous buffer, saving the old one if required
//
//-------------------------------------------------------------------------------------------------
void
ScreenEditor::previousBuffer(void)
{
    // if the user wants autosave  on buffer changes
    if (programDefaults->autoSaveOnBufferChange()) {
        saveCurrentEditBufferOnSwitch();
    }
    
    // create a new edit buffer
    CxEditBuffer *editBuffer = editBufferList->previous();
    
    // see if the buffer is in memory
    if (!editBuffer->isInMemory()) {
        
        // if not then load the text
        editBuffer->loadText( editBuffer->getFilePath() , TRUE );
    }
    
    // hand it to the edit view to display
    editView->setEditBuffer( editBuffer );
    
    // redraw the edit view
    editView->reframeAndUpdateScreen();
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::::checkfile
//
// This method checks if the path is actually a file (or potential file) and checks for
// the ability to write the file.  Not all cases are fatal but the user to notified of any
// issues ahead of time so if a mistake was made they an correct.
//
//-------------------------------------------------------------------------------------------------
int
ScreenEditor::checkFile( CxString filePath )
{
    char buffer[200];
    CxFileAccess::status stat = CxFileAccess::checkStatus( filePath );

    switch( stat ) {

        case CxFileAccess::NOT_A_REGULAR_FILE:
        {
            sprintf(buffer, "file: %s could not be loaded and is NOT A REGULAR file, SAVING WON'T work to the same path", filePath.data());
            setMessage( buffer );
            return(-1);
        }
        break;

        case CxFileAccess::FOUND_W:
        {
            sprintf(buffer, "file: %s could not be loaded, directory permissions WON'T ALLOW SAVING to the same path", filePath.data());
            setMessage( buffer );
            return(-1);
        }
        break;

        case CxFileAccess::FOUND_R:           // file was found and is readable only
        {
            sprintf(buffer, "file: %s loaded, however the file is READ ONLY and WON'T ALLOW SAVING to the same path", filePath.data());
            setMessage( buffer );
            return(0);
        }
        break;

        case CxFileAccess::FOUND_RW:           // file was found and is read/writable
        {
            sprintf(buffer, "file: %s loaded", filePath.data());
            setMessage( buffer );
            return(0);
        }
        break;

        case CxFileAccess::NOT_FOUND_W:        // file was not found but one can be written
        {
            sprintf(buffer, "file: %s not found, a new file will be created", filePath.data());
            setMessage( buffer );
            return(0);
        }
        break;

        case CxFileAccess::NOT_FOUND:
        {
            sprintf(buffer, "file %s was not found, and directory permissions WON'T ALLOW SAVING at that path", filePath.data() );
            setMessage( buffer );
            return(-1);
        }
        break;
    }

    return(-1);
}

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
    char buffer[200];
    
    if (editView->replaceAgain( _findString, _replaceString )  == TRUE ) {
        
        CxEditBufferPosition loc = editView->cursorPosition();

        sprintf(buffer, "loc=(%lu,%lu)", loc.row, loc.col);
        CxString locBuffer = CxString("(replace again found) ") + CxString(buffer);
        
        setMessage(locBuffer);
        
    } else {
        
        CxEditBufferPosition loc = editView->cursorPosition();

        sprintf(buffer, "loc=(%lu,%lu)", loc.row, loc.col);
        CxString locBuffer = CxString("(replace again not found) ") + CxString(buffer);
        
        setMessage(locBuffer);
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
    //---------------------------------------------------------------------------------------------
    // if in new command input mode, use the new handler
    //---------------------------------------------------------------------------------------------
    if (_cmdInputState == CMD_INPUT_COMMAND || _cmdInputState == CMD_INPUT_ARGUMENT) {
        handleCommandInput( keyAction );
        // cursor is positioned by updateCommandDisplay() or transitionToArgument()
        return;
    }

    //---------------------------------------------------------------------------------------------
    // legacy command line handling (for backward compatibility during transition)
    //---------------------------------------------------------------------------------------------
    switch (keyAction.actionType() )
    {
        //-----------------------------------------------------------------------------------------
        // handle the command (esc) and newline key
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::COMMAND:
        case CxKeyAction::NEWLINE:
        {
            // handle escape like its a return
            CxString commandLine = commandLineView->getText();
            commandLineView->setText("");

            programMode = ScreenEditor::EDIT;
            editView->placeCursor();

            int result = executeCommand( commandLine );
            if (result) {
                return;
            }

            commandLineView->setText("");
            editView->placeCursor();
            screen->flush();

        }
        break;

        //-----------------------------------------------------------------------------------------
        // handle all other keys
        //-----------------------------------------------------------------------------------------
        default:
        {
            commandLineView->routeKeyAction( keyAction );
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
// ScreenEditor::gatherHint
//
// fill in the command line with hint text for a command.  If the command is immediate then
// a 1 is returned and the command will be executed without typing any additional text or
// gathering any additional input from the user.  If a 0 is returned the test is simply typed
// into the command line and the user needs to finish the command
//
//-------------------------------------------------------------------------------------------------
int
ScreenEditor::gatherHint( void )
{
    CxKeyAction keyAction = keyboard->getAction();

    // based on the catagory of the key
    switch (keyAction.actionType() )
    {
        //-----------------------------------------------------------------------------------------
        // if a command action, ignore
        //
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::COMMAND:
        {
            return(0);
        }
        break;

        //-----------------------------------------------------------------------------------------
        // if a normal key, then see if it matches the first character of a well known command if it
        // is then type the command into the commandline.  The user can always backspace.
        //
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::LOWERCASE_ALPHA:
        case CxKeyAction::UPPERCASE_ALPHA:
        case CxKeyAction::NUMBER:
        case CxKeyAction::SYMBOL:
        {
             //------------------------------------------------------------------------------------
             // <esc> s - save command hint
             //------------------------------------------------------------------------------------
             if (keyAction.tag() == "s") {
                 commandLineView->typeText("save: ");
                 
                 CxEditBuffer *editBuffer = editBufferList->current();
                 
                 CxString filePath = editBuffer->getFilePath();
                 
                 if (filePath.length() ) {
                     commandLineView->typeText(filePath);
                 }
                 return(0);
             }
            
            //-------------------------------------------------------------------------------------
            // <esc> l - load a new buffer
            //-------------------------------------------------------------------------------------
            if (keyAction.tag() == "l") {
                commandLineView->typeText("load-new-buffer: ");
                return(0);
            }

             //------------------------------------------------------------------------------------
             // <esc> f - find command hint
             //------------------------------------------------------------------------------------
             if (keyAction.tag() == "f") {
                 commandLineView->typeText("find: ");
                 return(0);
             }

             //------------------------------------------------------------------------------------
             // (esc> r - replace command hint
             //------------------------------------------------------------------------------------
             if (keyAction.tag() == "r") {
                 commandLineView->typeText("replace: ");
                 return(0);
             }
             
             //------------------------------------------------------------------------------------
             // <esc> q - quit command hint
             //------------------------------------------------------------------------------------
             if (keyAction.tag() == "q") {
                 commandLineView->typeText("quit:");
                 return(0);
             }
             
             //------------------------------------------------------------------------------------
             // <esc> g - goto line command hint
             //------------------------------------------------------------------------------------
             if (keyAction.tag() == "g") {
                 commandLineView->typeText("goto-line: ");
                 return(0);
             }

             //------------------------------------------------------------------------------------
             // <esc> <space> or esc m set mark command hint
             //------------------------------------------------------------------------------------
             if ((keyAction.tag() == " ") || (keyAction.tag() == "m")) {
                 commandLineView->typeText("set-mark:");
                 editView->setMark();
                 return(1);
             }

             //------------------------------------------------------------------------------------
             // <esc> c - cut to mark command hint
             //------------------------------------------------------------------------------------
             if (keyAction.tag() == "c") {
                 commandLineView->typeText("cut-to-mark:");
                 return(0);
             }

             //------------------------------------------------------------------------------------
             // <esc> p - paste command hint
             //------------------------------------------------------------------------------------
             if (keyAction.tag() == "p") {
                 commandLineView->typeText("paste:");
                 return(0);
             }
            
             //------------------------------------------------------------------------------------
             // if no leading character matches a hint, then just type the character on the
             // command line so the user can enter a less used command that starts with the
             // the character
             //------------------------------------------------------------------------------------
             commandLineView->typeText(keyAction.tag());
         }
    }

    return(0);
}


//-------------------------------------------------------------------------------------------------
//
// ESC COMMAND INPUT SYSTEM
//
// Overview:
//   When the user presses ESC, the editor enters a command input mode that provides
//   fuzzy command completion similar to modern IDE command palettes. The user types
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
// Command Registry:
//   Commands are defined in CommandRegistry.cpp. Each entry has:
//   - name: the command name (e.g., "find", "goto-line")
//   - argHint: hint text for argument (e.g., "<pattern>", "<line>")
//   - description: help text
//   - flags: CMD_FLAG_NEEDS_ARG, CMD_FLAG_OPTIONAL_ARG
//   - handler: function pointer to ScreenEditor method
//
//   The registry provides fuzzy prefix matching that ignores hyphens, so typing
//   "gl" matches "goto-line" and "fa" matches "find-all".
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
    _cmdInputState = CMD_INPUT_IDLE;
    _cmdBuffer = "";
    _argBuffer = "";
    _currentCommand = NULL;
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
    int takesArgument = (cmd->flags & (CMD_FLAG_NEEDS_ARG | CMD_FLAG_OPTIONAL_ARG));

    if (takesArgument) {
        // go to argument input mode
        _currentCommand = cmd;
        _cmdInputState = CMD_INPUT_ARGUMENT;
        _argBuffer = "";
        updateArgumentDisplay();
    }
    else {
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
ScreenEditor::updateCommandDisplay( void )
{
    CommandEntry *matches[16];
    int count = _commandRegistry->findMatches( _cmdBuffer, matches, 16 );

    // build display: typed text + completions
    CxString display = _cmdBuffer;

    // don't show completion hint if typed text exactly matches a command
    int isExactMatch = (count == 1 && _cmdBuffer == matches[0]->name);

    if (count > 0 && !isExactMatch) {
        display += "  ";
        for (int i = 0; i < count && i < 8; i++) {
            display += "| ";
            display += matches[i]->name;
            if (matches[i]->argHint != NULL) {
                display += " ";
                display += matches[i]->argHint;
            }
            display += " ";
        }
        if (count > 8) {
            display += "...";
        }
    }

    CxString fullLine = "command> ";
    fullLine += display;

    // pad to screen width
    unsigned long targetWidth = screen->cols();
    if (targetWidth > 2) targetWidth -= 2;
    while (fullLine.length() < targetWidth) {
        fullLine += " ";
    }

    screen->resetColors();
    screen->writeTextAt( screen->rows() - 1, 1, fullLine, true );

    // cursor after typed text
    unsigned long cursorCol = 1 + 9 + _cmdBuffer.length();
    screen->placeCursor( screen->rows() - 1, cursorCol );
    screen->flush();
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

    // RETURN - execute if we have a match
    if (keyAction.actionType() == CxKeyAction::NEWLINE) {
        // prefer exact match (e.g., "find" when "find" and "find-all" both exist)
        CommandEntry *exact = _commandRegistry->findExact( _cmdBuffer );
        if (exact != NULL) {
            _currentCommand = exact;
            if (exact->flags & (CMD_FLAG_NEEDS_ARG | CMD_FLAG_OPTIONAL_ARG)) {
                selectCommand( exact );
            } else {
                executeCurrentCommand();
            }
            return;
        }

        // otherwise try unique prefix match
        CommandEntry *matches[16];
        int count = _commandRegistry->findMatches( _cmdBuffer, matches, 16 );

        if (count == 1) {
            _currentCommand = matches[0];
            if (matches[0]->flags & (CMD_FLAG_NEEDS_ARG | CMD_FLAG_OPTIONAL_ARG)) {
                selectCommand( matches[0] );
            } else {
                executeCurrentCommand();
            }
        }
        else if (count > 1) {
            setMessage( "(ambiguous command)" );
            cancelCommandInput();
        }
        else {
            setMessage( "(unknown command)" );
            cancelCommandInput();
        }
        return;
    }

    // TAB - complete prefix
    if (keyAction.actionType() == CxKeyAction::TAB) {
        CommandEntry *matches[16];
        int count = _commandRegistry->findMatches( _cmdBuffer, matches, 16 );

        if (count == 1) {
            _cmdBuffer = matches[0]->name;
            selectCommand( matches[0] );
        }
        else if (count > 1) {
            CxString common = _commandRegistry->completePrefix( _cmdBuffer );
            if (common.length() > _cmdBuffer.length()) {
                _cmdBuffer = common;
            }
            updateCommandDisplay();
        }
        else {
            setMessage( "(no match)" );
        }
        return;
    }

    // SPACE - transition to argument if exact match
    if (keyAction.tag() == ' ') {
        CommandEntry *exact = _commandRegistry->findExact( _cmdBuffer );
        if (exact != NULL) {
            selectCommand( exact );
        }
        return;
    }

    // Printable character - add to buffer
    if (keyAction.actionType() == CxKeyAction::LOWERCASE_ALPHA ||
        keyAction.actionType() == CxKeyAction::UPPERCASE_ALPHA ||
        keyAction.actionType() == CxKeyAction::NUMBER ||
        keyAction.actionType() == CxKeyAction::SYMBOL) {

        _cmdBuffer += keyAction.tag();

        CommandEntry *matches[16];
        int count = _commandRegistry->findMatches( _cmdBuffer, matches, 16 );

        if (count == 1) {
            selectCommand( matches[0] );
        }
        else {
            updateCommandDisplay();
        }
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

    // Printable character - add to argument
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
    // build prompt: "command <hint>: argument"
    CxString fullLine = _currentCommand->name;
    if (_currentCommand->argHint != NULL) {
        fullLine += " ";
        fullLine += _currentCommand->argHint;
    }
    fullLine += ": ";

    unsigned long promptLen = fullLine.length();
    fullLine += _argBuffer;

    // pad to screen width to clear old content
    unsigned long targetWidth = screen->cols();
    if (targetWidth > 2) targetWidth -= 2;
    while (fullLine.length() < targetWidth) {
        fullLine += " ";
    }

    // use default colors (not magenta)
    screen->resetColors();
    screen->writeTextAt( screen->rows() - 1, 1, fullLine, true );

    // position cursor after the argument text
    unsigned long cursorCol = 1 + promptLen + _argBuffer.length();
    screen->placeCursor( screen->rows() - 1, cursorCol );
    screen->flush();
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
        _cmdInputState = CMD_INPUT_IDLE;
        programMode = ScreenEditor::EDIT;
        return;
    }

    // check if handler exists
    if (_currentCommand->handler == NULL) {
        setMessage( "(command not implemented)" );
        _cmdInputState = CMD_INPUT_IDLE;
        _currentCommand = NULL;
        _cmdBuffer = "";
        _argBuffer = "";
        programMode = ScreenEditor::EDIT;
        editView->placeCursor();
        screen->flush();
        return;
    }

    // call the handler
    CommandHandler handler = _currentCommand->handler;
    (this->*handler)( _argBuffer );

    // clear internal command input state
    _cmdInputState = CMD_INPUT_IDLE;
    _currentCommand = NULL;
    _cmdBuffer = "";
    _argBuffer = "";

    // only return to edit mode if handler didn't switch to a different mode
    // (e.g., CMD_Help switches to HELPVIEW, CMD_BufferList switches to FILELIST)
    if (programMode == ScreenEditor::COMMANDLINE) {
        programMode = ScreenEditor::EDIT;
        editView->placeCursor();
        screen->flush();
    }
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::executeCommand
//
// this method executes whatever command is on the commandline
//
//-------------------------------------------------------------------------------------------------
int
ScreenEditor::executeCommand( CxString commandLine )
{
    // strip out the command
    CxString command = commandLine.nextToken(" \t\n");

    // save a file
    if (command == "save:") {
        CMD_SaveFile( commandLine );
        return(0);
    }
    
    if (command == "load-new-buffer:") {
        CMD_LoadFile( commandLine );
    }

    if (command == "quit:") {
        return(1);
    }

    if (command == "goto-line:") {
        CMD_GotoLine( commandLine );
        return(0);
    }

    if (command == "find:") {
        CMD_Find( commandLine );
        return(0);
    }

    if (command == "set-mark:") {
        CMD_SetMark( commandLine );
        return(0);
    }

    if (command == "cut-to-mark:") {
        CMD_CutToMark(commandLine );
        return(0);
    }

    if (command == "paste:") {
        CMD_PasteText( commandLine );
        return(0);
    }

    if (command == "replace:") {
        CMD_Replace( commandLine  );
        return(0);
    }

    if (command == "cb:") {
        CMD_CommentBlock( commandLine );
        return(0);
    }
    
    if (command =="new:") {
        CMD_NewBuffer( commandLine );
        return(0);
    }
    

    return(0);
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
    
    //CxEditBuffer *editBuffer = editBufferList->current();
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

    CxEditBuffer *editBuffer = editBufferList->current();
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
    fileListView->recalcScreenPlacements();
    fileListView->redraw();
    programMode = ScreenEditor::FILELIST;
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
    helpTextView->recalcScreenPlacements();
    helpTextView->redraw();
    programMode = HELPVIEW;
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
    CxEditBuffer *editBuffer = editView->getEditBuffer();

    unsigned long lineCount = editBuffer->numberOfLines();
    unsigned long charCount = editBuffer->characterCount();

    char buffer[200];
    sprintf(buffer, "(%lu lines, %lu characters)", lineCount, charCount);
    setMessage(buffer);
}


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

    char buffer[200];

    if(    editView->findString( _findString ) == TRUE ) {
        
        CxEditBufferPosition loc = editView->cursorPosition();
        
        sprintf(buffer, "loc=(%lu,%lu)", loc.row, loc.col);
        CxString locBuffer = CxString("(found) ") + CxString(buffer);
        
        setMessage(locBuffer);
        
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

    char buffer[200];

    if( editView->replaceString( _findString, _replaceString ) == TRUE ) {
        
        CxEditBufferPosition loc = editView->cursorPosition();

        sprintf(buffer, "loc=(%lu,%lu)", loc.row, loc.col);
        CxString locBuffer = CxString("(replace found) ") + CxString(buffer);
        
        setMessage(locBuffer);
        
    } else {
        
        CxEditBufferPosition loc = editView->cursorPosition();

        sprintf(buffer, "loc=(%lu,%lu)", loc.row, loc.col);
        CxString locBuffer = CxString("(replace not found) ") + CxString(buffer);
        
        setMessage(locBuffer);
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
    CxEditBuffer *editBuffer = editView->getEditBuffer();

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
}

void ScreenEditor::CTRL_Paste(void)
{
    editView->pasteText(_cutBuffer);
}

void ScreenEditor::CTRL_CutToEndOfLine(void)
{
    _cutBuffer = editView->cutTextToEndOfLine();
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
    fileListView->recalcScreenPlacements();
    fileListView->redraw();
    programMode = FILELIST;
}

void ScreenEditor::CTRL_UpdateScreen(void)
{
    editView->updateScreen();
}

void ScreenEditor::CTRL_Help(void)
{
    helpTextView->recalcScreenPlacements();
    helpTextView->redraw();
    programMode = HELPVIEW;
}

void ScreenEditor::CTRLX_Save(void)
{
    CxEditBuffer *editBuffer = editBufferList->current();
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


//-------------------------------------------------------------------------------------------------
// ScreenEditor::dispatchControlX
//
// Handle Control-X prefix commands (two-key sequences)
//-------------------------------------------------------------------------------------------------
int
ScreenEditor::dispatchControlX(void)
{
    CxKeyAction secondAction = keyboard->getAction();

    // Control-X, Enter - toggle jump scroll
    if (secondAction.actionType() == CxKeyAction::NEWLINE) {
        CONTROL_ToggleJumpScroll();
        editView->placeCursor();
        screen->flush();
        return 0;
    }

    // Must be a control key for other Control-X commands
    if (secondAction.actionType() != CxKeyAction::CONTROL) {
        return 0;
    }

    // Look up in Control-X dispatch table
    for (int i = 0; _ctrlXCommands[i].tag != NULL; i++) {
        if (secondAction.tag() == _ctrlXCommands[i].tag) {
            if (_ctrlXCommands[i].message != NULL) {
                setMessage(_ctrlXCommands[i].message);
            }
            (this->*_ctrlXCommands[i].handler)();
            editView->placeCursor();
            screen->flush();

            // Special case: Control-X Control-C (quit) returns 1
            if (secondAction.tag() == "C") {
                return 1;
            }
            return 0;
        }
    }

    return 0;
}


//-------------------------------------------------------------------------------------------------
// ScreenEditor::handleControl
//
// Dispatch control key commands using lookup tables.
// Control-X prefix commands are handled by dispatchControlX().
//
//-------------------------------------------------------------------------------------------------
int
ScreenEditor::handleControl( CxKeyAction keyAction )
{
    // Control-X prefix - two-key command sequence
    if (keyAction.tag() == "X") {
        return dispatchControlX();
    }

    // Control-H (backspace) - special case, routes directly to editView
    if (keyAction.tag() == "H") {
        editView->routeKeyAction(keyAction);
        return 0;
    }

    // Look up in dispatch table
    for (int i = 0; _controlCommands[i].tag != NULL; i++) {
        if (keyAction.tag() == _controlCommands[i].tag) {
            if (_controlCommands[i].message != NULL) {
                setMessage(_controlCommands[i].message);
            }
            (this->*_controlCommands[i].handler)();
            editView->placeCursor();
            screen->flush();
            return 0;
        }
    }

    return 0;
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
    char buffer[200];
    
    if (editView->findAgain( _findString ) == TRUE ) {
        
        CxEditBufferPosition loc = editView->cursorPosition();

        sprintf(buffer, "loc=(%lu,%lu)", loc.row, loc.col);
        CxString locBuffer = CxString("(found) ") + CxString(buffer);
        
        setMessage(locBuffer);
        
    } else {
        
        CxEditBufferPosition loc = editView->cursorPosition();

        sprintf(buffer, "loc=(%lu,%lu)", loc.row, loc.col);
        CxString locBuffer = CxString("(not found) ") + CxString(buffer);
        
        setMessage(locBuffer);
    }
    
    editView->placeCursor();
    screen->flush();
}

