//-------------------------------------------------------------------------------------------------
//
// MCPHandler.cpp
//
// MCP socket thread - connects to mcp_bridge and handles tool commands
//
// Linux and macOS only.
//
//-------------------------------------------------------------------------------------------------
//
// (c) Copyright 2024 T.Vernon
// ALL RIGHTS RESERVED
//
//-------------------------------------------------------------------------------------------------

#if defined(_LINUX_) || defined(_OSX_)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "MCPHandler.h"
#include "ScreenEditor.h"

#include <cx/net/inaddr.h>
#include <cx/json/json_factory.h>
#include <cx/json/json_object.h>
#include <cx/json/json_array.h>
#include <cx/json/json_string.h>
#include <cx/json/json_number.h>
#include <cx/json/json_boolean.h>
#include <cx/json/json_member.h>
#include <cx/regex/regex.h>

//-------------------------------------------------------------------------
// Constants
//-------------------------------------------------------------------------
static const int BRIDGE_PORT = 9876;
static const int RECONNECT_DELAY_SEC = 2;

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
MCPHandler::MCPHandler(ScreenEditor *editor)
    : _editor(editor)
    , _socket(NULL)
    , _needsRedraw(0)
    , _shutdownRequested(0)
    , _connected(0)
    , _pendingRequest(NULL)
    , _requestReady(0)
    , _responseReady(0)
{
}

//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
MCPHandler::~MCPHandler()
{
    if (_socket != NULL) {
        _socket->close();
        delete _socket;
        _socket = NULL;
    }
}

//-------------------------------------------------------------------------
// Request shutdown
//-------------------------------------------------------------------------
void MCPHandler::shutdown()
{
    _shutdownRequested = 1;
    suggestQuit();  // from CxThread

    // Close socket to unblock recvUntil
    _mutex.acquire();
    if (_socket != NULL) {
        _socket->close();
    }
    _mutex.release();
}

//-------------------------------------------------------------------------
// Check if screen needs refresh
//-------------------------------------------------------------------------
int MCPHandler::needsRedraw()
{
    _mutex.acquire();
    int result = _needsRedraw;
    _mutex.release();
    return result;
}

//-------------------------------------------------------------------------
// Clear redraw flag
//-------------------------------------------------------------------------
void MCPHandler::clearNeedsRedraw()
{
    _mutex.acquire();
    _needsRedraw = 0;
    _mutex.release();
}

//-------------------------------------------------------------------------
// Set status message (called from handlers)
// NOTE: Handlers are called from processPendingRequest which holds the mutex,
// so we don't acquire here to avoid deadlock with non-recursive mutex.
//-------------------------------------------------------------------------
void MCPHandler::setStatusMessage(CxString msg)
{
    _statusMessage = msg;
}

//-------------------------------------------------------------------------
// Get pending status message (thread-safe)
//-------------------------------------------------------------------------
CxString MCPHandler::getStatusMessage()
{
    _mutex.acquire();
    CxString result = _statusMessage;
    _mutex.release();
    return result;
}

//-------------------------------------------------------------------------
// Clear the status message
//-------------------------------------------------------------------------
void MCPHandler::clearStatusMessage()
{
    _mutex.acquire();
    _statusMessage = "";
    _mutex.release();
}

//-------------------------------------------------------------------------
// Check if connected to mcp_bridge
//-------------------------------------------------------------------------
int MCPHandler::isConnected()
{
    _mutex.acquire();
    int result = _connected;
    _mutex.release();
    return result;
}

//-------------------------------------------------------------------------
// Escape a string for JSON
//-------------------------------------------------------------------------
CxString MCPHandler::escapeJSON(CxString text)
{
    CxString escaped = "";
    for (unsigned long i = 0; i < text.length(); i++) {
        char c = text.charAt(i);
        switch (c) {
            case '"':  escaped.append("\\\""); break;
            case '\\': escaped.append("\\\\"); break;
            case '\n': escaped.append("\\n"); break;
            case '\r': escaped.append("\\r"); break;
            case '\t': escaped.append("\\t"); break;
            case '\b': escaped.append("\\b"); break;
            case '\f': escaped.append("\\f"); break;
            default:
                if ((unsigned char)c < 0x20) {
                    // Control character - skip or escape as \uXXXX
                    char buf[8];
                    sprintf(buf, "\\u%04x", (unsigned char)c);
                    escaped.append(buf);
                } else {
                    escaped.append(c);
                }
                break;
        }
    }
    return escaped;
}

//-------------------------------------------------------------------------
// Find buffer by path or filename suffix
// Tries exact match first, then falls back to matching by filename
//-------------------------------------------------------------------------
CmEditBuffer* MCPHandler::findBuffer(CxString bufferId)
{
    CmEditBufferList *bufList = _editor->editBufferList;

    // Try exact path match first
    CmEditBuffer *buf = bufList->findPath(bufferId);
    if (buf != NULL) {
        return buf;
    }

    // Fall back to matching by filename suffix
    // This handles cases where bufferId is "file.cpp" but path is "/full/path/file.cpp"
    for (int i = 0; i < bufList->items(); i++) {
        CmEditBuffer *b = bufList->at(i);
        CxString path = b->getFilePath();

        // Check if path ends with bufferId
        if (path.length() >= bufferId.length()) {
            int offset = path.length() - bufferId.length();
            // Make sure we're matching a full filename (preceded by / or at start)
            if (offset == 0 || path.charAt(offset - 1) == '/') {
                CxString suffix = path.subString(offset, bufferId.length());
                if (suffix == bufferId) {
                    return b;
                }
            }
        }
    }

    return NULL;
}

//-------------------------------------------------------------------------
// Build success response
//-------------------------------------------------------------------------
CxString MCPHandler::buildSuccessResponse(int id, CxString data)
{
    CxString response = "{\"id\":";
    char idBuf[32];
    sprintf(idBuf, "%d", id);
    response.append(idBuf);
    response.append(",\"ok\":true,\"data\":");
    response.append(data);
    response.append("}");
    return response;
}

//-------------------------------------------------------------------------
// Build error response
//-------------------------------------------------------------------------
CxString MCPHandler::buildErrorResponse(int id, CxString error)
{
    CxString response = "{\"id\":";
    char idBuf[32];
    sprintf(idBuf, "%d", id);
    response.append(idBuf);
    response.append(",\"ok\":false,\"error\":\"");
    response.append(escapeJSON(error));
    response.append("\"}");
    return response;
}

//-------------------------------------------------------------------------
// Thread entry point
//-------------------------------------------------------------------------
void MCPHandler::run()
{
    while (!_suggestQuit && !_shutdownRequested) {
        try {
            // Try to connect to bridge
            CxSocket *sock = new CxSocket();
            CxInetAddress addr(BRIDGE_PORT, "127.0.0.1");
            addr.process();

            if (sock->connect(addr, 5) != 0) {
                // Connection failed - retry after delay
                delete sock;
                for (int i = 0; i < RECONNECT_DELAY_SEC && !_shutdownRequested; i++) {
                    sleep(1);
                }
                continue;
            }

            // Connected
            _mutex.acquire();
            _socket = sock;
            _connected = 1;
            _mutex.release();
            _needsRedraw = 1;  // Update status bar to show connected

            // Main receive loop
            while (!_suggestQuit && !_shutdownRequested) {
                CxString line;
                try {
                    line = _socket->recvUntil('\n');
                } catch (...) {
                    // Socket error or closed
                    break;
                }

                if (line.length() == 0) {
                    // Connection closed
                    break;
                }

                // Parse request
                line = line.stripTrailing("\n\r");
                CxJSONBase *json = CxJSONFactory::parse(line);
                if (json == NULL) {
                    // Send error response for unparseable JSON
                    CxString errorResp = "{\"id\":0,\"ok\":false,\"error\":\"JSON parse error\"}\n";
                    try {
                        _socket->sendAtLeast(errorResp);
                    } catch (...) {
                        break;
                    }
                    continue;
                }

                CxJSONObject *request = (CxJSONObject *)json;

                // Queue request for main thread and wait for response
                _mutex.acquire();
                _pendingRequest = request;
                _requestReady = 1;
                _responseReady = 0;

                // Wait for main thread to process and set response
                while (!_responseReady && !_shutdownRequested) {
                    _condition.timedWait(&_mutex, 1);  // 1 second timeout to check shutdown
                }

                CxString response = _pendingResponse;
                _pendingRequest = NULL;
                _requestReady = 0;
                _mutex.release();

                delete json;

                // Send response
                response.append("\n");
                try {
                    _socket->sendAtLeast(response);
                } catch (...) {
                    break;
                }
            }

            // Cleanup socket
            _mutex.acquire();
            if (_socket != NULL) {
                _socket->close();
                delete _socket;
                _socket = NULL;
            }
            _connected = 0;
            _mutex.release();
            _needsRedraw = 1;  // Update status bar to show disconnected

        } catch (...) {
            // Catch any other socket exceptions
            _mutex.acquire();
            if (_socket != NULL) {
                try { _socket->close(); } catch (...) {}
                delete _socket;
                _socket = NULL;
            }
            _connected = 0;
            _mutex.release();
            _needsRedraw = 1;  // Update status bar to show disconnected
        }

        // Brief delay before reconnect attempt
        if (!_shutdownRequested) {
            sleep(1);
        }
    }
}

//-------------------------------------------------------------------------
// Process pending request on main thread
// Called from the keyboard idle callback to ensure thread safety
//-------------------------------------------------------------------------
void MCPHandler::processPendingRequest()
{
    _mutex.acquire();
    if (_requestReady && _pendingRequest != NULL) {
        // Process the command on the main thread
        _pendingResponse = handleCommand(_pendingRequest);
        _responseReady = 1;
        _condition.signal();
    }
    _mutex.release();
}

//-------------------------------------------------------------------------
// Command dispatcher
//-------------------------------------------------------------------------
CxString MCPHandler::handleCommand(CxJSONObject *request)
{
    // Get request ID
    int id = 0;
    CxJSONMember *idMember = request->find("id");
    if (idMember != NULL) {
        CxJSONNumber *idValue = (CxJSONNumber *)idMember->object();
        id = (int)idValue->get();
    }

    // Get command name
    CxJSONMember *cmdMember = request->find("cmd");
    if (cmdMember == NULL) {
        return buildErrorResponse(id, "missing cmd field");
    }
    CxJSONString *cmdValue = (CxJSONString *)cmdMember->object();
    CxString cmd = cmdValue->get();

    // Get arguments object
    CxJSONObject *args = NULL;
    CxJSONMember *argsMember = request->find("args");
    if (argsMember != NULL) {
        args = (CxJSONObject *)argsMember->object();
    }

    // Dispatch to handler
    if (cmd == "list_buffers") {
        return handleListBuffers();
    }
    else if (cmd == "get_buffer") {
        CxString bufferId = "";
        if (args != NULL) {
            CxJSONMember *m = args->find("buffer_id");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                bufferId = s->get();
            }
        }
        return handleGetBuffer(bufferId);
    }
    else if (cmd == "get_buffer_range") {
        CxString bufferId = "";
        int startLine = 1, endLine = 1;
        if (args != NULL) {
            CxJSONMember *m = args->find("buffer_id");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                bufferId = s->get();
            }
            m = args->find("start_line");
            if (m != NULL) {
                CxJSONNumber *n = (CxJSONNumber *)m->object();
                startLine = (int)n->get();
            }
            m = args->find("end_line");
            if (m != NULL) {
                CxJSONNumber *n = (CxJSONNumber *)m->object();
                endLine = (int)n->get();
            }
        }
        return handleGetBufferRange(bufferId, startLine, endLine);
    }
    else if (cmd == "replace_range") {
        CxString bufferId = "", newText = "";
        int startLine = 1, endLine = 1;
        if (args != NULL) {
            CxJSONMember *m = args->find("buffer_id");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                bufferId = s->get();
            }
            m = args->find("start_line");
            if (m != NULL) {
                CxJSONNumber *n = (CxJSONNumber *)m->object();
                startLine = (int)n->get();
            }
            m = args->find("end_line");
            if (m != NULL) {
                CxJSONNumber *n = (CxJSONNumber *)m->object();
                endLine = (int)n->get();
            }
            m = args->find("new_text");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                newText = s->get();
            }
        }
        return handleReplaceRange(bufferId, startLine, endLine, newText);
    }
    else if (cmd == "insert_lines") {
        CxString bufferId = "", text = "";
        int beforeLine = 1;
        if (args != NULL) {
            CxJSONMember *m = args->find("buffer_id");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                bufferId = s->get();
            }
            m = args->find("before_line");
            if (m != NULL) {
                CxJSONNumber *n = (CxJSONNumber *)m->object();
                beforeLine = (int)n->get();
            }
            m = args->find("text");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                text = s->get();
            }
        }
        return handleInsertLines(bufferId, beforeLine, text);
    }
    else if (cmd == "delete_lines") {
        CxString bufferId = "";
        int startLine = 1, endLine = 1;
        if (args != NULL) {
            CxJSONMember *m = args->find("buffer_id");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                bufferId = s->get();
            }
            m = args->find("start_line");
            if (m != NULL) {
                CxJSONNumber *n = (CxJSONNumber *)m->object();
                startLine = (int)n->get();
            }
            m = args->find("end_line");
            if (m != NULL) {
                CxJSONNumber *n = (CxJSONNumber *)m->object();
                endLine = (int)n->get();
            }
        }
        return handleDeleteLines(bufferId, startLine, endLine);
    }
    else if (cmd == "find_in_buffer") {
        CxString bufferId = "", pattern = "";
        int isRegex = 0, caseInsensitive = 0;
        if (args != NULL) {
            CxJSONMember *m = args->find("buffer_id");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                bufferId = s->get();
            }
            m = args->find("pattern");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                pattern = s->get();
            }
            m = args->find("is_regex");
            if (m != NULL) {
                CxJSONBoolean *b = (CxJSONBoolean *)m->object();
                isRegex = b->get() ? 1 : 0;
            }
            m = args->find("case_insensitive");
            if (m != NULL) {
                CxJSONBoolean *b = (CxJSONBoolean *)m->object();
                caseInsensitive = b->get() ? 1 : 0;
            }
        }
        return handleFindInBuffer(bufferId, pattern, isRegex, caseInsensitive);
    }
    else if (cmd == "find_and_replace") {
        CxString bufferId = "", pattern = "", replacement = "";
        int isRegex = 0, caseInsensitive = 0, maxReplacements = 0;
        if (args != NULL) {
            CxJSONMember *m = args->find("buffer_id");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                bufferId = s->get();
            }
            m = args->find("pattern");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                pattern = s->get();
            }
            m = args->find("replacement");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                replacement = s->get();
            }
            m = args->find("is_regex");
            if (m != NULL) {
                CxJSONBoolean *b = (CxJSONBoolean *)m->object();
                isRegex = b->get() ? 1 : 0;
            }
            m = args->find("case_insensitive");
            if (m != NULL) {
                CxJSONBoolean *b = (CxJSONBoolean *)m->object();
                caseInsensitive = b->get() ? 1 : 0;
            }
            m = args->find("max_replacements");
            if (m != NULL) {
                CxJSONNumber *n = (CxJSONNumber *)m->object();
                maxReplacements = (int)n->get();
            }
        }
        return handleFindAndReplace(bufferId, pattern, replacement, isRegex, caseInsensitive, maxReplacements);
    }
    else if (cmd == "open_file") {
        CxString path = "";
        if (args != NULL) {
            CxJSONMember *m = args->find("path");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                path = s->get();
            }
        }
        return handleOpenFile(path);
    }
    else if (cmd == "save_buffer") {
        CxString bufferId = "";
        if (args != NULL) {
            CxJSONMember *m = args->find("buffer_id");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                bufferId = s->get();
            }
        }
        return handleSaveBuffer(bufferId);
    }
    else if (cmd == "get_cursor") {
        return handleGetCursor();
    }
    else if (cmd == "goto_line") {
        CxString bufferId = "";
        int line = 1;
        if (args != NULL) {
            CxJSONMember *m = args->find("buffer_id");
            if (m != NULL) {
                CxJSONString *s = (CxJSONString *)m->object();
                bufferId = s->get();
            }
            m = args->find("line");
            if (m != NULL) {
                CxJSONNumber *n = (CxJSONNumber *)m->object();
                line = (int)n->get();
            }
        }
        return handleGotoLine(bufferId, line);
    }
    else {
        return buildErrorResponse(id, CxString("unknown command: ") + cmd);
    }
}

//-------------------------------------------------------------------------
// list_buffers - List all open buffers
//-------------------------------------------------------------------------
CxString MCPHandler::handleListBuffers()
{
    CxString result = "[";
    int first = 1;

    CmEditBufferList *bufList = _editor->editBufferList;
    int count = bufList->items();

    for (int i = 0; i < count; i++) {
        CmEditBuffer *buf = bufList->at(i);
        if (buf == NULL) continue;

        if (!first) result.append(",");
        first = 0;

        result.append("{\"buffer_id\":\"");
        result.append(escapeJSON(buf->getFilePath()));
        result.append("\",\"path\":\"");
        result.append(escapeJSON(buf->getFilePath()));
        result.append("\",\"modified\":");
        result.append(buf->isTouched() ? "true" : "false");
        result.append("}");
    }

    result.append("]");
    return buildSuccessResponse(0, result);
}

//-------------------------------------------------------------------------
// get_buffer - Get full buffer contents
//-------------------------------------------------------------------------
CxString MCPHandler::handleGetBuffer(CxString bufferId)
{
    CmEditBufferList *bufList = _editor->editBufferList;
    CmEditBuffer *buf = findBuffer(bufferId);

    if (buf == NULL) {
        return buildErrorResponse(0, CxString("buffer not found: ") + bufferId);
    }

    CxString content = buf->flattenBuffer();

    // Limit response size to avoid timeout (socket reads byte-by-byte)
    // For large files, suggest using get_buffer_range
    if (content.length() > 10000) {
        char msg[256];
        unsigned long lineCount = buf->numberOfLines();
        sprintf(msg, "buffer too large (%lu bytes, %lu lines). Use get_buffer_range with start_line and end_line to read in chunks.",
                content.length(), lineCount);
        return buildErrorResponse(0, msg);
    }

    CxString result = "\"";
    result.append(escapeJSON(content));
    result.append("\"");

    return buildSuccessResponse(0, result);
}

//-------------------------------------------------------------------------
// get_buffer_range - Get a range of lines
//-------------------------------------------------------------------------
CxString MCPHandler::handleGetBufferRange(CxString bufferId, int startLine, int endLine)
{
    CmEditBufferList *bufList = _editor->editBufferList;
    CmEditBuffer *buf = findBuffer(bufferId);

    if (buf == NULL) {
        return buildErrorResponse(0, CxString("buffer not found: ") + bufferId);
    }

    unsigned long lineCount = buf->numberOfLines();

    // Convert to 0-based, clamp to valid range
    int start = startLine - 1;
    int end = endLine - 1;
    if (start < 0) start = 0;
    if (end < 0) end = 0;
    if ((unsigned long)start >= lineCount) start = lineCount - 1;
    if ((unsigned long)end >= lineCount) end = lineCount - 1;
    if (start > end) {
        return buildErrorResponse(0, "invalid line range");
    }

    CxString content = "";
    for (int i = start; i <= end; i++) {
        CxUTFString *line = buf->line(i);
        if (line != NULL) {
            if (i > start) content.append("\n");
            content.append(line->toBytes());
        }
    }

    CxString result = "\"";
    result.append(escapeJSON(content));
    result.append("\"");

    return buildSuccessResponse(0, result);
}

//-------------------------------------------------------------------------
// replace_range - Replace a range of lines
//-------------------------------------------------------------------------
CxString MCPHandler::handleReplaceRange(CxString bufferId, int startLine, int endLine, CxString newText)
{
    CmEditBufferList *bufList = _editor->editBufferList;
    CmEditBuffer *buf = findBuffer(bufferId);

    if (buf == NULL) {
        return buildErrorResponse(0, CxString("buffer not found: ") + bufferId);
    }

    unsigned long lineCount = buf->numberOfLines();

    // Convert to 0-based
    int start = startLine - 1;
    int end = endLine - 1;
    if (start < 0) start = 0;
    if (end < 0) end = 0;
    if ((unsigned long)start >= lineCount) {
        return buildErrorResponse(0, "start line out of range");
    }
    if ((unsigned long)end >= lineCount) end = lineCount - 1;

    // Position cursor at start of range
    buf->cursorGotoRequest(start, 0);
    buf->setMark();

    // Position cursor at end of range (end of last line)
    CxUTFString *lastLine = buf->line(end);
    unsigned long lastCol = lastLine ? lastLine->charCount() : 0;
    buf->cursorGotoRequest(end, lastCol);

    // Delete the marked range
    buf->deleteText();

    // Insert new text
    buf->insertTextAtCursor(newText);

    char statusMsg[64];
    sprintf(statusMsg, "(MCP: replaced lines %d-%d)", startLine, endLine);
    setStatusMessage(statusMsg);
    _needsRedraw = 1;

    char msg[64];
    sprintf(msg, "\"replaced lines %d-%d\"", startLine, endLine);
    return buildSuccessResponse(0, msg);
}

//-------------------------------------------------------------------------
// insert_lines - Insert text before a line
//-------------------------------------------------------------------------
CxString MCPHandler::handleInsertLines(CxString bufferId, int beforeLine, CxString text)
{
    CmEditBufferList *bufList = _editor->editBufferList;
    CmEditBuffer *buf = findBuffer(bufferId);

    if (buf == NULL) {
        return buildErrorResponse(0, CxString("buffer not found: ") + bufferId);
    }

    // Convert to 0-based and clamp to valid range
    unsigned long lineCount = buf->numberOfLines();
    int lineNum = beforeLine - 1;
    if (lineNum < 0) lineNum = 0;
    if (lineCount == 0) {
        lineNum = 0;  // Empty buffer - insert at beginning
    } else if ((unsigned long)lineNum >= lineCount) {
        lineNum = lineCount;  // Insert at end
    }

    // Position cursor at start of the line (or end of buffer if appending)
    int appending = 0;
    if (lineCount == 0 || (unsigned long)lineNum >= lineCount) {
        // Append to end of buffer
        appending = 1;
        if (lineCount > 0) {
            unsigned long lastLine = lineCount - 1;
            CxUTFString *lastLineObj = buf->line(lastLine);
            unsigned long lastCol = lastLineObj ? lastLineObj->charCount() : 0;
            buf->cursorGotoRequest(lastLine, lastCol);
            buf->addReturn();  // Add newline before new content
        } else {
            buf->cursorGotoRequest(0, 0);  // Empty buffer
        }
    } else {
        buf->cursorGotoRequest(lineNum, 0);
    }

    // Insert text
    buf->insertTextAtCursor(text);

    // Add trailing newline only if inserting before existing content
    if (!appending) {
        buf->addReturn();
    }

    // Count lines inserted
    int linesInserted = 1;
    for (unsigned long i = 0; i < text.length(); i++) {
        if (text.charAt(i) == '\n') linesInserted++;
    }

    char statusMsg[64];
    sprintf(statusMsg, "(MCP: inserted %d lines at line %d)", linesInserted, beforeLine);
    setStatusMessage(statusMsg);
    _needsRedraw = 1;

    char msg[64];
    sprintf(msg, "\"inserted %d lines before line %d\"", linesInserted, beforeLine);
    return buildSuccessResponse(0, msg);
}

//-------------------------------------------------------------------------
// delete_lines - Delete a range of lines
//-------------------------------------------------------------------------
CxString MCPHandler::handleDeleteLines(CxString bufferId, int startLine, int endLine)
{
    CmEditBufferList *bufList = _editor->editBufferList;
    CmEditBuffer *buf = findBuffer(bufferId);

    if (buf == NULL) {
        return buildErrorResponse(0, CxString("buffer not found: ") + bufferId);
    }

    unsigned long lineCount = buf->numberOfLines();

    // Convert to 0-based
    int start = startLine - 1;
    int end = endLine - 1;
    if (start < 0) start = 0;
    if (end < 0) end = 0;
    if ((unsigned long)start >= lineCount) {
        return buildErrorResponse(0, "start line out of range");
    }
    if ((unsigned long)end >= lineCount) end = lineCount - 1;

    // Position cursor at start of range
    buf->cursorGotoRequest(start, 0);
    buf->setMark();

    // Position cursor at start of line after range (or end of buffer)
    if ((unsigned long)(end + 1) < lineCount) {
        buf->cursorGotoRequest(end + 1, 0);
    } else {
        // Deleting to end of buffer
        CxUTFString *lastLine = buf->line(end);
        unsigned long lastCol = lastLine ? lastLine->charCount() : 0;
        buf->cursorGotoRequest(end, lastCol);
    }

    // Delete the marked range
    buf->deleteText();

    char statusMsg[64];
    sprintf(statusMsg, "(MCP: deleted lines %d-%d)", startLine, endLine);
    setStatusMessage(statusMsg);
    _needsRedraw = 1;

    char msg[64];
    sprintf(msg, "\"deleted lines %d-%d\"", startLine, endLine);
    return buildSuccessResponse(0, msg);
}

//-------------------------------------------------------------------------
// find_in_buffer - Search for pattern in buffer
//-------------------------------------------------------------------------
CxString MCPHandler::handleFindInBuffer(CxString bufferId, CxString pattern, int isRegex, int caseInsensitive)
{
    CmEditBufferList *bufList = _editor->editBufferList;
    CmEditBuffer *buf = findBuffer(bufferId);

    if (buf == NULL) {
        return buildErrorResponse(0, CxString("buffer not found: ") + bufferId);
    }

    CxString result = "[";
    int first = 1;

    CxRegex regex;
    if (isRegex) {
        if (regex.compile(pattern, caseInsensitive) != 0) {
            return buildErrorResponse(0, CxString("invalid regex: ") + regex.getError());
        }
    }

    unsigned long lineCount = buf->numberOfLines();
    for (unsigned long i = 0; i < lineCount; i++) {
        CxUTFString *lineObj = buf->line(i);
        if (lineObj == NULL) continue;

        CxString lineText = lineObj->toBytes();
        int matched = 0;

        if (isRegex) {
            matched = regex.match(lineText);
        } else {
            // Simple string search
            if (caseInsensitive) {
                matched = (CxString::toLower(lineText).index(CxString::toLower(pattern)) >= 0) ? 1 : 0;
            } else {
                matched = (lineText.index(pattern) >= 0) ? 1 : 0;
            }
        }

        if (matched) {
            if (!first) result.append(",");
            first = 0;

            char lineBuf[32];
            sprintf(lineBuf, "%lu", i + 1);  // 1-based

            result.append("{\"line\":");
            result.append(lineBuf);
            result.append(",\"text\":\"");
            result.append(escapeJSON(lineText));
            result.append("\"}");
        }
    }

    result.append("]");
    return buildSuccessResponse(0, result);
}

//-------------------------------------------------------------------------
// find_and_replace - Find and replace in buffer
//-------------------------------------------------------------------------
CxString MCPHandler::handleFindAndReplace(CxString bufferId, CxString pattern, CxString replacement,
                                           int isRegex, int caseInsensitive, int maxReplacements)
{
    CmEditBufferList *bufList = _editor->editBufferList;
    CmEditBuffer *buf = findBuffer(bufferId);

    if (buf == NULL) {
        return buildErrorResponse(0, CxString("buffer not found: ") + bufferId);
    }

    CxRegex regex;
    if (isRegex) {
        if (regex.compile(pattern, caseInsensitive) != 0) {
            return buildErrorResponse(0, CxString("invalid regex: ") + regex.getError());
        }
    }

    int totalReplacements = 0;
    unsigned long lineCount = buf->numberOfLines();

    for (unsigned long i = 0; i < lineCount; i++) {
        if (maxReplacements > 0 && totalReplacements >= maxReplacements) break;

        CxUTFString *lineObj = buf->line(i);
        if (lineObj == NULL) continue;

        CxString lineText = lineObj->toBytes();
        CxString newLine;

        if (isRegex) {
            newLine = regexReplaceAll(lineText, pattern, replacement, caseInsensitive);
        } else {
            // Simple string replace - replaceAll modifies in place and returns count
            newLine = lineText;
            newLine.replaceAll(pattern, replacement);
        }

        if (newLine != lineText) {
            // Replace line content by positioning cursor and rewriting
            buf->cursorGotoRequest(i, 0);
            buf->setMark();
            CxUTFString *currentLine = buf->line(i);
            unsigned long lineLen = currentLine ? currentLine->charCount() : 0;
            buf->cursorGotoRequest(i, lineLen);
            buf->deleteText();
            buf->insertTextAtCursor(newLine);
            totalReplacements++;
        }
    }

    if (totalReplacements > 0) {
        char statusMsg[64];
        sprintf(statusMsg, "(MCP: replaced %d occurrences)", totalReplacements);
        setStatusMessage(statusMsg);
        _needsRedraw = 1;
    } else {
        setStatusMessage("(MCP: no replacements made)");
    }

    char msg[128];
    sprintf(msg, "{\"replacements\":%d,\"message\":\"replaced %d occurrences\"}",
            totalReplacements, totalReplacements);
    return buildSuccessResponse(0, msg);
}

//-------------------------------------------------------------------------
// open_file - Open a file in the editor
//-------------------------------------------------------------------------
CxString MCPHandler::handleOpenFile(CxString path)
{
    if (path.length() == 0) {
        return buildErrorResponse(0, "path is required");
    }

    // If path is relative, resolve it to absolute using cwd
    CxString resolvedPath = path;
    if (path.charAt(0) != '/') {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            resolvedPath = CxString(cwd) + "/" + path;
        }
    }

    // Check if already open
    CmEditBufferList *bufList = _editor->editBufferList;
    CmEditBuffer *existing = findBuffer(resolvedPath);
    if (existing != NULL) {
        // Already open - switch to that buffer
        _editor->editView->setEditBuffer(existing);
        _editor->editView->reframeAndUpdateScreen();

        setStatusMessage(CxString("(MCP: switched to ") + path + ")");
        _needsRedraw = 1;

        CxString result = "\"switched to ";
        result.append(escapeJSON(resolvedPath));
        result.append("\"");
        return buildSuccessResponse(0, result);
    }

    // Check if file exists first
    FILE *testFile = fopen(resolvedPath.data(), "r");
    if (testFile == NULL) {
        return buildErrorResponse(0, CxString("file not found: ") + resolvedPath);
    }
    fclose(testFile);

    // Load file (use preload=1 to immediately load content into memory)
    int loaded = _editor->loadNewFile(resolvedPath, 1);
    if (!loaded) {
        return buildErrorResponse(0, CxString("failed to open file (exists but loadNewFile failed): ") + resolvedPath);
    }

    // Switch view to the newly loaded buffer (like nextBuffer/previousBuffer do)
    _editor->editView->reframeAndUpdateScreen();

    setStatusMessage(CxString("(MCP: opened ") + path + ")");
    _needsRedraw = 1;

    CxString result = "\"opened ";
    result.append(escapeJSON(path));
    result.append("\"");
    return buildSuccessResponse(0, result);
}

//-------------------------------------------------------------------------
// save_buffer - Save a buffer to disk
//-------------------------------------------------------------------------
CxString MCPHandler::handleSaveBuffer(CxString bufferId)
{
    CmEditBufferList *bufList = _editor->editBufferList;
    CmEditBuffer *buf = findBuffer(bufferId);

    if (buf == NULL) {
        return buildErrorResponse(0, CxString("buffer not found: ") + bufferId);
    }

    CxString filePath = buf->getFilePath();
    if (filePath.length() == 0) {
        return buildErrorResponse(0, "buffer has no file path");
    }

    buf->saveText(filePath);

    setStatusMessage(CxString("(MCP: saved ") + filePath + ")");
    _needsRedraw = 1;

    return buildSuccessResponse(0, "\"saved\"");
}

//-------------------------------------------------------------------------
// get_cursor - Get current cursor position
//-------------------------------------------------------------------------
CxString MCPHandler::handleGetCursor()
{
    // Get current buffer from edit view
    CmEditBuffer *buf = _editor->editView->getEditBuffer();
    if (buf == NULL) {
        return buildErrorResponse(0, "no active buffer");
    }

    CxEditBufferPosition pos = _editor->editView->cursorPosition();

    CxString result = "{\"buffer_id\":\"";
    result.append(escapeJSON(buf->getFilePath()));
    result.append("\",\"line\":");

    char lineBuf[32];
    sprintf(lineBuf, "%lu", pos.row + 1);  // 1-based
    result.append(lineBuf);

    result.append(",\"col\":");
    sprintf(lineBuf, "%lu", pos.col + 1);  // 1-based
    result.append(lineBuf);

    result.append("}");

    return buildSuccessResponse(0, result);
}

//-------------------------------------------------------------------------
// goto_line - Move cursor to a specific line
//-------------------------------------------------------------------------
CxString MCPHandler::handleGotoLine(CxString bufferId, int line)
{
    CmEditBufferList *bufList = _editor->editBufferList;
    CmEditBuffer *buf = findBuffer(bufferId);

    if (buf == NULL) {
        return buildErrorResponse(0, CxString("buffer not found: ") + bufferId);
    }

    // Check if this is the current buffer
    CmEditBuffer *currentBuf = _editor->editView->getEditBuffer();
    if (currentBuf != buf) {
        return buildErrorResponse(0, "buffer is not the active buffer");
    }

    // Convert to 0-based
    int lineNum = line - 1;
    if (lineNum < 0) lineNum = 0;
    unsigned long lineCount = buf->numberOfLines();
    if ((unsigned long)lineNum >= lineCount) {
        lineNum = lineCount - 1;
    }

    // Move cursor using EditView's method which handles screen scrolling
    _editor->editView->cursorGotoLine(lineNum);

    char statusMsg[64];
    sprintf(statusMsg, "(MCP: jumped to line %d)", line);
    setStatusMessage(statusMsg);
    _needsRedraw = 1;

    char msg[64];
    sprintf(msg, "\"moved to line %d\"", line);
    return buildSuccessResponse(0, msg);
}

#endif // _LINUX_ || _OSX_
