//-------------------------------------------------------------------------------------------------
//
//  Project.h
//  cmacs
//
//  Copyright 2022-2025 Todd Vernon. All rights reserved.
//  Licensed under the Apache License, Version 2.0
//  See LICENSE file for details.
//
//  Project file and subproject management definitions.
//
//-------------------------------------------------------------------------------------------------

#include <stdio.h>

#ifndef Project_h
#define Project_h

#include <cx/base/string.h>
#include <cx/base/slist.h>
#include <cx/base/file.h>
#include <cx/json/json_factory.h>


//-------------------------------------------------------------------------------------------------
// Struct ProjectSubproject
//
// Represents a single subproject within a multi-project setup.
// Each subproject has its own directory, makefile, and file list.
//
//-------------------------------------------------------------------------------------------------
struct ProjectSubproject {
    CxString name;
    CxString directory;      // relative to baseDirectory
    CxString makefile;
    int isDefault;
    int isExpanded;          // UI state for ProjectView

    CxSList<CxString> files; // file paths relative to subproject directory

    ProjectSubproject() : isDefault(0), isExpanded(1) {}
};


//-------------------------------------------------------------------------------------------------
// Class Project
//
//-------------------------------------------------------------------------------------------------

class Project
{
public:

    Project( void );
    // constructor

    int load( CxString fname );
    // load defaults from file

    CxString projectName(void);
    // get the project name

    CxString projectFilePath(void);
    // returns path to .project file

    int numberOfFiles( void );
    // returns the number of files in the project (flat list across all subprojects)

    CxString fileAt( int c );
    // returns the nth file in the project (resolved path)

    // Subproject access
    int subprojectCount(void);
    ProjectSubproject* subprojectAt(int index);
    ProjectSubproject* findSubproject(CxString name);
    ProjectSubproject* getDefaultSubproject(void);

    // Build order access (uses buildOrder if specified, else subproject order)
    int buildOrderCount(void);
    ProjectSubproject* buildOrderAt(int index);

    // Path resolution
    CxString resolveFilePath(ProjectSubproject *sub, CxString filename);
    CxString getMakeDirectory(ProjectSubproject *sub);
    CxString getBaseDirectory(void);

    // Modification
    int save(void);
    void addFileToSubproject(ProjectSubproject *sub, CxString relativeFilename);

private:

    int readFile( CxString fname );
    int parse( void );

	int parseProjectName( CxJSONObject *object );

    // Subproject format parsing
    int parseBaseDirectory( CxJSONObject *object );
    int parseSubprojects( CxJSONObject *root );
    int parseSubproject( CxJSONObject *subObj );
    int parseDisplayOrder( CxJSONObject *root );
    int parseBuildOrder( CxJSONObject *root );
    void reorderSubprojects( void );
    void resolveBaseDirectory( void );
    void buildFlatFileList( void );

    CxString _data;

    CxString _projectName;
    CxString _projectFilePath;
    CxString _baseDirectory;
    CxString _originalBaseDirectory;  // un-resolved value for round-trip save
    CxString _headerComments;         // leading # comment lines from project file

    CxSList< CxString > _fileList;
    CxSList< ProjectSubproject* > _subprojects;
    CxSList< CxString > _displayOrder;
    CxSList< CxString > _buildOrder;

    CxJSONBase *_baseNode;
};

#endif /* Project_h */
