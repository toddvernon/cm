//-------------------------------------------------------------------------------------------------
//
//  CmTypes.h
//  cmacs
//
//  Platform-specific type definitions for UTF-8 support.
//  On modern platforms (Linux, macOS), use UTF-8 aware edit buffers.
//  On older platforms (IRIX, Solaris), use traditional byte-based buffers.
//
//-------------------------------------------------------------------------------------------------

#ifndef _CmTypes_h_
#define _CmTypes_h_

//-------------------------------------------------------------------------------------------------
// Enable UTF-8 support on modern platforms
//-------------------------------------------------------------------------------------------------
#if defined(_LINUX_) || defined(_OSX_)
#define CM_UTF8_SUPPORT
#endif

//-------------------------------------------------------------------------------------------------
// Include appropriate headers and create typedefs
//-------------------------------------------------------------------------------------------------
#ifdef CM_UTF8_SUPPORT

#include <cx/editbuffer/utfeditbuffer.h>
#include <cx/editbuffer/utfeditbufferlist.h>

typedef CxUTFEditBuffer     CmEditBuffer;
typedef CxUTFEditBufferList CmEditBufferList;

#else

#include <cx/editbuffer/editbuffer.h>
#include <cx/editbuffer/editbufferlist.h>

typedef CxEditBuffer        CmEditBuffer;
typedef CxEditBufferList    CmEditBufferList;

#endif

#endif
