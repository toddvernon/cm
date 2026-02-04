//-------------------------------------------------------------------------------------------------
//
//  CommandTable.cpp
//  cmacs
//
//  Static command table - defines all ESC commands.
//  Matching and completion are handled by the Completer library.
//
//  Created by Todd Vernon on 1/25/26.
//  Copyright (c) 2026 Todd Vernon. All rights reserved.
//
//-------------------------------------------------------------------------------------------------

#include "CommandTable.h"
#include "CmTypes.h"
#include "ScreenEditor.h"

//-------------------------------------------------------------------------------------------------
// Command table
//
// Commands are registered into Completer objects at startup for literal prefix matching.
// For example: "goto" matches "goto-line", "find" matches "find"
//
// The symbolFilter field is used for commands with CMD_FLAG_SYMBOL_ARG to set up
// child completers (e.g., "box-" for utf-box, "sym-" for utf-symbol).
//-------------------------------------------------------------------------------------------------
CommandEntry commandTable[] = {

    //---------------------------------------------------------------------------------------------
    // Find commands
    //---------------------------------------------------------------------------------------------
    { "find",
      "<pattern>",
      "Search for text in buffer",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_Find,
      NULL },

    //---------------------------------------------------------------------------------------------
    // Buffer info commands
    //---------------------------------------------------------------------------------------------
    { "wc",
      NULL,
      "Count lines and characters in buffer",
      0,
      &ScreenEditor::CMD_Count,
      NULL },

    //---------------------------------------------------------------------------------------------
    // Tab conversion commands
    //---------------------------------------------------------------------------------------------
    { "entab",
      NULL,
      "Convert leading spaces to tabs",
      0,
      &ScreenEditor::CMD_Entab,
      NULL },

    { "detab",
      NULL,
      "Convert tabs to spaces",
      0,
      &ScreenEditor::CMD_Detab,
      NULL },

    //---------------------------------------------------------------------------------------------
    // UTF symbol insertion (modern platforms only)
    //---------------------------------------------------------------------------------------------
#ifdef CM_UTF8_SUPPORT
    { "utf-box",
      "<symbol>",
      "Insert box drawing symbol (TAB for completion)",
      CMD_FLAG_NEEDS_ARG | CMD_FLAG_SYMBOL_ARG,
      &ScreenEditor::CMD_InsertUTFBox,
      "box-" },

    { "utf-symbol",
      "<symbol>",
      "Insert common symbol (TAB for completion)",
      CMD_FLAG_NEEDS_ARG | CMD_FLAG_SYMBOL_ARG,
      &ScreenEditor::CMD_InsertUTFSymbol,
      "sym-" },
#endif

    //---------------------------------------------------------------------------------------------
    // Replace commands
    //---------------------------------------------------------------------------------------------
    { "replace",
      "<replacement>",
      "Replace next occurrence (uses last find)",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_Replace,
      NULL },

    { "replace-all",
      "<replacement>",
      "Replace all occurrences (uses last find)",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_ReplaceAll,
      NULL },

    //---------------------------------------------------------------------------------------------
    // Navigation commands
    //---------------------------------------------------------------------------------------------
    { "goto-line",
      "<line>",
      "Go to specified line number",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_GotoLine,
      NULL },

    //---------------------------------------------------------------------------------------------
    // File commands
    //---------------------------------------------------------------------------------------------
    { "save",
      "[filename]",
      "Save current buffer",
      CMD_FLAG_OPTIONAL_ARG,
      &ScreenEditor::CMD_SaveFile,
      NULL },

    { "save-as",
      "<filename>",
      "Save buffer to new file",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_SaveFile,
      NULL },

    { "load",
      "<filename>",
      "Load file into new buffer",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_LoadFile,
      NULL },

    //---------------------------------------------------------------------------------------------
    // Buffer commands
    //---------------------------------------------------------------------------------------------
    { "buffer-next",
      NULL,
      "Switch to next buffer",
      0,
      &ScreenEditor::CMD_BufferNext,
      NULL },

    { "buffer-prev",
      NULL,
      "Switch to previous buffer",
      0,
      &ScreenEditor::CMD_BufferPrev,
      NULL },

    { "buffer-new",
      "<filename>",
      "Create new buffer",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_NewBuffer,
      NULL },

    { "buffer-list",
      NULL,
      "Show project/buffer list",
      0,
      &ScreenEditor::CMD_BufferList,
      NULL },

    //---------------------------------------------------------------------------------------------
    // Mark and cut/paste commands
    //---------------------------------------------------------------------------------------------
    { "mark",
      NULL,
      "Set mark at cursor position",
      0,
      &ScreenEditor::CMD_SetMark,
      NULL },

    { "cut",
      NULL,
      "Cut from mark to cursor",
      0,
      &ScreenEditor::CMD_CutToMark,
      NULL },

    { "paste",
      NULL,
      "Paste from cut buffer",
      0,
      &ScreenEditor::CMD_PasteText,
      NULL },

    { "system-paste",
      NULL,
      "Paste from system clipboard",
      0,
      &ScreenEditor::CMD_SystemPaste,
      NULL },

    //---------------------------------------------------------------------------------------------
    // Code editing commands
    //---------------------------------------------------------------------------------------------
    { "comment-block",
      "<column>",
      "Insert comment block to column",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_CommentBlock,
      NULL },

    //---------------------------------------------------------------------------------------------
    // Project commands
    //---------------------------------------------------------------------------------------------
    { "project-list",
      NULL,
      "Show project file list",
      0,
      &ScreenEditor::CMD_ListProjectFiles,
      NULL },

    //---------------------------------------------------------------------------------------------
    // Application commands
    //---------------------------------------------------------------------------------------------
    { "quit",
      NULL,
      "Quit editor",
      0,
      &ScreenEditor::CMD_Quit,
      NULL },

    { "help",
      NULL,
      "Show help screen",
      0,
      &ScreenEditor::CMD_Help,
      NULL },

    //---------------------------------------------------------------------------------------------
    // End of table
    //---------------------------------------------------------------------------------------------
    { NULL, NULL, NULL, 0, NULL, NULL }
};
