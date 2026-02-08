//-------------------------------------------------------------------------------------------------
//
//  FileListView.h
//  cmacs
//
//  Created by Todd Vernon on 9/15/23.
//  Copyright © 2022 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <math.h>

#include <sys/types.h>
#include <iostream>
#include <cx/base/string.h>
#include <cx/base/slist.h>
#include <cx/base/star.h>
#include <cx/base/hashmap.h>

#include <cx/keyboard/keyboard.h>
#include <cx/screen/screen.h>
#include <cx/screen/boxframe.h>
#include <cx/functor/defercall.h>

#include "ProgramDefaults.h"
#include "CmTypes.h"
#include "Project.h"


#ifndef _FileListView_h_
#define _FileListView_h_

//---------------------------------------------------------------------------------------------------------
//
//
//---------------------------------------------------------------------------------------------------------
class FileListView
{
  public:

    FileListView( ProgramDefaults *pd, CmEditBufferList *ebl, Project *proj, CxScreen *screen );
    // Constructor

    void routeKeyAction( CxKeyAction keyAction );
    // route a keyboard action
    
    void redraw( void );
    // redraw the part of the screen with the list
    
    void recalcScreenPlacements(void);
    // recalcs the key places in the window that parts are placed

    
    int calcLongestName(void);
    
    CxString getSelectedItem( void );
    // get the selected item in the list

    CmEditBuffer *getSelectedBuffer( void );
    // get the selected edit buffer (for save operation)

    void setVisible( int visible );
    // set visibility state for resize handling

  private:

    void screenResizeCallback( void );
    // callback to receive host window size updates

    int handleArrows( CxKeyAction keyAction );
    // handle the arrow keys
    
    int reframe( void );
    // make sure selection is visible in list
    
    CmEditBufferList *editBufferList;
    // pointer the the list of files being edited
    
    ProgramDefaults *programDefaults;
    // pointer to the program defaults

    CxScreen *screen;
    // pointer to the screen object

    CxBoxFrame *frame;
    // box frame for modal display

    Project *project;
    
    // these hold the key screen locations (zero based) for the editor all in screen
    // coordinates
    int  screenNumberOfLines;
    int  screenNumberOfCols;
    
    int  screenFileListTitleBarLine;
    int  screenFileListFrameLine;
    int  screenFileListNumberOfLines;
	int  screenFileListNumberOfCols;
    int  screenFileListFirstListLine;  // index of the first visible edit row (zero based)
    int  screenFileListLastListLine;   // index of the last visible edit row (zero based)
   
	// need list visible bounds 

    // these hold the windowing values that are used to translate edit buffer coordinates
    // to screen coordinates
    
    int firstVisibleListIndex;
	int selectedListItemIndex;

    int _visible;  // whether modal is currently displayed

};

#endif

