//-------------------------------------------------------------------------------------------------
//
//  ScreenEditorCore.cpp
//  cmacs
//
//  File management, buffer switching, control dispatch, and dispatch tables for ScreenEditor.
//  Extracted from ScreenEditor.cpp to reduce file size.
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
    { "O",    &ScreenEditor::CTRL_SwitchView,           NULL },
    { "B",    &ScreenEditor::CTRL_ShowBuild,            NULL },
    { NULL,   NULL,                                      NULL }
};

ScreenEditor::ControlCmd ScreenEditor::_ctrlXCommands[] = {
    { "S",    &ScreenEditor::CTRLX_Save,  NULL },
    { "C",    &ScreenEditor::CTRLX_Quit,  NULL },
    { NULL,   NULL,                        NULL }
};


//-------------------------------------------------------------------------------------------------
// ScreenEditor::loadNewFile
//
// load a new file into a new buffer and make it the viewable buffer
// Returns: 1 on success, 0 on failure
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
            CmEditBuffer *editBuffer = editBufferList->findPath( filePath );

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
                activeEditView()->setEditBuffer( editBuffer );

                commandLineView->updateScreen();
                return(1);  // Success - existing buffer

            // we did not find an edit buffer in the list, so create a new one
            // referencing that file path
            } else {

                // if the user wants autosave on buffer changes
                if (programDefaults->autoSaveOnBufferChange()) {
                    saveCurrentEditBufferOnSwitch();
                }

                // create a new edit buffer
                CmEditBuffer *editBuffer = new CmEditBuffer( );

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
                activeEditView()->setEditBuffer( editBuffer );

                commandLineView->updateScreen();
                return(1);  // Success - new buffer created

            }
        }
    }

    commandLineView->updateScreen();

    return(0);  // Failure - empty path or checkFile failed
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
    CmEditBuffer *currentEditBuffer = editBufferList->current();
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
    CmEditBuffer *editBuffer = editBufferList->next();

    // see if the buffer is in memory
    if (!editBuffer->isInMemory()) {

        // if not then load the text
        editBuffer->loadText( editBuffer->getFilePath() , TRUE );
    }

    // hand it to the edit view to display
    activeEditView()->setEditBuffer( editBuffer );

    // redraw the edit view
    activeEditView()->reframeAndUpdateScreen();

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
    CmEditBuffer *editBuffer = editBufferList->previous();

    // see if the buffer is in memory
    if (!editBuffer->isInMemory()) {

        // if not then load the text
        editBuffer->loadText( editBuffer->getFilePath() , TRUE );
    }

    // hand it to the edit view to display
    activeEditView()->setEditBuffer( editBuffer );

    // redraw the edit view
    activeEditView()->reframeAndUpdateScreen();
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
        if (programMode == EDIT) {
            activeEditView()->placeCursor();
            screen->flush();
        }
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
            if (programMode == EDIT) {
                activeEditView()->placeCursor();
                screen->flush();
            }

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
        activeEditView()->routeKeyAction(keyAction);
        return 0;
    }

    // Look up in dispatch table
    for (int i = 0; _controlCommands[i].tag != NULL; i++) {
        if (keyAction.tag() == _controlCommands[i].tag) {
            if (_controlCommands[i].message != NULL) {
                setMessage(_controlCommands[i].message);
            }
            (this->*_controlCommands[i].handler)();
            if (programMode == EDIT) {
                activeEditView()->placeCursor();
                screen->flush();
            }
            return 0;
        }
    }

    return 0;
}
