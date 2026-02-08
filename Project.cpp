//-------------------------------------------------------------------------------------------------
//
//  Project.cpp
//  cmacs
//
//  Created by Todd Vernon on 6/24/22.
//  Copyright © 2022 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------

#include "Project.h"


//-------------------------------------------------------------------------------------------------
// {
//      "projectName":"Cm",
//      "projectMakefile":"makefile",
//      "files":{
//          "Cm.cpp",
//          "CommandLineView.cpp",
//          "CommandLineView.h",
//          "EditView.cpp",
//          "EditView.h",
//          "ProgramDefaults.cpp",
//          "ProgramDefaults.h",
//          "ScreenEditor.cpp",
//          "ScreenEditor.h"
//        }
//}
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// Project::Project
//
//
//-------------------------------------------------------------------------------------------------
Project::Project(void)
:   _baseNode( NULL )
{
}


//-------------------------------------------------------------------------------------------------
// Project::parse
//
//
//-------------------------------------------------------------------------------------------------
int
Project::parse( void )
{
    _baseNode = CxJSONFactory::parse( _data );

    if (_baseNode == NULL) {
        return( false );
    }

    if (_baseNode->type() == CxJSONBase::OBJECT) {
        return( true );
    }

    if (_baseNode->type() == CxJSONBase::ARRAY) {
        return( false );
    }

    if (_baseNode->type() == CxJSONBase::NUMBER) {
        return( false );
    }

    if (_baseNode->type() == CxJSONBase::BOOLEAN) {
        return( false );
    }

    if (_baseNode->type() == CxJSONBase::STRING) {
        return( false );
    }

    if (_baseNode->type() == CxJSONBase::JNULL) {
        return( false );
    }

    return( false );
}


//-------------------------------------------------------------------------------------------------
// Project::readFile
//
//
//-------------------------------------------------------------------------------------------------
int
Project::readFile( CxString fname )
{
    CxFile inFile;
    
    if (!inFile.open(fname,"r")) {
        inFile.close();
        return( false );
    }
    
    while (!inFile.eof()) {
        
        CxString line = inFile.getUntil('\n');

        line.stripLeading(" \t");
        line.stripTrailing(" \n\r");

        if (line.firstChar('#') != 0) {
            _data += line;
        }
    }
    
    inFile.close();

    if (_data.length() == 0) return(false);

    return(true);
}


//-------------------------------------------------------------------------------------------------
// Project::load
//
//
// 
//-------------------------------------------------------------------------------------------------
int
Project::load( CxString fname )
{

    //---------------------------------------------------------------------------------------------
    // read the defaults file
    //
    //---------------------------------------------------------------------------------------------
    // Store the file path for later retrieval
    _projectFilePath = fname;

    if (readFile( fname ) == false ) {
        return( false );
    }

    if (parse( ) == false ) {
        return( false );
    }

    //---------------------------------------------------------------------------------------------
    // query the base node for members and sub nodes
    //
    //---------------------------------------------------------------------------------------------
    CxJSONObject *baseItem = (CxJSONObject *) _baseNode;
    if (baseItem) {

        //-----------------------------------------------------------------------------------------
        // make sure base node is an OBJECT type
        //
        //-----------------------------------------------------------------------------------------
        if (baseItem->type() == CxJSONBase::OBJECT) {

            //-------------------------------------------------------------------------------------
            // parse base node objects
            //
            //-------------------------------------------------------------------------------------
            CxJSONObject *object = (CxJSONObject *) baseItem;
			
            parseProjectName( object );
            parseProjectMakefile( object );
            
            //-------------------------------------------------------------------------------------
            // get the files member
            //
            //-------------------------------------------------------------------------------------
			CxJSONMember *filesMember = object->find("files");

            //-------------------------------------------------------------------------------------
            // make sure its there
            //
            //-------------------------------------------------------------------------------------
            if (filesMember != NULL) {

                //---------------------------------------------------------------------------------
                // parse files node object
                //
                //---------------------------------------------------------------------------------
                if (filesMember->object()->type() == CxJSONBase::ARRAY) {

                    //-----------------------------------------------------------------------------
                    // case to an object type
                    //
                    //-----------------------------------------------------------------------------
                    CxJSONArray *filesArray = (CxJSONArray *) filesMember->object();
                    
                    parseFileArray( filesArray );
                    

                 
                }
            }
        }
    }

    return(true);
}


//-------------------------------------------------------------------------------------------------
// Project::parseName
//
//
//-------------------------------------------------------------------------------------------------
int
Project::parseProjectName( CxJSONObject *object )
{
    CxJSONMember *member = object->find("projectName");
    if (member != NULL) {
        if (member->object()->type() == CxJSONBase::STRING) {
                
            CxJSONString *value  = (CxJSONString *) member->object();
            _projectName = value->get();
            

            return( TRUE );
        }
	}

	return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// Project::parseMakefile
//
//
//-------------------------------------------------------------------------------------------------
int
Project::parseProjectMakefile( CxJSONObject *object )
{
    CxJSONMember *member = object->find("projectMakefile");
    if (member != NULL) {
        if (member->object()->type() == CxJSONBase::STRING) {
                
            CxJSONString *value  = (CxJSONString *) member->object();
            
             _projectMakefile = value->get();
            

            
            return( TRUE );
        }
    }

    return( FALSE );

}


//-------------------------------------------------------------------------------------------------
// Project::parseFilesList
//
//
//-------------------------------------------------------------------------------------------------
int
Project::parseFileArray( CxJSONArray *filesArray )
{
    for (int c=0; c<filesArray->entries(); c++) {

        // get the next object in the array
        CxJSONBase *member = filesArray->at(c);
        
        if (member != NULL) {
            
            if (member->type() == CxJSONBase::STRING) {
                
                CxJSONString *value = (CxJSONString *) member;
                
                CxString theFilePath = value->get();
                
                _fileList.append( theFilePath );

            }
        }
    }
    
	return( TRUE );
}


//-------------------------------------------------------------------------------------------------
// Project::name
//
//
//-------------------------------------------------------------------------------------------------
CxString
Project::projectName(void)
{
    return (_projectName );
}


//-------------------------------------------------------------------------------------------------
// Project::makefile
//
//
//-------------------------------------------------------------------------------------------------
CxString
Project::projectMakefile(void)
{
    return (_projectMakefile );
}

int
Project::numberOfFiles(void)
{
    return( _fileList.entries() );
}

CxString
Project::fileAt( int c )
{
    return( _fileList.at( c ));
}


//-------------------------------------------------------------------------------------------------
// Project::projectFilePath
//
// Returns the path to the .project file
//
//-------------------------------------------------------------------------------------------------
CxString
Project::projectFilePath(void)
{
    return (_projectFilePath);
}
