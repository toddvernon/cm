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


//-------------------------------------------------------------------------------------------------
// cmCursorDisplayColumn
//
// Returns the cursor's *display* column (zero based) — i.e. its visual column on screen.
//
// On UTF-enabled platforms a single character can occupy 0 (combining), 1 (ASCII / most),
// or 2 (CJK / emoji) display columns, so cursor.col (a character index) is NOT the same as
// the cursor's visual column. The UTF buffer exposes cursorDisplayColumn() for this case.
//
// On non-UTF builds the buffer holds bytes and cursor.col is already the display column.
//
// All horizontal-window math (visible window first/last col, scroll thresholds, screen
// position calculations) must use display columns to behave correctly when the line
// contains double-width characters.
//-------------------------------------------------------------------------------------------------
inline unsigned long
cmCursorDisplayColumn( CmEditBuffer *eb )
{
#ifdef CM_UTF8_SUPPORT
    return (unsigned long)eb->cursorDisplayColumn();
#else
    return eb->cursor.col;
#endif
}

#endif
