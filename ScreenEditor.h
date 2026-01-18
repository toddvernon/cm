//-------------------------------------------------------------------------------------------------
//
//  ScreenEditor.h
//  cmacs
//
//  Created by Todd Vernon on 6/24/22.
//  Copyright © 2022 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <sys/types.h>

#include <cx/base/string.h>
#include <cx/base/slist.h>
#include <cx/base/star.h>
#include <cx/base/hashmap.h>

#include <cx/editbuffer/editbuffer.h>
#include <cx/editbuffer/edithint.h>
#include <cx/editbuffer/stringutils.h>
#include <cx/keyboard/keyboard.h>
#include <cx/screen/screen.h>
#include <cx/functor/defercall.h>
#include <cx/base/tokenizer.h>
#include <cx/base/fileaccess.h>

#include "EditView.h"
#include "CommandLineView.h"
#include "ProgramDefaults.h"
#include "Project.h"
#include "FileListView.h"
#include "HelpTextView.h"

#ifndef _ScreenEditor_h_
#define _ScreenEditor_h_


//-------------------------------------------------------------------------------------------------
//
//
//
//-------------------------------------------------------------------------------------------------

class ScreenEditor {

  public:

    enum ProgramMode {
        COMMANDLINE,
        EDIT,
        FILELIST,
		HELPVIEW
    };
    
     

    ScreenEditor( CxScreen *scr, CxKeyboard *key, CxString filePath="" );
    ~ScreenEditor(void);


	CxSList< CxString > loadHelpText(void);

    void run(void);
    
    void focusCommandPrompt(CxKeyAction keyAction);
    int  focusEditor(CxKeyAction keyAction);
    void focusFilelist( CxKeyAction keyAction);
    void focusHelpView( CxKeyAction keyAction);
    
    int executeCommand( CxString commandline );
    int gatherHint( void );

    void resetPrompt(void);
    void setMessage(CxString message);

    int handleControl( CxKeyAction keyAction);
    
    int loadNewFile( CxString filePath, int preload );
    // loads a new file into a new buffer and makes it the active buffer
    
    void saveCurrentEditBufferOnSwitch(void);
    // saves the current edit buffer
    
    void nextBuffer( void);
    void previousBuffer( void );
    
    int checkFile( CxString filePath );

    void CMD_Find(CxString commandLine);
    void CONTROL_FindAgain( void );

    void CMD_Replace(CxString commandLine);
    void CONTROL_ReplaceAgain( void );
    
    void CONTROL_ToggleLineNumbers( void );
    void CONTROL_ToggleJumpScroll( void );

    void CMD_SaveFile( CxString commandLine );
    void CMD_LoadFile( CxString commandLine );
    void CMD_SetMark( CxString commandLine );
    void CMD_GotoLine( CxString commandLine );
    void CMD_CutToMark( CxString commandLine );
    void CMD_PasteText( CxString commandLine );
	void CMD_CommentBlock( CxString commandLine );
    void CMD_NewBuffer( CxString commandLine );
    void CMD_ListProjectFiles( CxString commandLine );

    // Control key command handlers (called from dispatch table)
    void CTRL_Cut(void);
    void CTRL_Paste(void);
    void CTRL_CutToEndOfLine(void);
    void CTRL_PageDown(void);
    void CTRL_PageUp(void);
    void CTRL_NextBuffer(void);
    void CTRL_ProjectList(void);
    void CTRL_UpdateScreen(void);
    void CTRL_Help(void);

    // Control-X subcommand handlers
    void CTRLX_Save(void);
    void CTRLX_Quit(void);

    ProgramMode programMode;

    ProgramDefaults *programDefaults;
    CxScreen   *screen;
    CxKeyboard *keyboard;
    EditView   *editView;
    CommandLineView *commandLineView;
    CxEditBufferList *editBufferList;
    
    FileListView *fileListView;
	HelpTextView *helpTextView;
    
    Project *project;


    //CxString _filePath;

	CxString _cutBuffer;
    
    CxString _findString;
    CxString _replaceString;

private:
    // Dispatch table entry for control commands
    struct ControlCmd {
        const char* tag;
        void (ScreenEditor::*handler)(void);
        const char* message;  // NULL if handler sets its own message
    };

    static ControlCmd _controlCommands[];
    static ControlCmd _ctrlXCommands[];

    int dispatchControlX(void);
};


#endif

