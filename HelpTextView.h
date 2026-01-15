//-------------------------------------------------------------------------------------------------
//
//  HelpTextView.h
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

#include <cx/editbuffer/editbuffer.h>

#include <cx/keyboard/keyboard.h>
#include <cx/screen/screen.h>
#include <cx/functor/defercall.h>

#include "ProgramDefaults.h"
#include "Project.h"
#include "MarkUp.h"


#ifndef _HelpTextView_h_
#define _HelpTextView_h_

//-------------------------------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------------------------------
class HelpTextView
{
  public:

    HelpTextView( ProgramDefaults *pd, Project *proj, CxScreen *screen  );
    // Constructor
    
    int loadHelpText(CxString filePath);
    // load the help text

    void routeKeyAction( CxKeyAction keyAction );
    // route a keyboard action
    
    void redraw( void );
    // redraw the part of the screen with the list
        
    void recalcScreenPlacements(void);
    // recalcs the key places in the window that parts are placed

  private:

    void screenResizeCallback( void );
    // callback to receive host window size updates

    int handleArrows( CxKeyAction keyAction );
    // handle the arrow keys
    
    ProgramDefaults *programDefaults;
    // pointer to the program defaults

    CxScreen *screen;
    // pointer to the screen object
    
    Project *project;
    // handle to the project
    
 
    
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
	//int selectedListItemIndex;
    int maxFirstVisibleListIndex;
    
    MarkUp *markUp;
    // point to the text markup stuff
    
    CxEditBuffer *eb;

};

#endif

