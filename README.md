# CMacs

Zero dependencies. Syntax highlighting. Project builds. The grown-up nano—running on macOS, Linux, and your 1993 SPARCstation or IRIX Indigo.

## Why CMacs?

If you've outgrown nano but don't want to climb the vim or emacs learning curve, CMacs sits in the middle ground. Like nano, you're always in insert mode—just open a file and start typing. But where nano stops at basic editing, CMacs adds:

- **Tab-completed commands** — Press ESC and type. Don't memorize shortcuts; discover them as you type.
- **Project files** — Define your project once, jump between source files with a keystroke, kick off builds from the editor.
- **Split screens** — View two files side by side.
- **Syntax highlighting** — For C, C++, Python, Markdown, and more.

The real story is portability. CMacs is written in C++ with zero external dependencies—no Boost, no STL, no autoconf, no cmake, no package managers pulling in half the internet. Just `make`. It builds on modern macOS and Linux, but also on SGI IRIX, Solaris, and SunOS machines from the early 1990s. That combination—C++, no dependencies, runs on 30-year-old hardware—is rare.

On vintage systems, you'll need GNU Make and GCC 2.8.1 or later (cross-compiled or bootstrapped), but once you have those, CMacs builds clean.

On modern platforms, CMacs takes advantage of what's available: full UTF-8 support, RGB true-color syntax highlighting, fast differential screen updates, and—with the developer build—Claude MCP integration that lets Claude read your buffers and help write your code.

## Design Philosophy

CMacs is built for portability without compromise. The codebase avoids the C++ Standard Library entirely, relying instead on a self-contained support library (cx) that compiles everywhere—from modern macOS and Linux to vintage IRIX 6.5 and Solaris workstations.

But portability doesn't mean lowest common denominator. On modern terminals, CMacs lights up: 24-bit RGB color for syntax themes, Unicode box-drawing and symbols, smooth screen updates that don't flicker. The same binary that runs on your M3 Mac will build on an SGI Octane—it just adapts to what the platform offers.

## Getting Started

CMacs depends on the [cx library](https://github.com/toddvernon/cx), which must be cloned alongside it in a specific directory structure:

```
cx/
├── cx/              <- cx library
├── cx_apps/
│   └── cm/          <- this repo
└── lib/             <- built libraries (created by make)
```

### Clone and Build

```bash
# Create the directory structure
mkdir -p ~/dev/cx/cx_apps
cd ~/dev/cx

# Clone the cx library
git clone https://github.com/toddvernon/cx.git

# Clone CMacs
cd cx_apps
git clone https://github.com/toddvernon/cm.git

# Build the cx library first
cd ~/dev/cx/cx
make

# Build CMacs
cd ~/dev/cx/cx_apps/cm
make
```

That's it. No configure scripts, no dependency resolution, no package managers. If `make` works on your system, you're done.

### If Something Goes Wrong

- **Missing headers?** Make sure you cloned into the right directory structure. The makefile expects `cx/` to be two levels up.
- **Linker errors?** Build the cx library first (`cd ~/dev/cx/cx && make`).
- **Vintage Unix?** You'll need GNU Make and GCC 2.8.1 or later. The code is portable C++, but the makefile uses GNU Make syntax (`ifeq`, `$(shell)`, `:=`) and requires `g++`. Both are available at [gnu.org](https://www.gnu.org/software/).

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

First thing to try: press `C-h` for the built-in help.

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

## Claude MCP Integration (Experimental)

On macOS and Linux, CMacs can integrate with Claude Desktop via the Model Context Protocol (MCP). This lets Claude read your open buffers, search code, and make edits directly in the editor.

### Building with MCP Support

```bash
# Build CMacs with MCP enabled
cd ~/dev/cx/cx_apps/cm
make DEVELOPER=1

# Build the MCP bridge (required)
make DEVELOPER=1 mcp_bridge
```

This produces two binaries:
- `darwin_arm64/cm` (or `linux_x86_64/cm`) — the editor with MCP support
- `darwin_arm64/mcp_bridge` — the bridge process that Claude Desktop talks to

### Configuring Claude Desktop

Add the bridge to your Claude Desktop config:

**macOS:** `~/Library/Application Support/Claude/claude_desktop_config.json`

```json
{
  "mcpServers": {
    "cm-editor": {
      "command": "/Users/yourname/dev/cx/cx_apps/cm/darwin_arm64/mcp_bridge"
    }
  }
}
```

Restart Claude Desktop after editing the config.

### Usage

1. Start Claude Desktop (the bridge starts automatically)
2. Start CMacs and open some files
3. CMacs connects to the bridge on `localhost:9876`
4. Ask Claude: "What files are open in my editor?" or "Show me line 50-60 of main.cpp"

Claude can:
- List open buffers
- Read buffer contents
- Search for patterns (including regex)
- Replace text, insert lines, delete lines
- Open and save files

The editor screen updates automatically when Claude makes changes.

## License

Apache License 2.0. See LICENSE file.
