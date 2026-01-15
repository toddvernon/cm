//-------------------------------------------------------------------------------------------------
//
//  Project.h
//  cm
//
//  Created by Todd Vernon on 8/5/23.
//  Copyright © 2022 Todd Vernon. All rights reserved.
//
//-------------------------------------------------------------------------------------------------

#include <stdio.h>

#ifndef Project_h
#define Project_h

#include <cx/base/string.h>
#include <cx/base/file.h>
#include <cx/json/json_factory.h>

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
    // get the tabsize

	CxString projectMakefile(void);
	// is the UI in color
    
    int numberOfFiles( void );
    // returns the number of files in the project
    
    CxString fileAt( int c );
    // returns the n file in the project
    

private:
    
    int readFile( CxString fname );
    int parse( void );

	int parseProjectName( CxJSONObject *object );
	int parseProjectMakefile( CxJSONObject *object );
    int parseFileArray( CxJSONArray *array );

    CxString _data;

    CxString _projectName;
    CxString _projectMakefile;
    
    CxSList< CxString > _fileList;

    CxJSONBase *_baseNode;
};

#endif /* Project_h */
