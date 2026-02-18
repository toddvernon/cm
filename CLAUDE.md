# cmacs Project Instructions

## Overview
cmacs is a uEmacs-like terminal text editor written in C++.

## Naming Conventions
When the user refers to:
- **"cx"** - means all cx repositories together: `cx/cx` (library), `cx/cx_tests` (tests), and `cx/cx_apps/cm` (this app)
- **"cm"** - means this application (`cx/cx_apps/cm`)
- **"cx library"** or **"cx/cx"** - means specifically the shared library at `cx/cx`
- **"cx_tests"** or **"cx tests"** - means the test repository at `cx/cx_tests`

## Related Repositories
- **cx library**: `../../../cx/cx/` - shared library with modules: base, commandline, editbuffer, expression, functor, json, keyboard, log, net, screen, thread, tz

## Change Policy
- **Always show proposed changes before applying them** - describe what will be modified and wait for approval before editing files
- After approval, verify changes compile successfully by running `make`

## Do Not Modify
- `darwin_arm64/` - ARM64 build output
- `darwin_x86_64/` - x86_64 build output
- `linux_x86_64/` - Linux build output

## Build Instructions
```bash
make
```

## Creating Archives
Always use the make target to create distribution archives - never run tar manually:
```bash
make archive    # creates ../../ARCHIVE/cxapps_unix.tar
```
The make target excludes build outputs and other cruft that should not be distributed.

## Project Structure
- `Cm.cpp` - main entry point
- `ScreenEditor.*` - core editor logic (includes ScreenEditorCore.cpp, ScreenEditorCommands.cpp)
- `EditView.*` - text editing view (includes EditViewDisplay.cpp, EditViewInput.cpp)
- `CommandLineView.*` - command line interface
- `CommandTable.*` - static ESC command table (data only)
- `FileListView.*` - file browser
- `HelpView.*` - help display
- `BuildView.*` - build output display
- `ProjectView.*` - project/buffer list view
- `MarkUp.*` - syntax highlighting (includes MarkUpColorizers.cpp, MarkUpParsing.cpp)
- `ProgramDefaults.*` - configuration handling
- `Project.*` - project management
- `UTFSymbols.*` - UTF-8 symbol table for box drawing and common symbols

## Screen Resize Architecture

**ScreenEditor owns the single resize callback.** Sub-elements (EditView, CommandLineView, FileListView, HelpView) do NOT register their own callbacks with the OS.

- ScreenEditor registers ONE callback via `screen->addScreenSizeCallback()`
- On resize, ScreenEditor coordinates ALL recalcs first, THEN all redraws in correct z-order
- Sub-elements expose public `recalcScreenPlacements()` or `recalcForResize()` methods for ScreenEditor to call
- Each visual element (e.g., EditView) owns its own sub-elements (e.g., status bar) - no separate drawing by parent

**NEVER** add resize callbacks to sub-elements. All resize coordination goes through ScreenEditor.

## Makefile guidelines
- When modifying makefiles, follow the patterns already present in the existing makefiles.
- Makefiles must be portable across old make implementations (SunOS, IRIX, BSD). Only use features and automatic variables already present in the existing makefiles. Do not introduce GNU make extensions such as `$(filter ...)`, `$(wildcard ...)`, `$(patsubst ...)`, etc.

## Non-negotiable constraints
- DO NOT use the C++ Standard Library: no `std::` anywhere.
- DO NOT introduce templates, including template-based third-party libs.
- DO NOT add new external dependencies.
- Only use libraries that already exist in this repository under: ../lib with the source in ../cx directory
- If a feature would normally use STL/RAII/modern C++, implement it using existing in-repo utilities.

## Portability targets
- Code must compile on: macOS (darwin), Linux, SunOS/Solaris, IRIX.
- Assume older compilers. Avoid modern language features (C++11+), unless the codebase already uses them everywhere.

## Platform classification

For features with platform-specific behavior (e.g., UI responsiveness, I/O intensity):

- **Modern platforms**: macOS (`_OSX_`), Linux (`_LINUX_`)
  - Can afford more screen I/O, visual effects, real-time updates
- **Vintage platforms**: Everything else (`_SUNOS_`, `_SOLARIS6_`, `_SOLARIS10_`, `_IRIX6_`, `_NETBSD_`, `_NEXT_`)
  - Minimize screen I/O, avoid unnecessary redraws, batch updates

Use this pattern for platform-specific code:
```c
#if defined(_OSX_) || defined(_LINUX_)
    // Modern: richer UI behavior
#else
    // Vintage: minimal I/O
#endif
```

## Language/feature restrictions (assume old toolchains)
- Avoid: auto, nullptr, constexpr, lambda, range-for, threads, regex, exceptions (unless already widely used).
- Avoid: <iostream>, <string>, <vector>, <map>, <memory>, <algorithm>, <functional>, <type_traits>, etc.
- DO NOT define classes inside function bodies (local/inner classes). GCC 2.95 crashes generating DWARF2 debug info for local class methods. Always define classes at file scope.
- Prefer C-style interfaces or existing repo abstractions.

## Includes & headers
- Prefer existing project headers and utilities.
- Use C headers where appropriate: <stdio.h>, <stdlib.h>, <string.h>, <unistd.h> (guarded), etc.
- Minimize OS-specific includes; isolate with `#ifdef` blocks and use only existing defines for platform

## OS-specific code
- Any OS-specific code MUST be isolated behind preprocessor guards.
- Use existing platform abstraction layers if present.
- Never add a new platform directory structure without explicit instruction.

## Code changes workflow
- Make the smallest change that solves the problem.
- Do not reformat unrelated code.
- Do not rename symbols/files unless asked.
- Add comments where portability is non-obvious.

## Output expectation
- When proposing a change, briefly state:
  1) which files you changed,
  2) why it is portable across targets,
  3) what repo-local libraries you used.

## Completer Integration Rules

The `Completer` library (`cx/commandcompleter`) handles command selection via a status-driven loop. When integrating or modifying Completer usage:

- **NEVER hardcode knowledge of the command table into the input loop.** The loop must be blind to what commands exist, how many characters it takes to match, or what the outcome will be. It reacts to `getStatus()` and nothing else.
- **NEVER assume a specific number of processChar calls will reach a result.** Commands may be added, renamed, or restructured at any time.
- **The only correct pattern is a loop that reacts to status:** processChar/processTab/processEnter → check getStatus() → respond accordingly.
- **Only the addCandidate() setup knows the command structure.** The input loop does not.

## FORBIDDEN: Fuzzy / Dehyphenated Matching

**DO NOT implement, suggest, or reference any of the following:**
- Dehyphenated matching (removing hyphens before comparing)
- Fuzzy matching, abbreviated matching, or command shortening
- Matching "gl" to "goto-line", "sa" to "file-save-as", etc.
- A `dehyphenate()` function or any hyphen-stripping logic

**All matching in this project is LITERAL PREFIX matching.** The user's input must be an exact prefix of the candidate name, including hyphens. For example:
- "goto" matches "goto-line" (literal prefix) - CORRECT
- "gl" matches "goto-line" (dehyphenated) - WRONG, DO NOT DO THIS
- "file" matches "file-save" (literal prefix) - CORRECT
- "fs" matches "file-save" (dehyphenated) - WRONG, DO NOT DO THIS

This applies to the Completer library, CommandTable, UTFSymbols, and all code in this project.

See `COMPLETER_INTEGRATION.md` for completer flows and the integration pattern.
See `cx_tests/cxcommandcompleter/cxcommandcompleter_example.cpp` for working code.

## ESC Command System

### Architecture
- `CommandTable.h/cpp` - static command table (data only, no matching logic)
- `Completer` library (`cx/commandcompleter`) - handles all prefix matching/completion
- Child completers for UTF symbol selection (`insert-box`, `insert-symbol`)
- `ScreenEditor.h/cpp` - command input state machine uses Completer status-driven API

### How It Works
- ESC → "command> " prompt shows category prefixes (file-, edit-, search-, etc.)
- Type a letter to narrow to a category, TAB completes, ENTER executes
- Symbol commands (`insert-box`, `insert-symbol`) use child completers for two-level selection
- Freeform arg commands (`search-text`, `goto-line`, etc.) transition to argument input mode
- No-arg commands (`edit-mark`, `edit-cut`, etc.) execute directly on ENTER

### Command Categories (menu-bar order)
Each category is unique at the first keystroke:
```
f = file-       load, new, quit, save, save-as
e = edit-       cut, mark, paste, system-paste
s = search-     text, replace, replace-all
g = goto-       error, line
i = insert-     box, comment-block, symbol
t = text-       count, detab, entab, trim-trailing
v = view-       build, help, split, unsplit
p = project     (standalone, opens project dialog)
```

### CTRL Key Bindings
```
C-b   Show build output          C-p   Project/buffer list
C-f   Find again                 C-r   Replace again
C-h   Show help                  C-s   Split screen
C-j   Toggle jump scroll         C-u   Unsplit screen
C-k   Cut to end of line         C-v   Page down
C-l   Toggle line numbers        C-w   Cut mark to cursor
C-n   Next buffer                C-y   Paste
C-o   Switch split view          C-z   Page up
```

### CTRL-X Chord
```
C-x C-s   Save current buffer
C-x C-c   Quit editor
```

### Lazy Buffer Creation (no-file startup)

When cm starts with no filename argument, there is NO edit buffer until an
operation requires one. Every command that touches the buffer must check for
NULL (`activeEditView()->getEditBuffer()` or `editBufferList->current()`)
before dereferencing. The rules:

- **Read-only commands on empty buffer = report success.** If the command
  would have succeeded on an empty buffer, report the natural success message.
  Examples: `text-entab` → "(entab complete)", `text-count` → "(0 lines, 0
  characters)", `text-trim-trailing` → "(0 trailing characters removed)".

- **Commands that insert or write = create the buffer.** Any action where
  the user is showing intent to use the buffer (inserting text, saving a file,
  pasting) should create the buffer on demand, add it to `editBufferList`,
  set it in `activeEditView()`, then proceed normally.

- **Ambiguous cases must be surfaced during development.** If a command on
  a NULL buffer would not clearly succeed or clearly need a buffer, that is
  an ambiguity that needs to be discussed and resolved — do not silently
  guess.

### Key Files
- `CommandTable.h/cpp` - command definitions (data-driven, no matching logic)
- `ScreenEditor.h/cpp` - Completer members, status-driven input loop
- `ScreenEditorCore.cpp` - CTRL dispatch table, file loading
- `ScreenEditorCommands.cpp` - all CMD_*, CTRL_*, CONTROL_* handlers
- `UTFSymbols.h/cpp` - symbol table (matching uses Completer static methods)
- `makefile` - links `libcx_commandcompleter.a`
