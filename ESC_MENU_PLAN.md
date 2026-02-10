# Plan: Restructure ESC commands into category-action menu system

## Context

The ESC command system currently has inconsistent naming: some commands use `category-action` (project-make, buffer-next), some are bare verbs (find, save, cut), and some no-arg commands duplicate CTRL bindings without clear organization. The goal is to restructure all commands into a consistent `category-action` pattern that works like a hidden menu system — every command has a category prefix, and each category is unique at the first keystroke.

Additionally, `buffer-next` and `buffer-prev` are obsoleted by `project-show` (C-p) and should be removed.

## New command structure

Display order follows traditional menu bar precedence (File, Edit, Search, ...) with editor-specific categories after:

```
ESC → "command> "
│
│  (Traditional menu order)
│
├── f ── file-
│        ├── load <filename>        (was: load)
│        ├── new <filename>         (was: buffer-new)
│        ├── quit                   (was: quit)
│        ├── save [filename]        (was: save)
│        └── save-as <filename>     (was: save-as)
│
├── e ── edit-
│        ├── cut                    (was: cut)
│        ├── mark                   (was: mark)
│        ├── paste                  (was: paste)
│        └── system-paste           (was: system-paste)
│
├── s ── search-
│        ├── text <pattern>         (was: find)
│        ├── replace <replacement>  (was: replace)
│        └── replace-all <repl>     (was: replace-all)
│
├── g ── goto-
│        ├── error                  (was: goto-error)
│        └── line <line>            (was: goto-line)
│
├── i ── insert-
│        ├── box <symbol>           (was: utf-box)
│        ├── comment-block <col>    (was: comment-block)
│        └── symbol <symbol>        (was: utf-symbol)
│
├── t ── text-
│        ├── count                  (was: wc)
│        ├── detab                  (was: detab)
│        ├── entab                  (was: entab)
│        └── trim-trailing          (was: trim-trailing)
│
├── v ── view-
│        ├── build                  (was: show-build)
│        ├── help                   (was: help)
│        ├── split                  (was: split)
│        └── unsplit                (was: unsplit)
│
└── p ── project                    (was: project-show, = C-p)
```

**Removed commands:**
- `buffer-next` — superseded by project-show (C-p)
- `buffer-prev` — superseded by project-show (C-p)

**Removed project subcommands** (absorbed into project dialog):
- `project-clean` — use project dialog
- `project-create` — use project dialog
- `project-edit` — use project dialog
- `project-make` — use project dialog

## Why only CommandTable.cpp changes

The architecture is fully data-driven:
- Completer is populated dynamically from `commandTable[]` via `addCandidate()`
- Command input state machine reacts to Completer status, not command names
- Display renders from Completer match results, not hardcoded strings
- Handler method names (CMD_Find, CMD_SaveFile) are C++ identifiers, independent of command names

## Changes

### 1. `cm/CommandTable.cpp` — rewrite command table

Replace the entire `commandTable[]` array with the new category-action structure. Order entries by category in menu-bar order (file, edit, search, goto, insert, text, view, project).

### 2. `cm/ScreenEditor.cpp` — remove subproject completer setup

The `rebuildSubprojectCompleter()` method and `_subprojectCompleter` member were used for `project-make`/`project-clean` argument completion. Since those commands are removed (absorbed into project dialog), this code can be removed.

In `handleArgumentModeInput()`, remove the special-case TAB handling for project-make/project-clean subproject completion.

### 3. `cm/ScreenEditor.h` — remove subproject completer member

Remove `Completer _subprojectCompleter;` member declaration and `rebuildSubprojectCompleter()` method declaration.

### 4. `cm/ScreenEditorCommands.cpp` — remove obsolete handlers

Remove these handler methods:
- `CMD_BufferNext` — removed command
- `CMD_BufferPrev` — removed command
- `CMD_ProjectClean` — absorbed into dialog
- `CMD_ProjectCreate` — absorbed into dialog
- `CMD_ProjectEdit` — absorbed into dialog
- `CMD_ProjectMake` — absorbed into dialog

Keep all remaining handlers — they're referenced by the new command names via function pointers.

### 5. `cm/ScreenEditor.h` — remove obsolete handler declarations

Remove declarations for the 6 handlers listed above.

## What stays unchanged

- All handler implementations that are still referenced (CMD_Find, CMD_SaveFile, CMD_Replace, etc.)
- Handler C++ method names don't change (CMD_Find serves "search-text", etc.)
- Completer library — no changes
- CommandTable.h — struct and flags unchanged
- CommandLineView — display is data-driven
- EditView — unaffected
- UTFSymbols.h/cpp — symbol names keep "box-"/"sym-" prefixes, child completers unchanged
- `symbolFilter` values stay "box-" and "sym-" (they match UTFSymbol names, not command names)
- All CTRL bindings unchanged

## Display order rationale

Traditional menu bar order: **File, Edit, Search, Goto, Insert, Text, View, Project**

- `file-` first by convention (users expect it)
- `edit-` second (core editing, high frequency)
- `search-` third (very frequent in editors)
- `goto-` fourth (navigation)
- `insert-` fifth (symbols, comment blocks)
- `text-` sixth (formatting transforms)
- `view-` seventh (split, build, help)
- `project` last (standalone command, opens dialog)

## Verification

1. `make` in cm — should compile with only CommandTable.cpp recompiled
2. Test each category:
   - ESC → `f` TAB → see all file- commands
   - ESC → `e` TAB → see all edit- commands
   - ESC → `s` TAB → see all search- commands
   - etc.
3. Test commands with arguments: `search-text`, `goto-line`, `file-save-as`
4. Test symbol commands: `insert-box`, `insert-symbol` with child completers
5. Test `project` standalone command opens dialog
6. Verify all CTRL bindings still work unchanged
7. Verify removed commands (`buffer-next`, `project-make`, etc.) no longer appear
