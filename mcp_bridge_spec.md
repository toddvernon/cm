# MCP Editor Bridge — Implementation Spec

## Overview

Two C++ programs that connect Claude Desktop to cm (terminal text editor) via the Model Context Protocol (MCP). The editor is a full-screen terminal application that owns stdin/stdout for curses-style I/O, so MCP communication goes through a separate bridge process.

```
Claude Desktop <--stdio JSON-RPC--> mcp_bridge <--TCP localhost:9876--> cm
                                    (server)                            (client)
```

## Architecture

### mcp_bridge (standalone executable, acts as TCP server)

- Launched by Claude Desktop as a child process
- Reads MCP JSON-RPC 2.0 from stdin, writes responses to stdout
- All logging goes to stderr
- Listens on TCP `localhost:9876` for editor connections
- Tolerates editor not being connected — returns error responses to Claude when editor is absent
- Long-lived: stays running as long as Claude Desktop keeps it alive
- Accepts one editor connection at a time

### cm socket thread (connects as TCP client)

- A background thread in cm that connects to `localhost:9876`
- Blocks on socket recv waiting for commands
- Executes commands directly (with mutex protection for buffer access)
- Sends results back over the socket
- Sets `_mcpNeedsRedraw` flag when buffers are modified
- On cm quit, socket is closed. On cm start, attempts to connect (non-fatal if bridge not running)

### Why this architecture?

- Bridge is always running (Claude Desktop keeps it alive)
- cm can start/stop freely without cleanup issues
- No Unix domain socket file to manage
- Uses existing cx/net TCP socket library (no AF_UNIX support needed)
- Blocking keyboard reads in cm don't need to change

## Platform Support

**Linux and macOS only.** Claude Desktop doesn't run on SunOS/IRIX, and the cx/thread library only has pthread implementations for Linux/OSX.

All MCP-related code should be wrapped in platform guards:
```cpp
#if defined(_LINUX_) || defined(_OSX_)
// MCP code here
#endif
```

This applies to:
- `mcp_bridge.cpp` — entire file (or just don't build on other platforms)
- `MCPHandler.h/cpp` — entire files
- MCP-related members in `ScreenEditor.h/cpp`

On unsupported platforms, cm compiles and runs normally without MCP support.

## cx Library Usage

### From cx/net
- `CxSocket` — TCP socket with connect(), send(), recv(), recvUntil()
- `CxInetAddress` — TCP address (host + port)

### From cx/thread
- `CxThread` — subclass, override run(), call start()/join()
- `CxMutex` — acquire()/release() for buffer protection
- `CxCondition` — wait()/signal() if needed for shutdown coordination

### From cx/json
- `CxJSONFactory::parse()` — parse incoming JSON
- `CxJSONObject`, `CxJSONString`, `CxJSONNumber`, `CxJSONArray` — build responses

### From cx/regex (new, Linux/macOS only)
- `CxRegex` — compile and match regex patterns
- `regexReplaceAll()` — utility for find/replace

## Wire Protocols

### MCP side (bridge stdin/stdout)

Standard MCP over JSON-RPC 2.0. The bridge handles these methods:

#### initialize

Request from Claude Desktop:
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "protocolVersion": "2024-11-05",
    "capabilities": {},
    "clientInfo": { "name": "claude-desktop", "version": "1.0.0" }
  }
}
```

Bridge responds:
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "protocolVersion": "2024-11-05",
    "capabilities": { "tools": {} },
    "serverInfo": { "name": "cm-mcp-bridge", "version": "1.0.0" }
  }
}
```

#### initialized (notification, no response)
```json
{ "jsonrpc": "2.0", "method": "initialized" }
```

#### tools/list

Bridge responds with the tool catalog (hardcoded in bridge):
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "tools": [
      {
        "name": "list_buffers",
        "description": "List all open buffers with their file paths and modified status",
        "inputSchema": { "type": "object", "properties": {} }
      },
      {
        "name": "get_buffer",
        "description": "Get the full contents of an open buffer",
        "inputSchema": {
          "type": "object",
          "properties": {
            "buffer_id": { "type": "string", "description": "Buffer identifier (file path)" }
          },
          "required": ["buffer_id"]
        }
      },
      {
        "name": "get_buffer_range",
        "description": "Get a range of lines from a buffer",
        "inputSchema": {
          "type": "object",
          "properties": {
            "buffer_id": { "type": "string" },
            "start_line": { "type": "integer", "description": "1-based start line" },
            "end_line": { "type": "integer", "description": "1-based end line (inclusive)" }
          },
          "required": ["buffer_id", "start_line", "end_line"]
        }
      },
      {
        "name": "replace_range",
        "description": "Replace a range of lines in a buffer with new text. Supports undo.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "buffer_id": { "type": "string" },
            "start_line": { "type": "integer", "description": "1-based start line" },
            "end_line": { "type": "integer", "description": "1-based end line (inclusive)" },
            "new_text": { "type": "string", "description": "Replacement text (may contain newlines)" }
          },
          "required": ["buffer_id", "start_line", "end_line", "new_text"]
        }
      },
      {
        "name": "insert_lines",
        "description": "Insert text before a given line. Supports undo.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "buffer_id": { "type": "string" },
            "before_line": { "type": "integer", "description": "1-based line number to insert before" },
            "text": { "type": "string", "description": "Text to insert (may contain newlines)" }
          },
          "required": ["buffer_id", "before_line", "text"]
        }
      },
      {
        "name": "delete_lines",
        "description": "Delete a range of lines from a buffer. Supports undo.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "buffer_id": { "type": "string" },
            "start_line": { "type": "integer", "description": "1-based start line" },
            "end_line": { "type": "integer", "description": "1-based end line (inclusive)" }
          },
          "required": ["buffer_id", "start_line", "end_line"]
        }
      },
      {
        "name": "find_in_buffer",
        "description": "Search for a string or regex pattern in a buffer. Returns matching lines.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "buffer_id": { "type": "string" },
            "pattern": { "type": "string", "description": "Search string or regex pattern" },
            "is_regex": { "type": "boolean", "description": "Treat pattern as regex (default false)" },
            "case_insensitive": { "type": "boolean", "description": "Case insensitive search (default false)" }
          },
          "required": ["buffer_id", "pattern"]
        }
      },
      {
        "name": "find_and_replace",
        "description": "Find and replace text in a buffer. Supports regex. Returns number of replacements made.",
        "inputSchema": {
          "type": "object",
          "properties": {
            "buffer_id": { "type": "string" },
            "pattern": { "type": "string", "description": "Search string or regex pattern" },
            "replacement": { "type": "string", "description": "Replacement text" },
            "is_regex": { "type": "boolean", "description": "Treat pattern as regex (default false)" },
            "case_insensitive": { "type": "boolean", "description": "Case insensitive search (default false)" },
            "max_replacements": { "type": "integer", "description": "Max replacements, 0 = unlimited (default 0)" }
          },
          "required": ["buffer_id", "pattern", "replacement"]
        }
      },
      {
        "name": "open_file",
        "description": "Open a file in the editor",
        "inputSchema": {
          "type": "object",
          "properties": {
            "path": { "type": "string", "description": "Absolute or relative file path" }
          },
          "required": ["path"]
        }
      },
      {
        "name": "save_buffer",
        "description": "Save a buffer to disk",
        "inputSchema": {
          "type": "object",
          "properties": {
            "buffer_id": { "type": "string" }
          },
          "required": ["buffer_id"]
        }
      },
      {
        "name": "get_cursor",
        "description": "Get current cursor position and active buffer",
        "inputSchema": { "type": "object", "properties": {} }
      }
    ]
  }
}
```

#### tools/call

Bridge forwards to editor over socket, wraps editor response in MCP format.

Success:
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "result": {
    "content": [{ "type": "text", "text": "...result..." }],
    "isError": false
  }
}
```

Error (editor returned error):
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "result": {
    "content": [{ "type": "text", "text": "buffer 'foo.cpp' not open" }],
    "isError": true
  }
}
```

Error (editor not connected):
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "result": {
    "content": [{ "type": "text", "text": "editor not connected" }],
    "isError": true
  }
}
```

### Editor socket protocol (bridge <-> cm)

Simple newline-delimited JSON over TCP. NOT JSON-RPC — keep it minimal.

#### Request (bridge -> cm)

```json
{"id": 1, "cmd": "get_buffer", "args": {"buffer_id": "main.cpp"}}
```

Fields:
- `id` — integer, monotonically increasing, used to match responses
- `cmd` — string, matches MCP tool name exactly
- `args` — object, matches MCP tool arguments exactly (pass-through)

#### Response (cm -> bridge)

Success:
```json
{"id": 1, "ok": true, "data": "#include <stdio.h>\nint main() {\n    return 0;\n}\n"}
```

Error:
```json
{"id": 1, "ok": false, "error": "buffer not open"}
```

Fields:
- `id` — matches the request id
- `ok` — boolean
- `data` — string or JSON value with the result (present when ok is true)
- `error` — string error message (present when ok is false)

#### Response data formats by command

- `list_buffers` — data is JSON array: `[{"buffer_id": "main.cpp", "path": "/home/user/main.cpp", "modified": true}, ...]`
- `get_buffer` — data is string (full buffer contents)
- `get_buffer_range` — data is string (requested lines)
- `replace_range` — data is string: `"replaced lines 10-15"`
- `insert_lines` — data is string: `"inserted 3 lines before line 10"`
- `delete_lines` — data is string: `"deleted lines 10-15"`
- `find_in_buffer` — data is JSON array: `[{"line": 42, "text": "    int x = foo();"}]`
- `find_and_replace` — data is JSON object: `{"replacements": 5, "message": "replaced 5 occurrences"}`
- `open_file` — data is string: `"opened main.cpp"` or buffer_id
- `save_buffer` — data is string: `"saved"`
- `get_cursor` — data is JSON object: `{"buffer_id": "main.cpp", "line": 42, "col": 10}`

---

## Implementation Plan

### Phase 0: CxRegex library (cx/regex)

**New library in cx repository:** `cx/regex/`

Linux/macOS only. Wraps POSIX regex (`<regex.h>`) in a simple C++ class.

**Files:**
- `cx/regex/regex.h`
- `cx/regex/regex.cpp`
- `cx/regex/makefile`

**CxRegex class:**
```cpp
#if defined(_LINUX_) || defined(_OSX_)

#include <regex.h>
#include <cx/base/string.h>
#include <cx/base/slist.h>

class CxRegex
{
public:
    CxRegex();
    ~CxRegex();

    int compile(CxString pattern, int caseInsensitive = 0);
    // Compile pattern. Returns 0 on success, error code on failure.

    int isCompiled();
    // Returns true if pattern compiled successfully.

    int match(CxString text);
    // Returns true if text matches the compiled pattern.

    int match(CxString text, int *matchStart, int *matchLen);
    // Returns true if match found, sets start position and length.

    CxString getError();
    // Returns error message if compile() failed.

    static CxString escapeForLiteral(CxString text);
    // Escape special regex characters for literal matching.

private:
    regex_t _compiled;
    int _isCompiled;
    CxString _errorMsg;
};

// Utility functions
CxSList<int> regexFindLines(CxString pattern, CxSList<CxString> *lines);
// Find all line numbers (0-based) that match pattern.

CxString regexReplace(CxString text, CxString pattern, CxString replacement);
// Replace first match in text.

CxString regexReplaceAll(CxString text, CxString pattern, CxString replacement);
// Replace all matches in text.

#endif
```

**makefile:**
```makefile
# Only build on Linux/macOS
ifeq ($(UNAME_S),darwin)
    BUILD_REGEX=1
endif
ifeq ($(UNAME_S),linux)
    BUILD_REGEX=1
endif

ifndef BUILD_REGEX
all:
	@echo "CxRegex: skipping build (not Linux/macOS)"
else
# normal build rules...
endif
```

**Library output:** `lib/darwin_arm64/libcx_regex.a` (etc.)

**Update cx top-level makefile:**

The main `cx/makefile` needs to include the regex directory in the library build. Add to the list of subdirectories:

```makefile
# In cx/makefile, add regex to SUBDIRS (Linux/macOS only)
ifeq ($(UNAME_S),darwin)
    SUBDIRS += regex
endif
ifeq ($(UNAME_S),linux)
    SUBDIRS += regex
endif
```

Or if using explicit targets:
```makefile
libs: base json net thread regex ...

regex:
    cd regex && $(MAKE)
```

The regex makefile itself handles the platform check, so it's safe to call on all platforms (it just skips the build on unsupported ones).

**Testing:**
Create `cx_tests/cxregex/` with unit tests for:
- Basic pattern matching
- Case insensitive matching
- Match position extraction
- Replace operations
- Error handling for invalid patterns
- Special character escaping

### Phase 1: mcp_bridge executable

**New files in cm directory:**
- `mcp_bridge.cpp` — standalone MCP bridge program

**Functionality:**
1. Main loop reads lines from stdin, parses JSON-RPC
2. Handles `initialize`, `initialized`, `tools/list` directly
3. For `tools/call`, forwards to connected editor socket
4. TCP server on localhost:9876, accepts one connection
5. Uses cx/json for parsing, cx/net for sockets

**Build:**
```makefile
mcp_bridge: mcp_bridge.cpp
    $(CPP) $(CPPFLAGS) $(INC) mcp_bridge.cpp -o mcp_bridge $(JSON_LIB) $(NET_LIB) $(BASE_LIB)
```

**Testing:**
```bash
# Test MCP protocol manually
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' | ./mcp_bridge

# Test with MCP Inspector
npx @modelcontextprotocol/inspector ./mcp_bridge
```

### Phase 2: cm socket thread infrastructure

**New files:**
- `MCPHandler.h` — MCPHandler class declaration
- `MCPHandler.cpp` — socket thread + command handlers

**MCPHandler class:**
```cpp
class MCPHandler : public CxThread
{
public:
    MCPHandler(ScreenEditor *editor);
    ~MCPHandler();

    void run();  // thread entry point
    void shutdown();

    int needsRedraw();
    void clearNeedsRedraw();

private:
    void handleCommand(CxJSONObject *request);
    CxString handleListBuffers();
    CxString handleGetBuffer(CxString bufferId);
    CxString handleGetBufferRange(CxString bufferId, int startLine, int endLine);
    CxString handleReplaceRange(CxString bufferId, int startLine, int endLine, CxString newText);
    CxString handleInsertLines(CxString bufferId, int beforeLine, CxString text);
    CxString handleDeleteLines(CxString bufferId, int startLine, int endLine);
    CxString handleFindInBuffer(CxString bufferId, CxString pattern, int isRegex, int caseInsensitive);
    CxString handleFindAndReplace(CxString bufferId, CxString pattern, CxString replacement,
                                   int isRegex, int caseInsensitive, int maxReplacements);
    CxString handleOpenFile(CxString path);
    CxString handleSaveBuffer(CxString bufferId);
    CxString handleGetCursor();

    ScreenEditor *_editor;
    CxSocket *_socket;
    CxMutex _bufferMutex;
    int _needsRedraw;
    int _shutdownRequested;
};
```

**Thread behavior:**
```
run():
    while (!_shutdownRequested):
        try to connect to localhost:9876
        if connected:
            while connected && !_shutdownRequested:
                line = socket.recvUntil('\n')
                request = CxJSONFactory::parse(line)
                _bufferMutex.acquire()
                response = handleCommand(request)
                if (command was write operation):
                    _needsRedraw = 1
                _bufferMutex.release()
                socket.sendAtLeast(response + "\n")
        sleep 1 second before retry
```

### Phase 3: Integrate MCPHandler into ScreenEditor

**Changes to ScreenEditor.h:**
```cpp
#if defined(_LINUX_) || defined(_OSX_)
#include "MCPHandler.h"
#endif

class ScreenEditor {
    // ... existing members ...

private:
#if defined(_LINUX_) || defined(_OSX_)
    MCPHandler *_mcpHandler;
    CxMutex *_bufferMutex;  // shared with MCPHandler
#endif
};
```

**Changes to ScreenEditor.cpp:**

Constructor:
```cpp
#if defined(_LINUX_) || defined(_OSX_)
_bufferMutex = new CxMutex();
_mcpHandler = new MCPHandler(this, _bufferMutex);
_mcpHandler->start();
#endif
```

Destructor:
```cpp
#if defined(_LINUX_) || defined(_OSX_)
_mcpHandler->shutdown();
_mcpHandler->join();
delete _mcpHandler;
delete _bufferMutex;
#endif
```

Main loop (after keyboard read returns):
```cpp
#if defined(_LINUX_) || defined(_OSX_)
// Check if MCP handler modified buffers
if (_mcpHandler->needsRedraw()) {
    _mcpHandler->clearNeedsRedraw();
    redrawScreen();
}
#endif
```

**Buffer access pattern:**

Any code that modifies buffers should acquire the mutex:
```cpp
_bufferMutex->acquire();
// modify buffer
_bufferMutex->release();
```

For read-only operations (most of what cm does), mutex is optional but recommended for consistency.

### Phase 4: Implement command handlers

Each handler in MCPHandler.cpp accesses ScreenEditor's buffer list:

**list_buffers:**
```cpp
CxString MCPHandler::handleListBuffers() {
    CxString result = "[";
    // iterate _editor->_bufferList
    // for each: {"buffer_id": "...", "path": "...", "modified": true/false}
    result += "]";
    return result;
}
```

**get_buffer:**
```cpp
CxString MCPHandler::handleGetBuffer(CxString bufferId) {
    CxUTFEditBuffer *buf = _editor->findBufferById(bufferId);
    if (!buf) return errorResponse("buffer not found");
    return buf->asString();  // or iterate lines
}
```

**replace_range:**
```cpp
CxString MCPHandler::handleReplaceRange(CxString bufferId, int start, int end, CxString text) {
    CxUTFEditBuffer *buf = _editor->findBufferById(bufferId);
    if (!buf) return errorResponse("buffer not found");

    // Delete lines start..end
    // Insert new text at start
    // Mark buffer modified

    return successResponse("replaced lines");
}
```

**find_in_buffer (with regex):**
```cpp
CxString MCPHandler::handleFindInBuffer(CxString bufferId, CxString pattern,
                                         int isRegex, int caseInsensitive) {
    CxUTFEditBuffer *buf = _editor->findBufferById(bufferId);
    if (!buf) return errorResponse("buffer not found");

    CxString result = "[";
    int first = 1;

    if (isRegex) {
        CxRegex regex;
        if (regex.compile(pattern, caseInsensitive) != 0) {
            return errorResponse(regex.getError());
        }
        for (unsigned long i = 0; i < buf->lineCount(); i++) {
            CxString line = buf->lineAt(i);
            if (regex.match(line)) {
                if (!first) result += ",";
                result += "{\"line\":";
                result += CxString::fromInt(i + 1);  // 1-based
                result += ",\"text\":\"";
                result += escapeJSON(line);
                result += "\"}";
                first = 0;
            }
        }
    } else {
        // literal string search
        for (unsigned long i = 0; i < buf->lineCount(); i++) {
            CxString line = buf->lineAt(i);
            if (line.indexOf(pattern) >= 0) {
                // ... same as above
            }
        }
    }
    result += "]";
    return successResponse(result);
}
```

**find_and_replace:**
```cpp
CxString MCPHandler::handleFindAndReplace(CxString bufferId, CxString pattern,
                                           CxString replacement, int isRegex,
                                           int caseInsensitive, int maxReplacements) {
    CxUTFEditBuffer *buf = _editor->findBufferById(bufferId);
    if (!buf) return errorResponse("buffer not found");

    int count = 0;
    if (isRegex) {
        CxRegex regex;
        if (regex.compile(pattern, caseInsensitive) != 0) {
            return errorResponse(regex.getError());
        }
        for (unsigned long i = 0; i < buf->lineCount(); i++) {
            CxString line = buf->lineAt(i);
            CxString newLine = regexReplaceAll(line, pattern, replacement);
            if (newLine != line) {
                buf->replaceLine(i, newLine);
                count++;
                if (maxReplacements > 0 && count >= maxReplacements) break;
            }
        }
    } else {
        // literal string replace using CxString::replaceAll()
    }

    buf->setModified(true);
    return successResponse("{\"replacements\":" + CxString::fromInt(count) + "}");
}
```

### Phase 5: Add helper methods to ScreenEditor

MCPHandler needs access to buffer operations. Add public methods:

```cpp
class ScreenEditor {
public:
    // For MCP handler
    CxUTFEditBuffer* findBufferByPath(CxString path);
    CxSList<CxUTFEditBuffer*>* getBufferList();
    CxUTFEditBuffer* getCurrentBuffer();
    unsigned long getCursorLine();
    unsigned long getCursorCol();
    void loadFileIntoNewBuffer(CxString path);
    void saveBuffer(CxUTFEditBuffer *buf);
};
```

---

## Build Integration

**makefile additions (Linux/macOS only):**

```makefile
LIB_CX_NET_NAME=libcx_net.a
LIB_CX_THREAD_NAME=libcx_thread.a
LIB_CX_REGEX_NAME=libcx_regex.a

# MCP support only on Linux and macOS
ifeq ($(UNAME_S),darwin)
    MCP_SUPPORT=1
endif
ifeq ($(UNAME_S),linux)
    MCP_SUPPORT=1
endif

ifdef MCP_SUPPORT
    # Add net, thread, and regex libs to CX_LIBS
    CX_LIBS += \
        $(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_NET_NAME) \
        $(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_THREAD_NAME) \
        $(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_REGEX_NAME)

    # pthread needed for threading
    PLATFORM_LIBS += -lpthread

    # Add MCPHandler to objects
    MCP_OBJECTS = $(APP_OBJECT_DIR)/MCPHandler.o
endif

OBJECTS = \
    ... existing objects ... \
    $(MCP_OBJECTS)

# Separate target for bridge (only on supported platforms)
ifdef MCP_SUPPORT
mcp_bridge: mcp_bridge.cpp
    $(CPP) $(CPPFLAGS) $(INC) mcp_bridge.cpp -o $(APP_OBJECT_DIR)/mcp_bridge \
        $(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_JSON_NAME) \
        $(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_NET_NAME) \
        $(LIB_CX_PLATFORM_LIB_DIR)/$(LIB_CX_BASE_NAME)
endif
```

---

## Claude Desktop Configuration

File: `~/Library/Application Support/Claude/claude_desktop_config.json` (macOS)

```json
{
  "mcpServers": {
    "cm-editor": {
      "command": "/path/to/cm/darwin_arm64/mcp_bridge"
    }
  }
}
```

---

## Testing Plan

### Unit testing (Phase 1)
1. Run mcp_bridge, type JSON-RPC to stdin, verify responses
2. Test initialize, tools/list responses
3. Test error handling for malformed JSON

### Integration testing (Phase 2-4)
1. Start mcp_bridge in one terminal
2. Start cm in another terminal
3. Verify cm connects (check bridge stderr logging)
4. Use netcat or test client to send commands through bridge

### End-to-end testing (Phase 5)
1. Configure Claude Desktop with mcp_bridge
2. Open cm with a file
3. Ask Claude "what buffers are open in my editor?"
4. Ask Claude "show me line 10-20 of main.cpp"
5. Ask Claude "add a comment at line 5"
6. Verify cm screen updates

---

## Thread Safety Notes

**Protected by _bufferMutex:**
- All buffer read/write operations in MCPHandler
- Buffer list modifications in ScreenEditor (if any happen during MCP ops)

**Not protected (main thread only):**
- Screen/curses operations (never touch from socket thread)
- Keyboard handling
- Command line view

**Atomic flag:**
- `_needsRedraw` — set by socket thread, read/cleared by main thread
- Could use volatile or atomic, but mutex around it is safer

---

## Future Extensions

- `undo` / `redo` tools
- `goto_line` to move cursor (requires main thread to do screen update)
- `get_selection` / `set_selection` for mark-based operations
- `run_command` for arbitrary cm ESC commands
- Multiple cm instances (use different ports, or editor ID in protocol)
- Reconnection handling (cm reconnects if bridge restarts)
