# cmacs Help

## Overview

cmacs is a terminal text editor. Unlike vim, cmacs is always in insert
mode - just start typing. Use ESC to enter command mode, Ctrl keys for
quick actions.

## ESC Commands

Press ESC to open the command prompt. Then just start typing.

How it works:
  1. Each keystroke narrows the matches. The shared prefix fills in automatically.
  2. When your input uniquely identifies a command, it executes immediately.
  3. Invalid keystrokes are rejected - you cannot type a wrong path.

Example: to save a file, type ESC then f then s. After 'f', the prompt
fills "file-". After 's', "file-save" matches, so press Enter to execute
(or keep typing for file-save-as). Three keystrokes total.

Commands are organized by category:

file-
  file-load <filename>      Load file into new buffer
  file-new <filename>       Create new buffer
  file-quit                 Quit (refuses if unsaved changes)
  file-save [filename]      Save current buffer
  file-save-as <filename>   Save buffer to new file

edit-
  edit-copy                 Copy from mark to cursor (+ clipboard)
  edit-cut                  Cut from mark to cursor (+ clipboard)
  edit-mark                 Set mark at cursor position
  edit-paste                Paste from cut buffer
  edit-system-paste         Paste from system clipboard

search-
  search-text <pattern>     Search for text in buffer
  search-replace <text>     Replace next occurrence
  search-replace-all <text> Replace all occurrences

goto-
  goto-error                Jump to file:line from error under cursor
  goto-line <line>          Go to specified line number

insert-
  insert-box <symbol>       Insert box drawing symbol (TAB shows options)
  insert-comment-block <col> Insert comment block to column
  insert-symbol <symbol>    Insert common symbol (TAB shows options)

text-
  text-count                Count lines and characters in buffer
  text-detab                Convert tabs to spaces
  text-entab                Convert leading spaces to tabs
  text-trim-trailing        Remove trailing whitespace from all lines

view-
  view-build                Show build output
  view-help                 Show help screen
  view-jump-scroll          Toggle jump scroll mode
  view-split                Split screen horizontally
  view-unsplit              Return to single view

project                     Open project dialog (same as Ctrl-P)

quit-
  quit-save                 Save modified files and quit
  quit-nosave               Quit without saving


## Control Key Shortcuts

These work directly without entering command mode:

  Ctrl-B        Show build output
  Ctrl-F        Find again (repeat last search)
  Ctrl-H        Show this help
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
  Ctrl-X Ctrl-C   Quit without saving


## Mouse Support

On macOS and Linux terminals with mouse support:

  Click             Place cursor at click position
  Click+Drag        Select text (highlighted in blue)
  Shift+Click       Extend selection from mark
  Double-click      Select word under cursor
  Scroll wheel      Scroll up/down (3 lines)

  Split screen:     Click activates the target view
  ESC command:      Click hints to select commands
  Project view:     Click selects item, double-click opens
  Help/Build:       Scroll wheel navigates, click outside dismisses


## Navigation

  Arrow keys      Move cursor
  Home / End      Beginning / end of line
  Page Up/Down    Scroll by page
  Ctrl-A          Beginning of line
  Ctrl-E          End of line


## Tips

- ESC commands auto-complete and auto-execute - most take 2-3 keystrokes
- Ctrl-W copies to system clipboard, Ctrl-Y pastes from internal buffer
- Use edit-system-paste for system clipboard paste
- Mouse drag-select also copies to system clipboard (use edit-copy for keyboard)
- Split screen with Ctrl-S, switch views with Ctrl-O, unsplit with Ctrl-U
- In project view: S saves, A saves all, M builds, click or Enter to open
