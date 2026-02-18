//-------------------------------------------------------------------------------------------------
//
//  BuildView.h
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  Modal view for displaying build output with error/warning navigation.
//
//-------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <math.h>

#include <sys/types.h>
#include <cx/base/string.h>
#include <cx/base/slist.h>
#include <cx/base/star.h>
#include <cx/base/hashmap.h>

#include <cx/keyboard/keyboard.h>
#include <cx/screen/screen.h>
#include <cx/screen/boxframe.h>
#include <cx/functor/defercall.h>
#include <cx/buildoutput/buildoutput.h>

#include "ProgramDefaults.h"
#include "CmTypes.h"


#ifndef _BuildView_h_
#define _BuildView_h_

//-------------------------------------------------------------------------------------------------
// Spinner animation characters
//-------------------------------------------------------------------------------------------------
#if defined(_LINUX_) || defined(_OSX_)
static const char *BUILD_SPINNER_CHARS[] = { "|", "/", "-", "\\" };
#else
static const char *BUILD_SPINNER_CHARS[] = { "|", "/", "-", "\\" };
#endif
#define BUILD_SPINNER_COUNT 4


//---------------------------------------------------------------------------------------------------------
//
//  BuildView
//
//  Modal dialog that displays build output. Lines are classified as errors, warnings, or plain
//  output. Arrow keys navigate between lines. Enter on an error/warning line returns it for
//  goto-error functionality.
//
//---------------------------------------------------------------------------------------------------------
class BuildView
{
  public:

    BuildView( ProgramDefaults *pd, CxScreen *screen, BuildOutput *buildOutput );
    // Constructor

    void routeKeyAction( CxKeyAction keyAction );
    // route a keyboard action

    void redraw( void );
    // redraw the part of the screen with the list

    void recalcScreenPlacements(void);
    // recalcs the key places in the window that parts are placed

    BuildOutputLine *getSelectedLine( void );
    // get the selected line (for goto-error)

    int hasSelectedError( void );
    // returns 1 if selected line has file:line info

    void setVisible( int visible );
    // set visibility state for resize handling

    void advanceSpinner( void );
    // advance the spinner animation (call periodically during build)

    void scrollToEnd( void );
    // scroll to show the last line of output (call when new output arrives)

  private:

    int handleArrows( CxKeyAction keyAction );
    // handle the arrow keys

    int reframe( void );
    // make sure selection is visible in list

    void drawLine( int screenRow, int logicalIndex );
    // draw a single line

    ProgramDefaults *programDefaults;
    // pointer to the program defaults

    CxScreen *screen;
    // pointer to the screen object

    CxBoxFrame *frame;
    // box frame for modal display

    BuildOutput *buildOutput;
    // pointer to the build output data

    // screen placement values (zero based)
    int screenNumberOfLines;
    int screenNumberOfCols;

    int screenBuildTitleBarLine;
    int screenBuildFrameLine;
    int screenBuildNumberOfLines;
    int screenBuildNumberOfCols;
    int screenBuildFirstListLine;   // index of the first visible line
    int screenBuildLastListLine;    // index of the last visible line

    // scrolling and selection
    int firstVisibleLineIndex;
    int selectedLineIndex;

    int _visible;  // whether modal is currently displayed

    // spinner animation
    int _spinnerIndex;
};

#endif
