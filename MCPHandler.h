//-------------------------------------------------------------------------------------------------
//
// MCPHandler.h
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

#ifndef _MCPHANDLER_H_
#define _MCPHANDLER_H_

#if defined(_LINUX_) || defined(_OSX_)

#include <cx/base/string.h>
#include <cx/net/socket.h>
#include <cx/thread/thread.h>
#include <cx/thread/mutex.h>
#include <cx/thread/cond.h>
#include <cx/json/json_object.h>
#include "CmTypes.h"

class ScreenEditor;

//-------------------------------------------------------------------------------------------------
// MCPHandler - background thread that connects to mcp_bridge
//-------------------------------------------------------------------------------------------------

class MCPHandler : public CxThread
{
public:
    MCPHandler(ScreenEditor *editor);
    ~MCPHandler();

    void run();             // Thread entry point (override from CxThread)
    void shutdown();        // Request shutdown

    int needsRedraw();      // Check if screen needs refresh
    void clearNeedsRedraw();

    CxString getStatusMessage();   // Get pending status message (thread-safe)
    void clearStatusMessage();     // Clear the status message

    int isConnected();             // Check if connected to mcp_bridge

    void processPendingRequest();  // Called from main thread to process queued request

private:
    void setStatusMessage(CxString msg);  // Set status message (called from handlers)
    // Command dispatch
    CxString handleCommand(CxJSONObject *request);

    // Individual command handlers
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
    CxString handleGotoLine(CxString bufferId, int line);

    // Response builders
    CxString buildSuccessResponse(int id, CxString data);
    CxString buildErrorResponse(int id, CxString error);
    CxString escapeJSON(CxString text);

    // Helper to find buffer by path or filename
    CmEditBuffer* findBuffer(CxString bufferId);

    // State
    ScreenEditor *_editor;
    CxSocket *_socket;
    CxMutex _mutex;
    CxCondition _condition;
    int _needsRedraw;
    int _shutdownRequested;
    CxString _statusMessage;
    int _connected;

    // Request queue for main thread execution
    CxJSONObject *_pendingRequest;
    CxString _pendingResponse;
    int _requestReady;
    int _responseReady;
};

#endif // _LINUX_ || _OSX_

#endif // _MCPHANDLER_H_
