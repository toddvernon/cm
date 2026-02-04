# Completer Integration Plan for CMacs

This document describes how the `Completer` library will be used in cm.

## Command Categories

### Simple commands (no arguments)
- `quit`, `help`, `buffer-list`, `buffer-next`, `buffer-prev`
- Flow: Type → UNIQUE → ENTER → SELECTED → execute

### Commands with free-form arguments
- `find` - needs search pattern
- `replace` - needs find/replace patterns
- `goto-line` - needs line number
- `save`, `load` - need filename
- Flow: Type → UNIQUE → ENTER → SELECTED → app prompts for argument separately
- Note: Completer only handles command selection, not argument input

### Commands with child completers
- `utf-box` → horizontal, vertical, upper-left, upper-right, etc.
- `utf-symbol` → arrows, math symbols, etc.
- Flow: Type → NEXT_LEVEL → transition to child → type → UNIQUE → ENTER → SELECTED → insert symbol

### Ambiguous command groups
- `buffer-*` (list, next, prev)
- `save` vs `save-as`
- `replace` vs `replace-all`
- Flow: Type → PARTIAL → more chars → UNIQUE

## Matching Features Used

### Dehyphenated prefix matching
- `bufferlist` → `buffer-list` (hyphens ignored during matching)
- `gotoline` → `goto-line`
- `b` → matches `buffer-list`, `buffer-next`, etc.

## Status Handling

| Status | Meaning | App Response |
|--------|---------|--------------|
| COMPLETER_NO_MATCH | Nothing matches | Show error, let user retry |
| COMPLETER_MULTIPLE | Multiple matches, no auto-complete | Show hints |
| COMPLETER_PARTIAL | Auto-completed to common prefix | Show hints, wait for more input |
| COMPLETER_UNIQUE | Single match, ready | Wait for ENTER |
| COMPLETER_NEXT_LEVEL | Single match with child | Transition to child completer |
| COMPLETER_SELECTED | Confirmed selection | Execute command or get argument |

## Integration Flow

```
ESC pressed → enter command mode
    ↓
show "command> " prompt
    ↓
Completer *current = &commands;
CxString input = "";
    ↓
loop on keypress:
    ↓
    ENTER → processEnter()
        SELECTED → execute or prompt for args
        NO_MATCH → show error
        MULTIPLE → show "ambiguous"
    ↓
    ESC → cancel, return to editor
    ↓
    char → processChar()
        update display with getInput()
        NEXT_LEVEL → current = getNextLevel(), input = ""
        MULTIPLE/PARTIAL → show hints
        UNIQUE → wait for ENTER
        NO_MATCH → show error or beep
```

## Commands to Register

```cpp
// Simple commands
commands.addCandidate("quit", NULL, &quitHandler);
commands.addCandidate("help", NULL, &helpHandler);
commands.addCandidate("buffer-list", NULL, &bufferListHandler);
commands.addCandidate("buffer-next", NULL, &bufferNextHandler);
commands.addCandidate("buffer-prev", NULL, &bufferPrevHandler);

// Commands with arguments (app handles arg collection after selection)
commands.addCandidate("find", NULL, &findHandler);
commands.addCandidate("replace", NULL, &replaceHandler);
commands.addCandidate("replace-all", NULL, &replaceAllHandler);
commands.addCandidate("goto-line", NULL, &gotoLineHandler);
commands.addCandidate("save", NULL, &saveHandler);
commands.addCandidate("save-as", NULL, &saveAsHandler);
commands.addCandidate("load", NULL, &loadHandler);

// Commands with child completers
commands.addCandidate("utf-box", &boxSymbols, NULL);
commands.addCandidate("utf-symbol", &symbols, NULL);
```

## Child Completer Setup

```cpp
// Box drawing symbols
Completer boxSymbols;
boxSymbols.addCandidate("horizontal", NULL, (void*)"─");
boxSymbols.addCandidate("vertical", NULL, (void*)"│");
boxSymbols.addCandidate("upper-left", NULL, (void*)"┌");
boxSymbols.addCandidate("upper-right", NULL, (void*)"┐");
boxSymbols.addCandidate("lower-left", NULL, (void*)"└");
boxSymbols.addCandidate("lower-right", NULL, (void*)"┘");
// ... etc
```

## What Completer Does NOT Handle

- Argument input (filenames, patterns, line numbers)
- Backspace/delete (app manipulates input string, passes to Completer)
- ESC to cancel (app handles)
- Display/rendering (app calls getInput() and findMatches())
