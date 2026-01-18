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
