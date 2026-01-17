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
    editBufferList = NULL;
    
    programMode = ScreenEditor::EDIT;
    
    //---------------------------------------------------------------------------------------------
    // load the default setting file
    //
    //---------------------------------------------------------------------------------------------
    programDefaults = new ProgramDefaults();
    
    CxString homeDir = getenv("HOME");
    if (homeDir.length()) {
        homeDir += "/.cmrc";
        programDefaults->loadDefaults(homeDir);
    }
    
    //---------------------------------------------------------------------------------------------
    // init the cut buffer and replace buffer
    //
    //---------------------------------------------------------------------------------------------
    _cutBuffer = "";
    _findString = "";
    _replaceString = "";
    
    editBufferList = new CxEditBufferList();
    
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
    
    commandLineView->setPrompt("");
    
   
    
    screen->clearScreen();
    
    
    //---------------------------------------------------------------------------------------------
    // make an edit window where the editing happens
    //
    //---------------------------------------------------------------------------------------------
    editView = new EditView( programDefaults, screen );
    
    //---------------------------------------------------------------------------------------------
    // create a project object (regardless if there is a project)
    //
    //---------------------------------------------------------------------------------------------
    project = new Project();

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
        
        //-----------------------------------------------------------------------------------------
        // not a project file so just load the referenced file
        //
        //-----------------------------------------------------------------------------------------
        loadNewFile( filePath, TRUE );
    }
    
    //---------------------------------------------------------------------------------------------
    // create a file list view for the project
    //
    //---------------------------------------------------------------------------------------------
    
    fileListView = new FileListView(programDefaults,
                                    editBufferList,
                                    project,
                                    screen
                                    );


    //---------------------------------------------------------------------------------------------
    // create a help view for editor help
    //
    //---------------------------------------------------------------------------------------------

  	helpTextView = new HelpTextView(programDefaults,
									project,
									screen);
    
    helpTextView->loadHelpText("./cm.txt");

    
    //---------------------------------------------------------------------------------------------
    // update the screen and place the cursor
    //
    //---------------------------------------------------------------------------------------------
	editView->updateScreen();
    editView->placeCursor();
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
    delete screen;
    delete keyboard;
    delete commandLineView;
    delete editView;
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
        // handle escape key
        //-----------------------------------------------------------------------------------------
        case CxKeyAction::COMMAND:
        {
            resetPrompt();
            programMode = ScreenEditor::COMMANDLINE;

            //-------------------------------------------------------------------------------------
            // focus is now on the command line
            //-------------------------------------------------------------------------------------
            commandLineView->updateScreen();
            commandLineView->placeCursor();
            
            //-------------------------------------------------------------------------
            // place a hint command on the command line, some commands are immediate
            // meaning we should go ahead and execute immediately without a
            // confirmation keystroke, if gatherhint returns 1 then the command
            // should be executed and mode put back into edit mode.
            //-------------------------------------------------------------------------
            if (gatherHint() == 1) {
                
                //---------------------------------------------------------------------
                // immediate command, so the get the command line and process it
                //---------------------------------------------------------------------
                CxString commandLine = commandLineView->getText();
                
                //---------------------------------------------------------------------
                // execute the command
                //---------------------------------------------------------------------
                if (executeCommand( commandLine )) {
                    
                    //-----------------------------------------------------------------
                    // command was quit, so we are all done
                    //-----------------------------------------------------------------
                    editView->placeCursor();
                    return(1);
                }
                
                //---------------------------------------------------------------------
                // clear the commandline and place the cursor in the edit view
                //---------------------------------------------------------------------
                commandLineView->setText("");
                editView->placeCursor();
                screen->flush();
                
                programMode = ScreenEditor::EDIT;

            }
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
    
    char buffer[200];
    
    if(    editView->findString( _findString ) == TRUE ) {
        
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
    saveCurrentEditBufferOnSwitch();
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

