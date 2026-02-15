# CMacs

Zero dependencies. Syntax highlighting. Project builds. The grown-up nano—running on macOS, Linux, and your 1993 SPARCstation or IRIX Indigo.

## Why CMacs?

If you've outgrown nano but don't want to climb the vim or emacs learning curve, CMacs sits in the middle ground. Like nano, you're always in insert mode—just open a file and start typing. But where nano stops at basic editing, CMacs adds:

- **Tab-completed commands** — Press ESC and type. Don't memorize shortcuts; discover them as you type.
- **Project files** — Define your project once, jump between source files with a keystroke, kick off builds from the editor.
- **Split screens** — View two files side by side.
- **Syntax highlighting** — For C, C++, Python, Markdown, and more.

The real story is portability. CMacs is written in C++ with zero external dependencies—no Boost, no STL, no autoconf, no cmake, no package managers pulling in half the internet. Just `make`. It builds on modern macOS and Linux, but also on SGI IRIX, Solaris, and SunOS machines from the early 1990s. That combination—C++, no dependencies, runs on 30-year-old hardware—is rare.

On vintage systems, you'll need a GCC from a later era (cross-compiled or bootstrapped), but once you have that, CMacs builds clean.

On modern platforms, CMacs takes advantage of what's available: full UTF-8 support, RGB true-color syntax highlighting, fast differential screen updates, and—with the developer build—Claude MCP integration that lets Claude read your buffers and help write your code.

## Design Philosophy

CMacs is built for portability without compromise. The codebase avoids the C++ Standard Library entirely, relying instead on a self-contained support library (cx) that compiles everywhere—from modern macOS and Linux to vintage IRIX 6.5 and Solaris workstations.

But portability doesn't mean lowest common denominator. On modern terminals, CMacs lights up: 24-bit RGB color for syntax themes, Unicode box-drawing and symbols, smooth screen updates that don't flicker. The same binary that runs on your M3 Mac will build on an SGI Octane—it just adapts to what the platform offers.

## Building

CMacs builds with a simple makefile. No autoconf, no cmake, no package managers.

```bash
make
```

To install to `/usr/local/bin`:

```bash
make install
```

To install the help file to `/usr/local/share/cm`:

```bash
make install-help
```

Or install everything at once:

```bash
make install-all
```

### Supported Platforms

- macOS (ARM64 and x86_64)
- Linux (x86_64)
- Solaris / SunOS
- IRIX 6.5
- NetBSD

### Dependencies

None beyond a C++ compiler and standard POSIX headers. The cx library (included in the parent repository) provides all string handling, file I/O, screen management, and keyboard input.

## Usage

```bash
cm [filename]
cm myproject.project
```

CMacs is always in insert mode—just start typing. Unlike vim, there's no mode switching for basic editing.

## Key Bindings

CMacs takes inspiration from Emacs but diverges in two significant ways:

**1. Tab-Completed Commands**

Press `ESC` to enter command mode. Commands are organized by category and support incremental tab completion:

```
ESC f<TAB>     →  file-     (shows file commands)
ESC file-s<TAB> →  file-save
```

Categories: `file-`, `edit-`, `search-`, `goto-`, `insert-`, `text-`, `view-`, `project`

**2. Direct CTRL Bindings**

Common operations have single CTRL key bindings instead of chords:

| Key | Action |
|-----|--------|
| C-h | Help |
| C-p | Project/buffer list |
| C-n | Next buffer |
| C-s | Split screen |
| C-f | Find again |
| C-y | Paste |
| C-w | Cut (mark to cursor) |
| C-k | Cut to end of line |

The traditional Emacs `C-x` chord prefix is supported for save (`C-x C-s`) and quit (`C-x C-c`).

## Help

Press `C-h` to view the built-in help, or see `cm_help.md` for the full reference.

## Configuration

CMacs reads configuration from `.cmrc` (current directory first, then home directory). Settings include tab width, line numbers, jump scroll, and syntax highlighting colors.

## License

Apache License 2.0. See LICENSE file.
