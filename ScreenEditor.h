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

#include <cx/editbuffer/edithint.h>
#include "CmTypes.h"
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
#include "ProjectView.h"
#include "HelpView.h"
#include "CommandTable.h"
#include <cx/commandcompleter/completer.h>
#include <cx/buildoutput/buildoutput.h>
#include "BuildView.h"

#if defined(_LINUX_) || defined(_OSX_)
#include "MCPHandler.h"
#endif

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
        PROJECTVIEW,
		HELPVIEW,
        BUILDVIEW
    };

    enum CommandInputState {
        CMD_INPUT_IDLE,         // not in command input mode
        CMD_INPUT_COMMAND,      // entering command name
        CMD_INPUT_ARGUMENT      // entering command argument
    };
    
     

    ScreenEditor( CxScreen *scr, CxKeyboard *key, CxString filePath="" );
    ~ScreenEditor(void);


    void run(void);

    void focusCommandPrompt(CxKeyAction keyAction);
    int  focusEditor(CxKeyAction keyAction);
    void focusProjectView( CxKeyAction keyAction);
    void focusHelpView( CxKeyAction keyAction);
    void focusBuildView( CxKeyAction keyAction);

    // command input methods
    void enterCommandMode( void );
    void handleCommandInput( CxKeyAction keyAction );
    void updateCommandDisplay( void );
    void updateArgumentDisplay( void );
    void executeCurrentCommand( void );

    // command input helpers (private implementation)
    void initCommandCompleters( void );
    void selectCommand( CommandEntry *cmd );
    void cancelCommandInput( void );
    void handleCommandModeInput( CxKeyAction keyAction );
    void handleArgumentModeInput( CxKeyAction keyAction );

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
    void CMD_ReplaceAll(CxString commandLine);
    void CONTROL_ReplaceAgain( void );
    
    void CONTROL_ToggleLineNumbers( void );
    void CONTROL_ToggleJumpScroll( void );

    void CMD_SaveFile( CxString commandLine );
    void CMD_LoadFile( CxString commandLine );
    void CMD_SetMark( CxString commandLine );
    void CMD_GotoLine( CxString commandLine );
    void CMD_CutToMark( CxString commandLine );
    void CMD_PasteText( CxString commandLine );
    void CMD_SystemPaste( CxString commandLine );
	void CMD_CommentBlock( CxString commandLine );
    void CMD_NewBuffer( CxString commandLine );
    void CMD_Quit( CxString commandLine );
    void CMD_Help( CxString commandLine );
    void CMD_Count( CxString commandLine );
    void CMD_Entab( CxString commandLine );
    void CMD_Detab( CxString commandLine );
    void CMD_TrimTrailing( CxString commandLine );
    void CMD_GotoError( CxString commandLine );
    void CMD_ProjectShow( CxString commandLine );
    void CMD_ShowBuild( CxString commandLine );
    void CMD_Split( CxString commandLine );
    void CMD_Unsplit( CxString commandLine );
#ifdef CM_UTF8_SUPPORT
    void CMD_InsertUTFBox( CxString commandLine );
    void CMD_InsertUTFSymbol( CxString commandLine );
    void insertUTFSymbolHelper( CxString commandLine, const char *symbolType );
#endif

    // Control key command handlers (called from dispatch table)
    void CTRL_Cut(void);
    void CTRL_Paste(void);
    void CTRL_CutToEndOfLine(void);
    void CTRL_PageDown(void);
    void CTRL_PageUp(void);
    void CTRL_NextBuffer(void);
    void CTRL_ProjectList(void);
    void CTRL_Help(void);
    void CTRL_SwitchView(void);
    void CTRL_ShowBuild(void);
    void CTRL_Split(void);
    void CTRL_Unsplit(void);

    // Control-X subcommand handlers
    void CTRLX_Save(void);
    void CTRLX_Quit(void);

    // Split screen methods
    void splitHorizontal(void);         // split screen in half
    void unsplit(void);                 // return to single view
    void switchActiveView(void);        // toggle between top/bottom
    EditView* activeEditView(void);     // return currently active view
    void recalcSplitRegions(void);      // recalculate regions after resize

    ProgramMode programMode;

    ProgramDefaults *programDefaults;
    CxScreen   *screen;
    CxKeyboard *keyboard;
    EditView   *editView;
    EditView   *editViewBottom;         // bottom view when split (NULL if not split)
    CommandLineView *commandLineView;
    CmEditBufferList *editBufferList;
    
    ProjectView *projectView;
	HelpView *helpView;
    BuildView *buildView;
    BuildOutput *buildOutput;

    Project *project;


    //CxString _filePath;

	CxString _cutBuffer;
    
    CxString _findString;
    CxString _replaceString;
    CxString _buildStatusPrefix;    // "Building..." while build is running

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

    // helper methods to reduce duplication
    void setMessageWithLocation(CxString prefix);
    void showProjectView(void);
    void showHelpView(void);
    void showBuildView(void);
    void startBuild(ProjectSubproject *sub, CxString makeTarget);
    void startBuildAll(CxString makeTarget);
    void continueBuildAll(void);
    void buildIdleCallback(void);       // polls build output during keyboard idle
    void returnToEditMode(void);        // dismiss modal and redraw editors
    void enterCommandLineMode(void);    // EDIT -> COMMANDLINE
    void exitCommandLineMode(void);     // COMMANDLINE -> EDIT
    void resetCommandInputState(void);

    // decomposed command input handlers
    void handleCommandEnter(void);
    void handleCommandTab(void);
    void handleCommandChar(CxKeyAction keyAction);

    // decomposed display methods
    void updateCommandDisplayForSymbol(void);
    void renderCommandLine(CxString prefix, CxString display, unsigned long cursorOffset);

    // command completion
    Completer       _commandCompleter;      // top-level command selection
#ifdef CM_UTF8_SUPPORT
    Completer       _boxSymbolCompleter;    // child: box drawing symbols
    Completer       _symSymbolCompleter;    // child: common symbols
#endif
    Completer      *_activeCompleter;       // points to whichever is active

    // command input state
    CommandInputState _cmdInputState;
    CxString _cmdBuffer;            // input for active completer
    CxString _argBuffer;            // freeform argument text
    CommandEntry *_currentCommand;  // selected command (after completion)
    int _quitRequested;             // set by CMD_Quit to signal exit

    // build state
    ProjectSubproject *_activeBuildSubproject;  // current/last build target
    int _buildAllIndex;                         // -1 if not "build all", else current subproject index
    CxString _buildAllTarget;                   // make target for "build all"

    // split screen state
    int _splitMode;                 // 0 = single view, 1 = horizontal split
    int _splitRow;                  // screen row where divider sits
    int _activeView;                // 0 = top/only, 1 = bottom

    // resize callback - coordinates all redrawing based on programMode
    void screenResizeCallback(void);

#if defined(_LINUX_) || defined(_OSX_)
    MCPHandler *_mcpHandler;        // MCP socket handler thread
    void mcpIdleCallback();         // Called during keyboard idle to check for MCP updates
#endif
};


#endif

