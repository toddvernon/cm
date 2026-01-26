//-------------------------------------------------------------------------------------------------
//
//  CommandRegistry.h
//  cmacs
//
//  Created by Todd Vernon on 1/25/26.
//  Copyright © 2026 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------

#ifndef _CommandRegistry_h_
#define _CommandRegistry_h_

#include <cx/base/string.h>

//-------------------------------------------------------------------------------------------------
// Command flags
//-------------------------------------------------------------------------------------------------
#define CMD_FLAG_NEEDS_ARG      0x01    // command requires an argument
#define CMD_FLAG_OPTIONAL_ARG   0x02    // argument is optional

//-------------------------------------------------------------------------------------------------
// Command handler function type
//-------------------------------------------------------------------------------------------------
class ScreenEditor;
typedef void (ScreenEditor::*CommandHandler)( CxString arg );

//-------------------------------------------------------------------------------------------------
// CommandEntry - defines a single command
//-------------------------------------------------------------------------------------------------
struct CommandEntry
{
    const char *name;           // "find", "find-all", "goto-line"
    const char *argHint;        // hint for argument: "<pattern>", "<line>", or NULL
    const char *description;    // "Search for text in buffer"
    int flags;                  // CMD_FLAG_*
    CommandHandler handler;     // pointer to member function
};

//-------------------------------------------------------------------------------------------------
// CommandRegistry - manages command table and provides completion
//-------------------------------------------------------------------------------------------------
class CommandRegistry
{
public:

    CommandRegistry( void );
    // constructor

    int findMatches( CxString prefix, CommandEntry **matches, int maxMatches );
    // returns count of commands matching prefix (fuzzy, ignores hyphens)
    // fills matches array with pointers to matching entries

    CommandEntry *findExact( CxString name );
    // returns exact match or NULL

    CxString completePrefix( CxString prefix );
    // returns longest common prefix among all matches (using dehyphenated form)

    int commandCount( void );
    // total number of registered commands

    CommandEntry *commandAt( int index );
    // return command at index (for iteration)

private:

    int matchesPrefix( CxString commandName, CxString prefix );
    // fuzzy prefix match - ignores hyphens in command name

    int matchesAcronym( CxString commandName, CxString prefix );
    // acronym match - "bl" matches "buffer-list"

    CxString dehyphenate( CxString s );
    // remove hyphens from string

    CxString acronym( CxString commandName );
    // compute acronym - "buffer-list" -> "bl"

    static CommandEntry _commands[];
};

#endif
