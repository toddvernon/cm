# CMacs Help

## Overview
CMacs is a uEmacs-inspired terminal text editor written in C++.
Unlike vim, CMacs is always in insert mode - just start typing.
Use ESC to enter command mode, Ctrl keys for quick actions.

  ┌─────────┬─────────────────────┬──────────────────────────────┬────────────────────────────────────────────┐
  │ Binding │       cm does       │         VS Code does         │                 Collision?                 │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-b     │ Show build output   │ Toggle sidebar               │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-f     │ Find again          │ Open find                    │ Partial -- same domain, different behavior │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-h     │ Show help           │ Find and replace             │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-j     │ Toggle jump scroll  │ Toggle panel                 │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-k     │ Cut to end of line  │ Chord prefix (C-k C-c, etc.) │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-l     │ Toggle line numbers │ Go to line                   │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-n     │ Next buffer         │ New file                     │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-o     │ Switch split view   │ Open file                    │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-p     │ Project list        │ Quick open / command palette │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-r     │ Replace again       │ Open recent                  │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-s     │ Split screen        │ Save                         │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-u     │ Unsplit screen      │ Undo cursor                  │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-v     │ Page down           │ Paste                        │ Yes -- dangerous                           │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-w     │ Cut mark to cursor  │ Close tab                    │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-y     │ Paste               │ Redo                         │ Yes                                        │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-z     │ Page up             │ Undo                         │ Yes -- dangerous                           │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-x C-s │ Save                │ C-s = Save                   │ Same intent, extra keystroke               │
  ├─────────┼─────────────────────┼──────────────────────────────┼────────────────────────────────────────────┤
  │ C-x C-c │ Quit                │ C-q = Quit                   │ Same intent, different key                 │
  └─────────┴─────────────────────┴──────────────────────────────┴────────────────────────────────────────────┘


## ESC Commands
Press ESC to enter command mode, then type a command name.
Commands follow a category-action naming convention. Each category
is unique at the first keystroke, so typing one letter narrows to
that category. TAB completes, ENTER executes.

### file-
  file-load <filename>      Load file into new buffer
  file-new <filename>       Create new buffer
  file-quit                 Quit editor
  file-save [filename]      Save current buffer
  file-save-as <filename>   Save buffer to new file

### edit-
  edit-cut                  Cut from mark to cursor
  edit-mark                 Set mark at cursor position
  edit-paste                Paste from cut buffer
  edit-system-paste         Paste from system clipboard

### search-
  search-text <pattern>     Search for text in buffer
  search-replace <text>     Replace next occurrence
  search-replace-all <text> Replace all occurrences

### goto-
  goto-error                Jump to file:line from error under cursor
  goto-line <line>          Go to specified line number

### insert-
  insert-box <symbol>       Insert box drawing symbol (TAB for completion)
  insert-comment-block <col> Insert comment block to column
  insert-symbol <symbol>    Insert common symbol (TAB for completion)

### text-
  text-count                Count lines and characters in buffer
  text-detab                Convert tabs to spaces
  text-entab                Convert leading spaces to tabs
  text-trim-trailing        Remove trailing whitespace from all lines

### view-
  view-build                Show build output
  view-help                 Show help screen
  view-split                Split screen horizontally
  view-unsplit              Return to single view

### project
  project                   Open project dialog (same as Ctrl-P)


## Control Key Shortcuts
These work directly without entering command mode:

  Ctrl-B        Show build output
  Ctrl-F        Find again (repeat last search)
  Ctrl-H        Show this help
  Ctrl-J        Toggle jump scroll
  Ctrl-K        Cut to end of line
  Ctrl-L        Toggle line numbers
  Ctrl-N        Next buffer
  Ctrl-O        Switch view (when split)
  Ctrl-P        Project/buffer list
  Ctrl-R        Replace again (repeat last replace)
  Ctrl-S        Split screen
  Ctrl-U        Unsplit screen
  Ctrl-V        Page down
  Ctrl-W        Cut mark to cursor (+ system clipboard)
  Ctrl-Y        Paste (internal buffer)
  Ctrl-Z        Page up


## Control-X Commands
Two-key sequences starting with Ctrl-X:

  Ctrl-X Ctrl-S   Save current buffer
  Ctrl-X Ctrl-C   Quit editor


## Navigation
  Arrow keys      Move cursor
  Home / End      Beginning / end of line
  Page Up/Down    Scroll by page
  Ctrl-A          Beginning of line
  Ctrl-E          End of line


## Command Structure

  CTRL Keys
  ├── C-b ─────── Show build output
  ├── C-f ─────── Find again (repeat last search)
  ├── C-h ─────── Show help
  ├── C-j ─────── Toggle jump scroll
  ├── C-k ─────── Cut to end of line
  ├── C-l ─────── Toggle line numbers
  ├── C-n ─────── Next buffer
  ├── C-o ─────── Switch split view
  ├── C-p ─────── Project/buffer list
  ├── C-r ─────── Replace again
  ├── C-s ─────── Split screen
  ├── C-u ─────── Unsplit screen
  ├── C-v ─────── Page down
  ├── C-w ─────── Cut mark to cursor (+ system clipboard)
  ├── C-y ─────── Paste (internal buffer)
  ├── C-z ─────── Page up
  │
  └── C-x (chord prefix)
      ├── C-x C-s ── Save
      └── C-x C-c ── Quit

  ESC Commands (command> prompt, TAB completes, ENTER executes)
  │
  ├── f ── file-
  │        ├── file-load <filename> ──── Load file into new buffer
  │        ├── file-new <filename> ───── Create new buffer
  │        ├── file-quit ────────────── Quit editor
  │        ├── file-save [filename] ─── Save current buffer
  │        └── file-save-as <filename>  Save buffer to new file
  │
  ├── e ── edit-
  │        ├── edit-cut ────────────── Cut from mark to cursor
  │        ├── edit-mark ───────────── Set mark at cursor
  │        ├── edit-paste ──────────── Paste from cut buffer
  │        └── edit-system-paste ───── Paste from system clipboard
  │
  ├── s ── search-
  │        ├── search-text <pattern> ── Search for text
  │        ├── search-replace <text> ── Replace next occurrence
  │        └── search-replace-all ───── Replace all occurrences
  │
  ├── g ── goto-
  │        ├── goto-error ──────────── Jump to error location
  │        └── goto-line <line> ────── Go to line number
  │
  ├── i ── insert-
  │        ├── insert-box <symbol> ──── Insert box drawing character
  │        ├── insert-comment-block ─── Insert comment block
  │        └── insert-symbol <symbol> ─ Insert common symbol
  │
  ├── t ── text-
  │        ├── text-count ──────────── Count lines and characters
  │        ├── text-detab ──────────── Convert tabs to spaces
  │        ├── text-entab ──────────── Convert spaces to tabs
  │        └── text-trim-trailing ──── Remove trailing whitespace
  │
  ├── v ── view-
  │        ├── view-build ──────────── Show build output
  │        ├── view-help ───────────── Show help screen
  │        ├── view-split ──────────── Split screen
  │        └── view-unsplit ────────── Unsplit screen
  │
  └── p ── project ─────────────────── Open project dialog


