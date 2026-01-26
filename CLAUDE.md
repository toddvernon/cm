# CMacs Project Instructions

## Overview
CMacs is a uEmacs-like terminal text editor written in C++.

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

## Project Structure
- `Cm.cpp` - main entry point
- `ScreenEditor.*` - core editor logic
- `EditView.*` - text editing view
- `CommandLineView.*` - command line interface
- `FileListView.*` - file browser
- `HelpTextView.*` - help display
- `MarkUp.*` - syntax highlighting
- `ProgramDefaults.*` - configuration handling
- `Project.*` - project management

## Non-negotiable constraints
- DO NOT use the C++ Standard Library: no `std::` anywhere.
- DO NOT introduce templates, including template-based third-party libs.
- DO NOT add new external dependencies.
- Only use libraries that already exist in this repository under: ../lib with the source in ../cx directory
- If a feature would normally use STL/RAII/modern C++, implement it using existing in-repo utilities.

## Portability targets
- Code must compile on: macOS (darwin), Linux, SunOS/Solaris, IRIX.
- Assume older compilers. Avoid modern language features (C++11+), unless the codebase already uses them everywhere.

## Language/feature restrictions (assume old toolchains)
- Avoid: auto, nullptr, constexpr, lambda, range-for, threads, regex, exceptions (unless already widely used).
- Avoid: <iostream>, <string>, <vector>, <map>, <memory>, <algorithm>, <functional>, <type_traits>, etc.
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

## Current Work (2026-01-25)

**New ESC command system with tab completion - NOT YET COMMITTED**

### What's Done
- Created `CommandRegistry.h/cpp` - command table with fuzzy prefix matching
- Modified `ScreenEditor.h/cpp` - command input state machine, tab completion
- ESC → "command> " prompt → type partial command → TAB completes → ENTER executes
- `goto-line` command tested and working via new system

### What Needs Testing
- `find` command via new system (ESC → "fi" → TAB → pattern → ENTER)
- `replace` command
- `save`, `load`, `mark`, `cut`, `paste` commands

### Commands with NULL handlers (show "not implemented")
- `find-all`, `buffer-next`, `buffer-prev`, `buffer-list`, `quit`, `help`

### Files changed (uncommitted)
- `CommandRegistry.h` (new)
- `CommandRegistry.cpp` (new)
- `ScreenEditor.h`
- `ScreenEditor.cpp`
- `Makefile`
- `Cm.cpp` (terminal cleanup on exit)

## ACTIVE DEBUGGING: Intermittent Startup Crash

**Status**: Debug logging enabled, signal blocking disabled to reproduce crash

### The Problem
- Intermittent crash (~20% of the time) at startup after "(Loaded ...)" message
- "trace trap" error, doesn't reproduce in debugger
- Was "fixed" by blocking SIGWINCH during construction, but root cause unknown

### Current Debug Setup (ScreenEditor.cpp constructor)
- Writes to `/tmp/cm_debug.log` with numbered steps (1-20)
- Each step is fflush'd immediately
- SIGWINCH blocking is COMMENTED OUT to try to reproduce crash
- When crash occurs, check `cat /tmp/cm_debug.log` for last successful step

### To Re-enable the Fix
In `ScreenEditor.cpp` constructor, uncomment:
```cpp
sigset_t blockSet, oldSet;
sigemptyset(&blockSet);
sigaddset(&blockSet, SIGWINCH);
sigprocmask(SIG_BLOCK, &blockSet, &oldSet);
```
And at end of constructor:
```cpp
sigprocmask(SIG_SETMASK, &oldSet, NULL);
```

### To Remove Debug Logging
Remove all `if (dbg) { fprintf(...); fflush(dbg); }` lines and the `FILE *dbg = fopen(...)` line
