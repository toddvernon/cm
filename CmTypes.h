//-------------------------------------------------------------------------------------------------
//
//  CmTypes.h
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  Platform-specific type definitions for UTF-8 support.
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
