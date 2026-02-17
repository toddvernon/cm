//-------------------------------------------------------------------------------------------------
//
//  ProjectView.h
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  Modal project/buffer list view definitions.
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


#ifndef _ProjectView_h_
#define _ProjectView_h_


//-------------------------------------------------------------------------------------------------
// ProjectViewItemType
//
// Type of item in the visible items list.
//
//-------------------------------------------------------------------------------------------------
enum ProjectViewItemType {
    PVITEM_ALL,           // "All" row at top
    PVITEM_SUBPROJECT,    // Subproject header
    PVITEM_FILE,          // File under a subproject
    PVITEM_OPEN_HEADER,   // "Open Files" section header
    PVITEM_OPEN_FILE,     // Buffer not in any subproject
    PVITEM_SEPARATOR      // Visual separator line between sections
};


//-------------------------------------------------------------------------------------------------
// ProjectViewItem
//
// A single visible row in the project view.
//
//-------------------------------------------------------------------------------------------------
struct ProjectViewItem {
    ProjectViewItemType type;
    int subprojectIndex;     // -1 for ALL, 0+ for subprojects/files
    int fileIndex;           // -1 for ALL and headers, 0+ for files
    int bufferIndex;         // index into editBufferList (-1 if N/A)
    int isModified;          // cached: buffer has unsaved changes
    int isInMemory;          // cached: buffer is loaded in memory
    int hasModifiedFile;     // cached: subproject has modified files (for headers)
    CxString formattedText;  // pre-computed display text (with padding)
};


//---------------------------------------------------------------------------------------------------------
//
//  ProjectView
//
//  Modal dialog showing project subprojects with expand/collapse and file navigation.
//
//---------------------------------------------------------------------------------------------------------
class ProjectView
{
  public:

    ProjectView( ProgramDefaults *pd, CmEditBufferList *ebl, Project *proj, CxScreen *screen );
    // Constructor

    void routeKeyAction( CxKeyAction keyAction );
    // route a keyboard action

    void redraw( void );
    // redraw the part of the screen with the list

    void recalcScreenPlacements(void);
    // recalcs the key places in the window that parts are placed

    CxString getSelectedItem( void );
    // get resolved file path for selected file item, empty string if not a file

    ProjectViewItemType getSelectedItemType( void );
    // get the type of the currently selected item

    ProjectSubproject* getSelectedSubproject( void );
    // get the subproject for the current selection (NULL if "All" selected)

    void toggleSelectedSubproject( void );
    // toggle expand/collapse of the selected subproject header

    void setVisible( int visible );
    // set visibility state for resize handling

    void rebuildVisibleItems( void );
    // rebuild the flat list of visible items from project structure

    CxString getContextFooter( void );
    // build footer string based on currently selected item type

  private:

    int isProjectFilePath( CxString path );
    // check if a path belongs to any subproject in the project

    int subprojectHasModifiedFile( ProjectSubproject *sub );
    // check if any file in the subproject has unsaved changes

    int handleArrows( CxKeyAction keyAction );
    // handle the arrow keys

    int reframe( void );
    // make sure selection is visible in list

    void redrawLine( int logicalIndex, int isSelected );
    // redraw a single content line

    void redrawFooter( void );
    // redraw just the footer

    CmEditBufferList *editBufferList;
    // pointer to the list of files being edited (for buffer status checks)

    ProgramDefaults *programDefaults;
    // pointer to the program defaults

    CxScreen *screen;
    // pointer to the screen object

    CxBoxFrame *frame;
    // box frame for modal display

    Project *project;

    // flat list of visible items (rebuilt on expand/collapse)
    CxSList<ProjectViewItem*> _visibleItems;

    // screen placement values (zero based)
    int  screenNumberOfLines;
    int  screenNumberOfCols;

    int  screenProjectTitleBarLine;
    int  screenProjectFrameLine;
    int  screenProjectNumberOfLines;
	int  screenProjectNumberOfCols;
    int  screenProjectFirstListLine;
    int  screenProjectLastListLine;

    // scrolling and selection
    int firstVisibleListIndex;
	int selectedListItemIndex;

    int _visible;  // whether modal is currently displayed

    int _otherFilesExpanded;  // expand/collapse state for "Other Files" section

    // pre-built strings for efficient redraw (built in recalcScreenPlacements)
    CxString _paddingSpaces;   // spaces for padding lines
    CxString _separatorLine;   // pre-built separator line
    CxString _emptyLine;       // pre-built empty line
    int _cachedContentWidth;   // content width when strings were built

    CxString _lastFooter;      // cached footer for change detection

};

#endif
