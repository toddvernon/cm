//-------------------------------------------------------------------------------------------------
//
// mcp_bridge.cpp
//
// MCP Bridge - connects Claude Desktop to cm editor via TCP socket
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
#include <signal.h>
#include <errno.h>
#include <sstream>
#include <sys/socket.h>

#include <cx/base/string.h>
#include <cx/net/socket.h>
#include <cx/net/inaddr.h>
#include <cx/json/json_factory.h>
#include <cx/json/json_object.h>
#include <cx/json/json_array.h>
#include <cx/json/json_string.h>
#include <cx/json/json_number.h>
#include <cx/json/json_boolean.h>
#include <cx/json/json_member.h>

//-------------------------------------------------------------------------
// Constants
//-------------------------------------------------------------------------
static const int BRIDGE_PORT = 9876;
static const char* PROTOCOL_VERSION = "2024-11-05";
static const char* SERVER_NAME = "cm-mcp-bridge";
static const char* SERVER_VERSION = "1.0.0";

//-------------------------------------------------------------------------
// Global state
//-------------------------------------------------------------------------
static CxSocket* g_editorSocket = NULL;
static int g_editorConnected = 0;
static int g_nextRequestId = 1;
static FILE* g_debugLog = NULL;

//-------------------------------------------------------------------------
// Debug log file
//-------------------------------------------------------------------------
static void openDebugLog() {
    g_debugLog = fopen("/tmp/mcp_bridge.log", "a");
    if (g_debugLog) {
        fprintf(g_debugLog, "\n========== mcp_bridge started ==========\n");
        fflush(g_debugLog);
    }
}

static void closeDebugLog() {
    if (g_debugLog) {
        fprintf(g_debugLog, "========== mcp_bridge exiting ==========\n");
        fclose(g_debugLog);
        g_debugLog = NULL;
    }
}

//-------------------------------------------------------------------------
// Logging to stderr and debug file
//-------------------------------------------------------------------------
static void logMsg(const char* msg) {
    fprintf(stderr, "[mcp_bridge] %s\n", msg);
    fflush(stderr);
    if (g_debugLog) {
        fprintf(g_debugLog, "[mcp_bridge] %s\n", msg);
        fflush(g_debugLog);
    }
}

static void logMsg(CxString msg) {
    fprintf(stderr, "[mcp_bridge] %s\n", msg.data());
    fflush(stderr);
    if (g_debugLog) {
        fprintf(g_debugLog, "[mcp_bridge] %s\n", msg.data());
        fflush(g_debugLog);
    }
}

static void logError(const char* msg) {
    fprintf(stderr, "[mcp_bridge] ERROR: %s\n", msg);
    fflush(stderr);
    if (g_debugLog) {
        fprintf(g_debugLog, "[mcp_bridge] ERROR: %s\n", msg);
        fflush(g_debugLog);
    }
}

//-------------------------------------------------------------------------
// Pretty print JSON for debug log
//-------------------------------------------------------------------------
static void prettyPrintJSON(FILE* f, const char* prefix, const char* json) {
    fprintf(f, "%s ", prefix);
    int indent = 0;
    int inString = 0;
    char prev = 0;

    for (const char* p = json; *p; p++) {
        char c = *p;

        // Track string state (handle escaped quotes)
        if (c == '"' && prev != '\\') {
            inString = !inString;
        }

        if (!inString) {
            if (c == '{' || c == '[') {
                fprintf(f, "%c\n", c);
                indent += 2;
                for (int i = 0; i < indent; i++) fputc(' ', f);
            } else if (c == '}' || c == ']') {
                fprintf(f, "\n");
                indent -= 2;
                for (int i = 0; i < indent; i++) fputc(' ', f);
                fprintf(f, "%c", c);
            } else if (c == ',') {
                fprintf(f, ",\n");
                for (int i = 0; i < indent; i++) fputc(' ', f);
            } else if (c == ':') {
                fprintf(f, ": ");
            } else {
                fputc(c, f);
            }
        } else {
            fputc(c, f);
        }
        prev = c;
    }
    fprintf(f, "\n");
    fflush(f);
}

//-------------------------------------------------------------------------
// Send JSON-RPC response to stdout
//-------------------------------------------------------------------------
static void sendResponse(CxString json) {
    if (g_debugLog) {
        prettyPrintJSON(g_debugLog, ">>", json.data());
    }
    printf("%s\n", json.data());
    fflush(stdout);
}

//-------------------------------------------------------------------------
// Log incoming request
//-------------------------------------------------------------------------
static void logRequest(CxString line) {
    if (g_debugLog) {
        prettyPrintJSON(g_debugLog, "<<", line.data());
    }
}

//-------------------------------------------------------------------------
// Build JSON-RPC error response
//-------------------------------------------------------------------------
static CxString buildErrorResponse(int id, int code, CxString message) {
    char buf[4096];
    sprintf(buf, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":%d,\"message\":\"%s\"}}",
            id, code, message.data());
    return CxString(buf);
}

//-------------------------------------------------------------------------
// Build JSON-RPC result response
//-------------------------------------------------------------------------
static CxString buildResultResponse(int id, CxString result) {
    char buf[65536];
    sprintf(buf, "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}", id, result.data());
    return CxString(buf);
}

//-------------------------------------------------------------------------
// Build tools/call result (MCP format)
//-------------------------------------------------------------------------
static CxString buildToolResult(CxString text, int isError) {
    // Escape the text for JSON
    CxString escaped = "";
    for (unsigned long i = 0; i < text.length(); i++) {
        char c = text.charAt(i);
        switch (c) {
            case '"':  escaped.append("\\\""); break;
            case '\\': escaped.append("\\\\"); break;
            case '\n': escaped.append("\\n"); break;
            case '\r': escaped.append("\\r"); break;
            case '\t': escaped.append("\\t"); break;
            default:   escaped.append(c); break;
        }
    }

    char buf[65536];
    sprintf(buf, "{\"content\":[{\"type\":\"text\",\"text\":\"%s\"}],\"isError\":%s}",
            escaped.data(), isError ? "true" : "false");
    return CxString(buf);
}

//-------------------------------------------------------------------------
// Handle initialize request
//-------------------------------------------------------------------------
static CxString handleInitialize() {
    char buf[1024];
    sprintf(buf,
        "{\"protocolVersion\":\"%s\","
        "\"capabilities\":{\"tools\":{}},"
        "\"serverInfo\":{\"name\":\"%s\",\"version\":\"%s\"}}",
        PROTOCOL_VERSION, SERVER_NAME, SERVER_VERSION);
    return CxString(buf);
}

//-------------------------------------------------------------------------
// Get tools list JSON
//-------------------------------------------------------------------------
static CxString getToolsList() {
    return CxString(
        "{\"tools\":["
        "{\"name\":\"list_buffers\","
        "\"description\":\"List all open buffers with their file paths and modified status\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"

        "{\"name\":\"get_buffer\","
        "\"description\":\"Get the full contents of an open buffer\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
        "\"buffer_id\":{\"type\":\"string\",\"description\":\"Buffer identifier (file path)\"}},"
        "\"required\":[\"buffer_id\"]}},"

        "{\"name\":\"get_buffer_range\","
        "\"description\":\"Get a range of lines from a buffer\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
        "\"buffer_id\":{\"type\":\"string\"},"
        "\"start_line\":{\"type\":\"integer\",\"description\":\"1-based start line\"},"
        "\"end_line\":{\"type\":\"integer\",\"description\":\"1-based end line (inclusive)\"}},"
        "\"required\":[\"buffer_id\",\"start_line\",\"end_line\"]}},"

        "{\"name\":\"replace_range\","
        "\"description\":\"Replace a range of lines in a buffer with new text. Supports undo.\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
        "\"buffer_id\":{\"type\":\"string\"},"
        "\"start_line\":{\"type\":\"integer\",\"description\":\"1-based start line\"},"
        "\"end_line\":{\"type\":\"integer\",\"description\":\"1-based end line (inclusive)\"},"
        "\"new_text\":{\"type\":\"string\",\"description\":\"Replacement text (may contain newlines)\"}},"
        "\"required\":[\"buffer_id\",\"start_line\",\"end_line\",\"new_text\"]}},"

        "{\"name\":\"insert_lines\","
        "\"description\":\"Insert text before a given line. Supports undo.\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
        "\"buffer_id\":{\"type\":\"string\"},"
        "\"before_line\":{\"type\":\"integer\",\"description\":\"1-based line number to insert before\"},"
        "\"text\":{\"type\":\"string\",\"description\":\"Text to insert (may contain newlines)\"}},"
        "\"required\":[\"buffer_id\",\"before_line\",\"text\"]}},"

        "{\"name\":\"delete_lines\","
        "\"description\":\"Delete a range of lines from a buffer. Supports undo.\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
        "\"buffer_id\":{\"type\":\"string\"},"
        "\"start_line\":{\"type\":\"integer\",\"description\":\"1-based start line\"},"
        "\"end_line\":{\"type\":\"integer\",\"description\":\"1-based end line (inclusive)\"}},"
        "\"required\":[\"buffer_id\",\"start_line\",\"end_line\"]}},"

        "{\"name\":\"find_in_buffer\","
        "\"description\":\"Search for a string or regex pattern in a buffer. Returns matching lines.\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
        "\"buffer_id\":{\"type\":\"string\"},"
        "\"pattern\":{\"type\":\"string\",\"description\":\"Search string or regex pattern\"},"
        "\"is_regex\":{\"type\":\"boolean\",\"description\":\"Treat pattern as regex (default false)\"},"
        "\"case_insensitive\":{\"type\":\"boolean\",\"description\":\"Case insensitive search (default false)\"}},"
        "\"required\":[\"buffer_id\",\"pattern\"]}},"

        "{\"name\":\"find_and_replace\","
        "\"description\":\"Find and replace text in a buffer. Supports regex. Returns number of replacements.\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
        "\"buffer_id\":{\"type\":\"string\"},"
        "\"pattern\":{\"type\":\"string\",\"description\":\"Search string or regex pattern\"},"
        "\"replacement\":{\"type\":\"string\",\"description\":\"Replacement text\"},"
        "\"is_regex\":{\"type\":\"boolean\",\"description\":\"Treat pattern as regex (default false)\"},"
        "\"case_insensitive\":{\"type\":\"boolean\",\"description\":\"Case insensitive search (default false)\"},"
        "\"max_replacements\":{\"type\":\"integer\",\"description\":\"Max replacements, 0 = unlimited (default 0)\"}},"
        "\"required\":[\"buffer_id\",\"pattern\",\"replacement\"]}},"

        "{\"name\":\"open_file\","
        "\"description\":\"Open a file in the editor\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
        "\"path\":{\"type\":\"string\",\"description\":\"Absolute or relative file path\"}},"
        "\"required\":[\"path\"]}},"

        "{\"name\":\"save_buffer\","
        "\"description\":\"Save a buffer to disk\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
        "\"buffer_id\":{\"type\":\"string\"}},"
        "\"required\":[\"buffer_id\"]}},"

        "{\"name\":\"get_cursor\","
        "\"description\":\"Get current cursor position and active buffer\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"

        "{\"name\":\"goto_line\","
        "\"description\":\"Move cursor to a specific line in a buffer and scroll to make it visible\","
        "\"inputSchema\":{\"type\":\"object\",\"properties\":{"
        "\"buffer_id\":{\"type\":\"string\",\"description\":\"Buffer identifier (file path)\"},"
        "\"line\":{\"type\":\"integer\",\"description\":\"1-based line number to go to\"}},"
        "\"required\":[\"buffer_id\",\"line\"]}}"
        "]}"
    );
}

//-------------------------------------------------------------------------
// Convert JSON object to string
//-------------------------------------------------------------------------
static CxString jsonToString(CxJSONBase* json) {
    std::ostringstream oss;
    oss << *json;
    return CxString(oss.str().c_str());
}

//-------------------------------------------------------------------------
// Forward declaration for connection check
//-------------------------------------------------------------------------
static void checkForEditorConnection(CxSocket* serverSocket);
static CxSocket* g_serverSocket = NULL;  // Reference to server socket for reconnection

//-------------------------------------------------------------------------
// Forward tool call to editor and get response
//-------------------------------------------------------------------------
static CxString forwardToEditor(CxString toolName, CxString argsJson) {
    if (!g_editorConnected || g_editorSocket == NULL) {
        // Try to accept a pending connection before giving up
        if (g_serverSocket != NULL) {
            checkForEditorConnection(g_serverSocket);
        }
        if (!g_editorConnected || g_editorSocket == NULL) {
            return buildToolResult("editor not connected", 1);
        }
    }

    // Build request for editor
    char reqBuf[65536];
    int reqId = g_nextRequestId++;
    sprintf(reqBuf, "{\"id\":%d,\"cmd\":\"%s\",\"args\":%s}\n",
            reqId, toolName.data(), argsJson.data());

    // Send to editor
    try {
        g_editorSocket->sendAtLeast(reqBuf);
    } catch (...) {
        logError("Failed to send to editor - checking for reconnection");
        g_editorConnected = 0;
        // Try to accept a new connection (editor may have restarted)
        if (g_serverSocket != NULL) {
            checkForEditorConnection(g_serverSocket);
            if (g_editorConnected) {
                // Retry with new connection
                try {
                    g_editorSocket->sendAtLeast(reqBuf);
                } catch (...) {
                    g_editorConnected = 0;
                    return buildToolResult("editor connection lost", 1);
                }
            } else {
                return buildToolResult("editor connection lost", 1);
            }
        } else {
            return buildToolResult("editor connection lost", 1);
        }
    }

    // Read response (newline-delimited JSON)
    CxString response;
    try {
        response = g_editorSocket->recvUntil('\n');
    } catch (...) {
        logError("Failed to read from editor - checking for reconnection");
        g_editorConnected = 0;
        // Try to accept a new connection (editor may have restarted)
        if (g_serverSocket != NULL) {
            checkForEditorConnection(g_serverSocket);
        }
        return buildToolResult("editor not responding", 1);
    }

    // Parse editor response
    CxJSONBase* json = CxJSONFactory::parse(response);
    if (json == NULL) {
        return buildToolResult("invalid response from editor", 1);
    }

    CxJSONObject* obj = (CxJSONObject*)json;
    CxJSONMember* okMember = obj->find("ok");

    if (okMember == NULL) {
        delete json;
        return buildToolResult("malformed response from editor", 1);
    }

    CxJSONBoolean* okValue = (CxJSONBoolean*)okMember->object();
    int isOk = okValue->get();

    if (isOk) {
        CxJSONMember* dataMember = obj->find("data");
        if (dataMember != NULL) {
            CxJSONBase* dataValue = dataMember->object();
            CxString dataStr = jsonToString(dataValue);
            delete json;
            return buildToolResult(dataStr, 0);
        }
        delete json;
        return buildToolResult("success", 0);
    } else {
        CxJSONMember* errorMember = obj->find("error");
        CxString errorMsg = "unknown error";
        if (errorMember != NULL) {
            CxJSONString* errorValue = (CxJSONString*)errorMember->object();
            errorMsg = errorValue->get();
        }
        delete json;
        return buildToolResult(errorMsg, 1);
    }
}

//-------------------------------------------------------------------------
// Handle tools/call request
//-------------------------------------------------------------------------
static CxString handleToolsCall(CxJSONObject* params) {
    // Get tool name
    CxJSONMember* nameMember = params->find("name");
    if (nameMember == NULL) {
        return buildToolResult("missing tool name", 1);
    }
    CxJSONString* nameValue = (CxJSONString*)nameMember->object();
    CxString toolName = nameValue->get();

    // Get arguments
    CxJSONMember* argsMember = params->find("arguments");
    CxString argsJson = "{}";
    if (argsMember != NULL) {
        argsJson = jsonToString(argsMember->object());
    }

    logMsg(CxString("Tool call: ") + toolName);

    return forwardToEditor(toolName, argsJson);
}

//-------------------------------------------------------------------------
// Process a JSON-RPC request
//-------------------------------------------------------------------------
static void processRequest(CxString line) {
    // Log incoming request
    logRequest(line);

    // Parse JSON
    CxJSONBase* json = CxJSONFactory::parse(line);
    if (json == NULL) {
        sendResponse(buildErrorResponse(0, -32700, "Parse error"));
        return;
    }

    CxJSONObject* request = (CxJSONObject*)json;

    // Get id (may be null for notifications)
    int id = 0;
    CxJSONMember* idMember = request->find("id");
    if (idMember != NULL) {
        CxJSONNumber* idValue = (CxJSONNumber*)idMember->object();
        id = (int)idValue->get();
    }

    // Get method
    CxJSONMember* methodMember = request->find("method");
    if (methodMember == NULL) {
        delete json;
        sendResponse(buildErrorResponse(id, -32600, "Invalid Request: missing method"));
        return;
    }
    CxJSONString* methodValue = (CxJSONString*)methodMember->object();
    CxString method = methodValue->get();

    // Handle methods
    if (method == "initialize") {
        CxString result = handleInitialize();
        sendResponse(buildResultResponse(id, result));
    }
    else if (method == "initialized") {
        // Notification, no response
        logMsg("Client initialized");
    }
    else if (method == "tools/list") {
        CxString result = getToolsList();
        sendResponse(buildResultResponse(id, result));
    }
    else if (method == "tools/call") {
        CxJSONMember* paramsMember = request->find("params");
        if (paramsMember == NULL) {
            delete json;
            sendResponse(buildErrorResponse(id, -32602, "Invalid params"));
            return;
        }
        CxJSONObject* params = (CxJSONObject*)paramsMember->object();
        CxString result = handleToolsCall(params);
        sendResponse(buildResultResponse(id, result));
    }
    else {
        sendResponse(buildErrorResponse(id, -32601, "Method not found"));
    }

    delete json;
}

//-------------------------------------------------------------------------
// Accept editor connection (non-blocking check)
//-------------------------------------------------------------------------
static void checkForEditorConnection(CxSocket* serverSocket) {
    // Check if there's a pending connection (non-blocking)
    if (serverSocket->recvDataPending(0, 100000)) {  // 100ms timeout
        if (g_editorSocket != NULL) {
            g_editorSocket->close();
            delete g_editorSocket;
        }

        CxSocket accepted = serverSocket->accept();
        if (accepted.good()) {
            g_editorSocket = new CxSocket(accepted);
            g_editorConnected = 1;
            logMsg("Editor connected");
        }
    }
}

//-------------------------------------------------------------------------
// Main
//-------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // Ignore SIGPIPE (broken pipe)
    signal(SIGPIPE, SIG_IGN);

    // Open debug log
    openDebugLog();

    logMsg("Starting MCP bridge");

    // Create server socket
    CxSocket serverSocket;
    CxInetAddress addr(BRIDGE_PORT, "127.0.0.1");
    addr.process();

    // Allow port reuse so new bridge can start even if old one just exited
    int reuseAddr = 1;
    setsockopt(serverSocket.fd(), SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

    if (serverSocket.bind(addr) != 0) {
        logError("Failed to bind to port");
        return 1;
    }

    if (serverSocket.listen(1) != 0) {
        logError("Failed to listen");
        return 1;
    }

    char msg[256];
    sprintf(msg, "Listening on port %d", BRIDGE_PORT);
    logMsg(msg);

    // Store server socket reference for reconnection handling
    g_serverSocket = &serverSocket;

    // Main loop: read from stdin, process requests
    char inputBuf[65536];
    while (fgets(inputBuf, sizeof(inputBuf), stdin) != NULL) {
        // Check for editor connections periodically
        checkForEditorConnection(&serverSocket);

        // Process the input line
        CxString line = inputBuf;
        line = line.stripTrailing("\n\r");

        if (line.length() > 0) {
            processRequest(line);
        }
    }

    logMsg("Shutting down");

    if (g_editorSocket != NULL) {
        g_editorSocket->close();
        delete g_editorSocket;
    }
    serverSocket.close();

    closeDebugLog();
    return 0;
}

#else

// Not Linux/macOS
#include <stdio.h>

int main(int argc, char* argv[]) {
    fprintf(stderr, "mcp_bridge: not supported on this platform\n");
    return 1;
}

#endif
