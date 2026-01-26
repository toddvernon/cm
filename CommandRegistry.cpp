//-------------------------------------------------------------------------------------------------
//
//  CommandRegistry.cpp
//  cmacs
//
//  Created by Todd Vernon on 1/25/26.
//  Copyright © 2026 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------

#include "CommandRegistry.h"
#include "ScreenEditor.h"

//-------------------------------------------------------------------------------------------------
// Command table
//
// Commands are matched using fuzzy prefix matching that ignores hyphens.
// For example: "fa" matches "find-all", "gl" matches "goto-line"
//-------------------------------------------------------------------------------------------------
CommandEntry CommandRegistry::_commands[] = {

    //---------------------------------------------------------------------------------------------
    // Find commands
    //---------------------------------------------------------------------------------------------
    { "find",
      "<pattern>",
      "Search for text in buffer",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_Find },

    //---------------------------------------------------------------------------------------------
    // Buffer info commands
    //---------------------------------------------------------------------------------------------
    { "count",
      NULL,
      "Count lines and characters in buffer",
      0,
      &ScreenEditor::CMD_Count },

    //---------------------------------------------------------------------------------------------
    // Replace commands
    //---------------------------------------------------------------------------------------------
    { "replace",
      "<replacement>",
      "Replace next occurrence (uses last find)",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_Replace },

    { "replace-all",
      "<replacement>",
      "Replace all occurrences (uses last find)",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_ReplaceAll },

    //---------------------------------------------------------------------------------------------
    // Navigation commands
    //---------------------------------------------------------------------------------------------
    { "goto-line",
      "<line>",
      "Go to specified line number",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_GotoLine },

    //---------------------------------------------------------------------------------------------
    // File commands
    //---------------------------------------------------------------------------------------------
    { "save",
      "[filename]",
      "Save current buffer",
      CMD_FLAG_OPTIONAL_ARG,
      &ScreenEditor::CMD_SaveFile },

    { "save-as",
      "<filename>",
      "Save buffer to new file",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_SaveFile },

    { "load",
      "<filename>",
      "Load file into new buffer",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_LoadFile },

    //---------------------------------------------------------------------------------------------
    // Buffer commands
    //---------------------------------------------------------------------------------------------
    { "buffer-next",
      NULL,
      "Switch to next buffer",
      0,
      &ScreenEditor::CMD_BufferNext },

    { "buffer-prev",
      NULL,
      "Switch to previous buffer",
      0,
      &ScreenEditor::CMD_BufferPrev },

    { "buffer-new",
      "<filename>",
      "Create new buffer",
      CMD_FLAG_NEEDS_ARG,
      &ScreenEditor::CMD_NewBuffer },

    { "buffer-list",
      NULL,
      "Show project/buffer list",
      0,
      &ScreenEditor::CMD_BufferList },

    //---------------------------------------------------------------------------------------------
    // Mark and cut/paste commands
    //---------------------------------------------------------------------------------------------
    { "mark",
      NULL,
      "Set mark at cursor position",
      0,
      &ScreenEditor::CMD_SetMark },

    { "cut",
      NULL,
      "Cut from mark to cursor",
      0,
      &ScreenEditor::CMD_CutToMark },

    { "paste",
      NULL,
      "Paste from cut buffer",
      0,
      &ScreenEditor::CMD_PasteText },

    //---------------------------------------------------------------------------------------------
    // Application commands
    //---------------------------------------------------------------------------------------------
    { "quit",
      NULL,
      "Quit editor",
      0,
      &ScreenEditor::CMD_Quit },

    { "help",
      NULL,
      "Show help screen",
      0,
      &ScreenEditor::CMD_Help },

    //---------------------------------------------------------------------------------------------
    // End of table
    //---------------------------------------------------------------------------------------------
    { NULL, NULL, NULL, 0, NULL }
};


//-------------------------------------------------------------------------------------------------
// CommandRegistry::CommandRegistry
//-------------------------------------------------------------------------------------------------
CommandRegistry::CommandRegistry( void )
{
}


//-------------------------------------------------------------------------------------------------
// CommandRegistry::dehyphenate
//
// Remove hyphens from a string for fuzzy matching
//-------------------------------------------------------------------------------------------------
CxString
CommandRegistry::dehyphenate( CxString s )
{
    CxString result = "";

    for (unsigned long i = 0; i < s.length(); i++) {
        char c = (char) s.charAt( (int) i );
        if (c != '-') {
            result += c;
        }
    }

    return result;
}


//-------------------------------------------------------------------------------------------------
// CommandRegistry::matchesPrefix
//
// Fuzzy prefix match - user input matches if it's a prefix of the dehyphenated command name
//
// Examples:
//   "f"    matches "find"         (find starts with f)
//   "fa"   matches "find-all"     (findall starts with fa)
//   "fal"  matches "find-all"     (findall starts with fal)
//   "fr"   matches "find-replace" (findreplace starts with fr)
//   "gl"   matches "goto-line"    (gotoline starts with gl)
//-------------------------------------------------------------------------------------------------
int
CommandRegistry::matchesPrefix( CxString commandName, CxString prefix )
{
    // if user typed more chars than the command name, can't match
    // this prevents "find-" from matching "find"
    if (prefix.length() > commandName.length()) {
        return FALSE;
    }

    CxString dehyphenCmd = dehyphenate( commandName );
    CxString dehyphenPfx = dehyphenate( prefix );

    if (dehyphenPfx.length() > dehyphenCmd.length()) {
        return FALSE;
    }

    for (unsigned long i = 0; i < dehyphenPfx.length(); i++) {
        if (dehyphenPfx.charAt( (int) i ) != dehyphenCmd.charAt( (int) i )) {
            return FALSE;
        }
    }

    return TRUE;
}


//-------------------------------------------------------------------------------------------------
// CommandRegistry::acronym
//
// Compute acronym from command name - first letter of each hyphen-separated segment
// Examples: "buffer-list" -> "bl", "goto-line" -> "gl", "find" -> "f"
//-------------------------------------------------------------------------------------------------
CxString
CommandRegistry::acronym( CxString commandName )
{
    CxString result = "";
    int atStart = TRUE;

    for (unsigned long i = 0; i < commandName.length(); i++) {
        char c = (char) commandName.charAt( (int) i );
        if (c == '-') {
            atStart = TRUE;
        } else if (atStart) {
            result += c;
            atStart = FALSE;
        }
    }

    return result;
}


//-------------------------------------------------------------------------------------------------
// CommandRegistry::matchesAcronym
//
// Check if user input matches the command's acronym
// Examples: "bl" matches "buffer-list", "gl" matches "goto-line"
//-------------------------------------------------------------------------------------------------
int
CommandRegistry::matchesAcronym( CxString commandName, CxString prefix )
{
    CxString acro = acronym( commandName );

    // prefix must be at least as long as acronym for exact match,
    // or acronym must start with prefix for partial match
    if (prefix.length() > acro.length()) {
        return FALSE;
    }

    // check if acronym starts with prefix
    for (unsigned long i = 0; i < prefix.length(); i++) {
        if (prefix.charAt( (int) i ) != acro.charAt( (int) i )) {
            return FALSE;
        }
    }

    return TRUE;
}


//-------------------------------------------------------------------------------------------------
// CommandRegistry::findMatches
//
// Find all commands matching the given prefix via:
//   1. Prefix match (ignoring hyphens): "buffer" matches "buffer-list"
//   2. Acronym match: "bl" matches "buffer-list"
//-------------------------------------------------------------------------------------------------
int
CommandRegistry::findMatches( CxString prefix, CommandEntry **matches, int maxMatches )
{
    int count = 0;

    for (int i = 0; _commands[i].name != NULL && count < maxMatches; i++) {
        if (matchesPrefix( _commands[i].name, prefix ) ||
            matchesAcronym( _commands[i].name, prefix )) {
            matches[count++] = &_commands[i];
        }
    }

    return count;
}


//-------------------------------------------------------------------------------------------------
// CommandRegistry::findExact
//
// Find command with exact name match
//-------------------------------------------------------------------------------------------------
CommandEntry *
CommandRegistry::findExact( CxString name )
{
    for (int i = 0; _commands[i].name != NULL; i++) {
        if (name == _commands[i].name) {
            return &_commands[i];
        }
    }

    return NULL;
}


//-------------------------------------------------------------------------------------------------
// CommandRegistry::completePrefix
//
// Return the longest common prefix among all matching commands
// Uses dehyphenated form for matching, but returns actual command name portion
//-------------------------------------------------------------------------------------------------
CxString
CommandRegistry::completePrefix( CxString prefix )
{
    CommandEntry *matches[64];
    int count = findMatches( prefix, matches, 64 );

    if (count == 0) {
        return prefix;
    }

    if (count == 1) {
        return matches[0]->name;
    }

    // find longest common prefix among dehyphenated names
    CxString first = dehyphenate( matches[0]->name );
    unsigned long commonLen = first.length();

    for (int i = 1; i < count; i++) {
        CxString other = dehyphenate( matches[i]->name );
        unsigned long len = 0;

        while (len < commonLen && len < other.length() &&
               first.charAt( (int) len ) == other.charAt( (int) len )) {
            len++;
        }

        commonLen = len;
    }

    // return the dehyphenated common prefix (user continues typing without hyphens)
    return first.subString( 0, commonLen );
}


//-------------------------------------------------------------------------------------------------
// CommandRegistry::commandCount
//-------------------------------------------------------------------------------------------------
int
CommandRegistry::commandCount( void )
{
    int count = 0;

    while (_commands[count].name != NULL) {
        count++;
    }

    return count;
}


//-------------------------------------------------------------------------------------------------
// CommandRegistry::commandAt
//-------------------------------------------------------------------------------------------------
CommandEntry *
CommandRegistry::commandAt( int index )
{
    if (index < 0 || index >= commandCount()) {
        return NULL;
    }

    return &_commands[index];
}
