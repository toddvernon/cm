//-------------------------------------------------------------------------------------------------
//
//  CommandTable.cpp
//  cmacs
//
//  Static command table - defines all ESC commands.
//  Matching and completion are handled by the Completer library.
//
//  Commands follow a category-action naming convention for menu-like discoverability.
//  Categories are ordered by traditional menu bar precedence:
//  File, Edit, Search, Goto, Insert, Text, View, Project
//
//  Each category is unique at the first keystroke:
//  f=file, e=edit, s=search, g=goto, i=insert, t=text, v=view, p=project
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
// The symbolFilter field is used for commands with CMD_FLAG_SYMBOL_ARG to set up
// child completers (e.g., "box-" for insert-box, "sym-" for insert-symbol).
//-------------------------------------------------------------------------------------------------
CommandEntry commandTable[] = {

    //--- file- ---------------------------------------------------------------
    { "file-load",
      "<filename>",
      "Load file into new buffer",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_LoadFile,
      NULL },

    { "file-new",
      "<filename>",
      "Create new buffer",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_NewBuffer,
      NULL },

    { "file-quit",
      NULL,
      "Quit editor",
      0,
      &ScreenEditor::CMD_Quit,
      NULL },

    { "file-save",
      "[filename]",
      "Save current buffer",
      CMD_FLAG_OPTIONAL_ARG,
      &ScreenEditor::CMD_SaveFile,
      NULL },

    { "file-save-as",
      "<filename>",
      "Save buffer to new file",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_SaveFile,
      NULL },

    //--- edit- ---------------------------------------------------------------
    { "edit-cut",
      NULL,
      "Cut from mark to cursor",
      0,
      &ScreenEditor::CMD_CutToMark,
      NULL },

    { "edit-mark",
      NULL,
      "Set mark at cursor position",
      0,
      &ScreenEditor::CMD_SetMark,
      NULL },

    { "edit-paste",
      NULL,
      "Paste from cut buffer",
      0,
      &ScreenEditor::CMD_PasteText,
      NULL },

    { "edit-system-paste",
      NULL,
      "Paste from system clipboard",
      0,
      &ScreenEditor::CMD_SystemPaste,
      NULL },

    //--- search- -------------------------------------------------------------
    { "search-text",
      "<pattern>",
      "Search for text in buffer",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_Find,
      NULL },

    { "search-replace",
      "<replacement>",
      "Replace next occurrence (uses last search)",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_Replace,
      NULL },

    { "search-replace-all",
      "<replacement>",
      "Replace all occurrences (uses last search)",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_ReplaceAll,
      NULL },

    //--- goto- ---------------------------------------------------------------
    { "goto-error",
      NULL,
      "Jump to file:line from error message under cursor",
      0,
      &ScreenEditor::CMD_GotoError,
      NULL },

    { "goto-line",
      "<line>",
      "Go to specified line number",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_GotoLine,
      NULL },

    //--- insert- -------------------------------------------------------------
#ifdef CM_UTF8_SUPPORT
    { "insert-box",
      "<symbol>",
      "Insert box drawing symbol (TAB for completion)",
      CMD_FLAG_NEEDS_ARG | CMD_FLAG_SYMBOL_ARG,
      &ScreenEditor::CMD_InsertUTFBox,
      "box-" },
#endif

    { "insert-comment-block",
      "<column>",
      "Insert comment block to column",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_CommentBlock,
      NULL },

#ifdef CM_UTF8_SUPPORT
    { "insert-symbol",
      "<symbol>",
      "Insert common symbol (TAB for completion)",
      CMD_FLAG_NEEDS_ARG | CMD_FLAG_SYMBOL_ARG,
      &ScreenEditor::CMD_InsertUTFSymbol,
      "sym-" },
#endif

    //--- text- ---------------------------------------------------------------
    { "text-count",
      NULL,
      "Count lines and characters in buffer",
      0,
      &ScreenEditor::CMD_Count,
      NULL },

    { "text-detab",
      NULL,
      "Convert tabs to spaces",
      0,
      &ScreenEditor::CMD_Detab,
      NULL },

    { "text-entab",
      NULL,
      "Convert leading spaces to tabs",
      0,
      &ScreenEditor::CMD_Entab,
      NULL },

    { "text-trim-trailing",
      NULL,
      "Remove trailing whitespace from all lines",
      0,
      &ScreenEditor::CMD_TrimTrailing,
      NULL },

    //--- view- ---------------------------------------------------------------
    { "view-build",
      NULL,
      "Show build output",
      0,
      &ScreenEditor::CMD_ShowBuild,
      NULL },

    { "view-help",
      NULL,
      "Show help screen",
      0,
      &ScreenEditor::CMD_Help,
      NULL },

    { "view-split",
      NULL,
      "Split screen horizontally",
      0,
      &ScreenEditor::CMD_Split,
      NULL },

    { "view-unsplit",
      NULL,
      "Return to single view",
      0,
      &ScreenEditor::CMD_Unsplit,
      NULL },

    //--- project (standalone, opens dialog) ----------------------------------
    { "project",
      NULL,
      "Open project dialog",
      0,
      &ScreenEditor::CMD_ProjectShow,
      NULL },

    //--- end -----------------------------------------------------------------
    { NULL, NULL, NULL, 0, NULL, NULL }
};
