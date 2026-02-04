# CMacs Command System Design

This document describes the ESC command system with tab completion, including the
two-phase matching strategy and abstraction patterns.

## Overview

The command system uses a state machine with three states:
- `CMD_INPUT_IDLE` - normal editing mode
- `CMD_INPUT_COMMAND` - typing a command name (after ESC)
- `CMD_INPUT_ARGUMENT` - typing an argument (after selecting a command)

## Key Files

- `CommandTable.h/cpp` - Static command table (data only)
- `Completer` library (`cx/commandcompleter`) - Literal prefix matching and completion
- `UTFSymbols.h/cpp` - UTF symbol table (matching uses Completer static methods)
- `ScreenEditor.cpp` - State machine and input handling (status-driven Completer API)

## Two-Phase Matching Strategy

**CRITICAL DESIGN PATTERN**: Matching and completion use different comparison strategies.

### Prefix Matching

Matching is prefix-based: the user's input must be a prefix of the candidate name.
- User types "f" -> matches "find"
- User types "goto" -> matches "goto-line"
- User types "buffer-" -> matches "buffer-list", "buffer-next", etc.

Users are expected to type hyphens as part of the command name.

### Common Prefix Completion

When computing the common prefix for tab completion, literal comparison is used:
- Matches ["utf-box", "utf-symbol"] -> common prefix is "utf-"
- Matches ["buffer-list", "buffer-next"] -> common prefix is "buffer-"

This preserves hyphens in the completed prefix.

## Auto-Completion Behavior

The system auto-completes as the user types, without requiring TAB:

### Command Auto-Completion

When typing a command name, if all matches share a common prefix longer than the
current input, the input is automatically extended to that prefix.

| Input | Matches | Common Prefix | Result |
|-------|---------|---------------|--------|
| "b" | buffer-next, buffer-prev, buffer-new, buffer-list | "buffer-" | Auto-complete to "buffer-" |
| "u" | utf-box, utf-symbol | "utf-" | Auto-complete to "utf-" |
| "r" | replace, replace-all | "replace" | Auto-complete to "replace" |
| "s" | save, save-as, system-paste | "s" | No auto-complete (no shared prefix) |
| "sa" | save, save-as | "save" | Auto-complete to "save" |

When a command name is a prefix of another (like "replace" and "replace-all"),
auto-completing to the shorter name is correct. The user can:
- Press ENTER to execute the exact match ("replace")
- Continue typing to narrow further ("-a" for "replace-all")

### Symbol Argument Auto-Completion

When typing a symbol argument for commands like `utf-box`, if exactly one symbol
matches the input, it auto-completes immediately without requiring TAB.

| Input | Matches | Result |
|-------|---------|--------|
| "h" | horizontal | Auto-complete to "horizontal" |
| "u" | upper-left, upper-right | No auto-complete (multiple matches) |
| "upper-l" | upper-left | Auto-complete to "upper-left" |

This eliminates unnecessary TAB presses when the user's input uniquely identifies
a symbol.

## Completer API

The Completer library provides both static matching utilities and a status-driven
completion engine. The input loop reacts ONLY to CompleterStatus, never to command
table knowledge.

```cpp
class Completer {
public:
    // Setup
    void addCandidate( CxString name, Completer *childCompleter, void *userData );

    // Core operations - process input, return status
    CompleterResult processChar( CxString currentInput, char c );
    CompleterResult processTab( CxString currentInput );
    CompleterResult processEnter( CxString currentInput );

    // Query
    int findMatches( CxString input, CxString *names, int maxMatches );
    int findMatchesFull( CxString input, CompleterCandidate **matches, int maxMatches );

    // Static utilities
    static int matchesPrefix( CxString candidateName, CxString userInput );
    static CxString findCommonPrefix( CxString *names, int count );
};

enum CompleterStatus {
    COMPLETER_NO_MATCH,     // No matches
    COMPLETER_MULTIPLE,     // Multiple matches, show hints
    COMPLETER_PARTIAL,      // Auto-completed to common prefix
    COMPLETER_UNIQUE,       // Single match, awaiting ENTER
    COMPLETER_NEXT_LEVEL,   // Transition to child completer
    COMPLETER_SELECTED      // Selection confirmed (from processEnter)
};
```

## Command Entry Structure

```cpp
struct CommandEntry {
    const char *name;           // "find", "utf-box"
    const char *argHint;        // "<pattern>", "<symbol>"
    const char *description;    // "Search for text"
    int flags;                  // CMD_FLAG_NEEDS_ARG, CMD_FLAG_SYMBOL_ARG
    CommandHandler handler;     // Function pointer
    const char *symbolFilter;   // "box-", "sym-", or NULL
};
```

The `symbolFilter` field enables filtered symbol commands. For `utf-box`, the filter
is "box-" which limits completion to box drawing symbols and prepends "box-" to user
input when searching.

## Command Progression Examples

### Example 1: goto-line Command (Single Match)

```
ESC                     -> Show "command> " prompt
g                       -> Auto-complete to "goto-line" (only one match starting with g)
                        -> Prompt changes to "goto-line <line>: "
42                      -> Show "goto-line <line>: 42"
ENTER                   -> Execute command, jump to line 42
```

**Key behavior**: "g" uniquely matches "goto-line", so it auto-completes and
transitions to argument input immediately. No TAB or extra typing needed.

### Example 2: utf-box Command (with Symbol Filter)

```
ESC                     -> Show "command> " prompt
u                       -> Auto-complete to "utf-" (common prefix of utf-box, utf-symbol)
                        -> Show "command> utf-" hints: | utf-box | utf-symbol |
b                       -> Auto-complete to "utf-box" (only one match)
                        -> Prompt changes to "utf-box <symbol>: "
                        -> Hints show: | horizontal | vertical | upper-left | ...
h                       -> Auto-complete to "horizontal" (only one 'h' symbol)
ENTER                   -> Insert horizontal line symbol at cursor
```

**Key behaviors**:
1. "u" auto-completes to "utf-" because both commands share that prefix
2. "utf-b" auto-completes to "utf-box" because it's the only match
3. "h" auto-completes to "horizontal" because it's the only symbol starting with 'h'
4. Symbol filter "box-" is NOT shown to user - they type "horizontal" not "box-horizontal"

### Example 3: utf-box with Multiple Symbol Matches

```
ESC                     -> Show "command> " prompt
u                       -> Auto-complete to "utf-"
b                       -> Auto-complete to "utf-box", transition to argument
                        -> Hints show: | horizontal | vertical | upper-left | ...
u                       -> Show "utf-box <symbol>: u" hints: | upper-left | upper-right |
                        -> (no auto-complete, multiple matches)
l                       -> Auto-complete to "upper-left" (only one match now)
ENTER                   -> Insert upper-left corner symbol
```

**Key behavior**: When "u" matches multiple symbols (upper-left, upper-right),
no auto-completion occurs. Adding "l" narrows to one match, triggering auto-complete.

### Example 4: replace Command (Command as Prefix of Another)

```
ESC                     -> Show "command> " prompt
r                       -> Auto-complete to "replace" (common prefix)
                        -> Show "command> replace" hints: | replace | replace-all |
ENTER                   -> Execute "replace" command (exact match)
```

Or to get replace-all:

```
ESC                     -> Show "command> " prompt
r                       -> Auto-complete to "replace"
-                       -> Show "command> replace-" hint: | replace-all |
a                       -> Auto-complete to "replace-all" (only one match)
ENTER                   -> Execute "replace-all" command
```

**Key behavior**: When a command name is a prefix of another, auto-completing to
the shorter name lets the user press ENTER for that command, or continue typing
to reach the longer command.

### Example 5: find Command (Single Match, Needs Argument)

```
ESC                     -> Show "command> " prompt
f                       -> Auto-complete to "find" (only one match)
                        -> Prompt changes to "find <pattern>: "
hello                   -> Show "find <pattern>: hello"
ENTER                   -> Execute find, search for "hello"
```

## Adding a New Command

1. Add entry to `commandTable[]` in CommandTable.cpp:
```cpp
{ "my-command",
  "<arg>",                          // argument hint
  "Description of command",
  CMD_FLAG_NEEDS_ARG,               // or 0, CMD_FLAG_OPTIONAL_ARG
  &ScreenEditor::CMD_MyCommand,
  NULL },                           // symbolFilter, or "prefix-" for symbols
```

2. Implement handler in ScreenEditor:
```cpp
void ScreenEditor::CMD_MyCommand( CxString arg ) {
    // implementation
}
```

3. Declare in ScreenEditor.h.

## Adding a New Symbol Category

1. Add symbols to `UTFSymbols::_symbols[]` with prefix:
```cpp
{ "cat-name",
  "\xe2\x80\xa2",    // UTF-8 bytes
  "Description" },
```

2. Add command entry with symbolFilter:
```cpp
{ "utf-cat",
  "<symbol>",
  "Insert cat symbol",
  CMD_FLAG_NEEDS_ARG | CMD_FLAG_SYMBOL_ARG,
  &ScreenEditor::CMD_InsertUTFCat,
  "cat-" },                         // filter prefix
```

3. Implement handler using helper:
```cpp
void ScreenEditor::CMD_InsertUTFCat( CxString arg ) {
    insertUTFSymbolHelper( arg, "cat" );
}
```

## Common Pitfalls

1. **Forgetting hyphen in completion**: If auto-complete doesn't include shared
   hyphens, check that `findCommonPrefix()` uses literal comparison.

2. **Filter displayed to user**: Symbol filters should be invisible to users. They
   type "upper-left" not "box-upper-left". The filter is applied internally.

3. **Auto-completion not triggering**: Auto-completion only occurs when the common
   prefix is LONGER than the current input. If input equals the common prefix,
   no auto-completion happens (user must continue typing or press ENTER).

4. **Single match vs common prefix**: For commands, single match calls `selectCommand()`
   which may transition to argument mode. For multiple matches, only the common
   prefix is auto-completed, staying in command mode.

## State Machine

The input loop is driven entirely by `CompleterStatus`. It never hardcodes
knowledge of the command table.

```
EDIT mode
    |
    v (ESC key)
CMD_INPUT_COMMAND (_activeCompleter = &_commandCompleter)
    |
    +-- (character) -> processChar() on _activeCompleter
    |     COMPLETER_UNIQUE -> selectCommand() (freeform arg -> CMD_INPUT_ARGUMENT)
    |     COMPLETER_NEXT_LEVEL -> switch _activeCompleter to child, stay in COMMAND
    |     COMPLETER_PARTIAL/MULTIPLE -> update display with hints
    |     COMPLETER_NO_MATCH -> reject character
    +-- (TAB) -> processTab() on _activeCompleter (same status handling)
    +-- (ENTER) -> processEnter() on _activeCompleter
    |     COMPLETER_SELECTED -> execute or transition based on command flags
    |     COMPLETER_NEXT_LEVEL -> switch to child completer
    |     COMPLETER_MULTIPLE -> "(ambiguous command)"
    +-- (ESC) -> cancel, return to EDIT
    |
    v (selectCommand for freeform arg command)
CMD_INPUT_ARGUMENT
    |
    +-- (character) -> add to _argBuffer (plain text, no completion)
    +-- (ENTER) -> execute command, return to EDIT
    +-- (ESC) -> cancel, return to EDIT
```

Symbol argument selection (utf-box, utf-symbol) uses child completers and stays
in CMD_INPUT_COMMAND state - the child completer handles completion. On ENTER
with COMPLETER_SELECTED at child level, the symbol name becomes _argBuffer and
the command executes.
