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
#include <cx/json/json_boolean.h>
#include <stdlib.h>
#include <limits.h>


//-------------------------------------------------------------------------------------------------
// Project file format:
//
// {
//      "projectName":"Cm Development",
//      "baseDirectory":"../..",
//      "displayOrder":["cm", "cx", "cx_tests"],
//      "buildOrder":["cx", "cx_tests", "cm"],
//      "subprojects":[
//          {
//              "name":"cx",
//              "directory":"cx",
//              "makefile":"makefile",
//              "files":[ "base/string.cpp" ]
//          },
//          {
//              "name":"cm",
//              "directory":"cx_apps/cm",
//              "makefile":"makefile",
//              "files":[ "Cm.cpp", "ScreenEditor.cpp" ]
//          }
//      ]
// }
//
// displayOrder: controls subproject display order in ProjectView and startup file selection.
//               If omitted, JSON array order is used.
// buildOrder:   controls build-all sequence (M on "All" in ProjectView).
//               If omitted, JSON array order is used.
//
// Path resolution:
//   baseDirectory is relative to project file location (or absolute)
//   Each subproject directory is relative to baseDirectory
//   Each file path is relative to its subproject directory
//
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// extractDirectory
//
// Returns the directory portion of a file path (everything before the last slash).
// Returns "." if no directory separator found.
//
//-------------------------------------------------------------------------------------------------
static CxString
extractDirectory( CxString filePath )
{
    int lastSlash = -1;
    for (int i = 0; i < (int)filePath.length(); i++) {
        if (filePath.charAt(i) == '/') {
            lastSlash = i;
        }
    }
    if (lastSlash < 0) {
        return CxString(".");
    }
    return filePath.subString(0, lastSlash);
}


//-------------------------------------------------------------------------------------------------
// Project::Project
//
//-------------------------------------------------------------------------------------------------
Project::Project(void)
:   _baseNode( NULL )
{
}


//-------------------------------------------------------------------------------------------------
// Project::parse
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

    return( false );
}


//-------------------------------------------------------------------------------------------------
// Project::readFile
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
// Load project from file.  Expects the multi-subproject format with "subprojects" array.
//
//-------------------------------------------------------------------------------------------------
int
Project::load( CxString fname )
{
    _projectFilePath = fname;

    if (readFile( fname ) == false ) {
        return( false );
    }

    if (parse( ) == false ) {
        return( false );
    }

    CxJSONObject *baseItem = (CxJSONObject *) _baseNode;
    if (baseItem == NULL || baseItem->type() != CxJSONBase::OBJECT) {
        return( false );
    }

    CxJSONObject *object = (CxJSONObject *) baseItem;

    parseProjectName( object );
    parseBaseDirectory( object );
    resolveBaseDirectory();
    parseSubprojects( object );
    parseDisplayOrder( object );
    parseBuildOrder( object );
    reorderSubprojects();
    buildFlatFileList();

    return(true);
}


//-------------------------------------------------------------------------------------------------
// Project::parseProjectName
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
// Project::parseBaseDirectory
//
// Parse the "baseDirectory" string from the root JSON object.
//
//-------------------------------------------------------------------------------------------------
int
Project::parseBaseDirectory( CxJSONObject *object )
{
    CxJSONMember *member = object->find("baseDirectory");
    if (member != NULL) {
        if (member->object()->type() == CxJSONBase::STRING) {
            CxJSONString *value = (CxJSONString *) member->object();
            _baseDirectory = value->get();
            return( TRUE );
        }
    }
    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// Project::resolveBaseDirectory
//
// Resolve _baseDirectory to a usable path by combining with the project file's directory.
// If baseDirectory is absolute, use it directly.
// If relative or empty, combine with the project file's containing directory.
//
//-------------------------------------------------------------------------------------------------
void
Project::resolveBaseDirectory( void )
{
    CxString projectDir = extractDirectory( _projectFilePath );

    if (_baseDirectory.length() > 0) {
        // check if baseDirectory is absolute
        if (_baseDirectory.charAt(0) == '/') {
            // already absolute - use as-is
        } else {
            // relative - combine with project file directory
            _baseDirectory = projectDir + CxString("/") + _baseDirectory;
        }
    } else {
        // no baseDirectory specified - use project file directory
        _baseDirectory = projectDir;
    }
}


//-------------------------------------------------------------------------------------------------
// Project::parseSubprojects
//
// Parse the "subprojects" array from the root JSON object.
//
//-------------------------------------------------------------------------------------------------
int
Project::parseSubprojects( CxJSONObject *root )
{
    CxJSONMember *member = root->find("subprojects");
    if (member == NULL) {
        return( FALSE );
    }

    if (member->object()->type() != CxJSONBase::ARRAY) {
        return( FALSE );
    }

    CxJSONArray *array = (CxJSONArray *) member->object();

    for (int c = 0; c < array->entries(); c++) {

        CxJSONBase *item = array->at(c);

        if (item != NULL && item->type() == CxJSONBase::OBJECT) {
            parseSubproject( (CxJSONObject *) item );
        }
    }

    return( TRUE );
}


//-------------------------------------------------------------------------------------------------
// Project::parseSubproject
//
// Parse a single subproject JSON object and add it to the subprojects list.
//
//-------------------------------------------------------------------------------------------------
int
Project::parseSubproject( CxJSONObject *subObj )
{
    ProjectSubproject *sub = new ProjectSubproject();

    //---------------------------------------------------------------------------------------------
    // parse "name"
    //---------------------------------------------------------------------------------------------
    CxJSONMember *nameMember = subObj->find("name");
    if (nameMember != NULL && nameMember->object()->type() == CxJSONBase::STRING) {
        CxJSONString *value = (CxJSONString *) nameMember->object();
        sub->name = value->get();
    }

    //---------------------------------------------------------------------------------------------
    // parse "directory"
    //---------------------------------------------------------------------------------------------
    CxJSONMember *dirMember = subObj->find("directory");
    if (dirMember != NULL && dirMember->object()->type() == CxJSONBase::STRING) {
        CxJSONString *value = (CxJSONString *) dirMember->object();
        sub->directory = value->get();
    }

    //---------------------------------------------------------------------------------------------
    // parse "makefile"
    //---------------------------------------------------------------------------------------------
    CxJSONMember *makeMember = subObj->find("makefile");
    if (makeMember != NULL && makeMember->object()->type() == CxJSONBase::STRING) {
        CxJSONString *value = (CxJSONString *) makeMember->object();
        sub->makefile = value->get();
    }

    //---------------------------------------------------------------------------------------------
    // parse "default" (optional boolean)
    //---------------------------------------------------------------------------------------------
    CxJSONMember *defaultMember = subObj->find("default");
    if (defaultMember != NULL && defaultMember->object()->type() == CxJSONBase::BOOLEAN) {
        CxJSONBoolean *value = (CxJSONBoolean *) defaultMember->object();
        sub->isDefault = value->get();
    }

    //---------------------------------------------------------------------------------------------
    // parse "files" array
    //---------------------------------------------------------------------------------------------
    CxJSONMember *filesMember = subObj->find("files");
    if (filesMember != NULL && filesMember->object()->type() == CxJSONBase::ARRAY) {
        CxJSONArray *filesArray = (CxJSONArray *) filesMember->object();

        for (int f = 0; f < filesArray->entries(); f++) {
            CxJSONBase *fileItem = filesArray->at(f);
            if (fileItem != NULL && fileItem->type() == CxJSONBase::STRING) {
                CxJSONString *value = (CxJSONString *) fileItem;
                sub->files.append( value->get() );
            }
        }
    }

    _subprojects.append( sub );

    return( TRUE );
}


//-------------------------------------------------------------------------------------------------
// Project::buildFlatFileList
//
// Build the flat _fileList from all subprojects with resolved paths.
// This allows numberOfFiles() and fileAt() to work across all subprojects
// for backwards compatibility with the startup file loading code.
//
//-------------------------------------------------------------------------------------------------
void
Project::buildFlatFileList( void )
{
    for (int s = 0; s < (int)_subprojects.entries(); s++) {
        ProjectSubproject *sub = _subprojects.at(s);

        for (int f = 0; f < (int)sub->files.entries(); f++) {
            CxString resolved = resolveFilePath( sub, sub->files.at(f) );
            _fileList.append( resolved );
        }
    }
}


//-------------------------------------------------------------------------------------------------
// Project::parseDisplayOrder
//
// Parse the optional "displayOrder" array of subproject names from the root JSON object.
//
//-------------------------------------------------------------------------------------------------
int
Project::parseDisplayOrder( CxJSONObject *root )
{
    CxJSONMember *member = root->find("displayOrder");
    if (member == NULL) return FALSE;
    if (member->object()->type() != CxJSONBase::ARRAY) return FALSE;

    CxJSONArray *array = (CxJSONArray *) member->object();
    for (int i = 0; i < array->entries(); i++) {
        CxJSONBase *item = array->at(i);
        if (item != NULL && item->type() == CxJSONBase::STRING) {
            CxJSONString *value = (CxJSONString *) item;
            _displayOrder.append(value->get());
        }
    }
    return TRUE;
}


//-------------------------------------------------------------------------------------------------
// Project::parseBuildOrder
//
// Parse the optional "buildOrder" array of subproject names from the root JSON object.
//
//-------------------------------------------------------------------------------------------------
int
Project::parseBuildOrder( CxJSONObject *root )
{
    CxJSONMember *member = root->find("buildOrder");
    if (member == NULL) return FALSE;
    if (member->object()->type() != CxJSONBase::ARRAY) return FALSE;

    CxJSONArray *array = (CxJSONArray *) member->object();
    for (int i = 0; i < array->entries(); i++) {
        CxJSONBase *item = array->at(i);
        if (item != NULL && item->type() == CxJSONBase::STRING) {
            CxJSONString *value = (CxJSONString *) item;
            _buildOrder.append(value->get());
        }
    }
    return TRUE;
}


//-------------------------------------------------------------------------------------------------
// Project::reorderSubprojects
//
// Reorder _subprojects to match _displayOrder.  If _displayOrder is empty, leave in
// JSON array order.  Any subprojects not mentioned in displayOrder are appended at the end.
//
//-------------------------------------------------------------------------------------------------
void
Project::reorderSubprojects( void )
{
    if (_displayOrder.entries() == 0) return;

    CxSList<ProjectSubproject*> reordered;

    // Add subprojects in displayOrder sequence
    for (int i = 0; i < (int)_displayOrder.entries(); i++) {
        ProjectSubproject *sub = findSubproject(_displayOrder.at(i));
        if (sub != NULL) reordered.append(sub);
    }

    // Append any not mentioned in displayOrder
    for (int s = 0; s < (int)_subprojects.entries(); s++) {
        ProjectSubproject *sub = _subprojects.at(s);
        int found = 0;
        for (int i = 0; i < (int)_displayOrder.entries(); i++) {
            if (sub->name == _displayOrder.at(i)) { found = 1; break; }
        }
        if (!found) reordered.append(sub);
    }

    // Replace (clear removes list nodes without deleting pointed-to objects)
    _subprojects.clear();
    for (int i = 0; i < (int)reordered.entries(); i++) {
        _subprojects.append(reordered.at(i));
    }
}


//-------------------------------------------------------------------------------------------------
// Project::buildOrderCount
//
// Returns the number of subprojects to build, using buildOrder if specified,
// otherwise the subproject count.
//
//-------------------------------------------------------------------------------------------------
int
Project::buildOrderCount(void)
{
    if (_buildOrder.entries() > 0) return (int)_buildOrder.entries();
    return (int)_subprojects.entries();
}


//-------------------------------------------------------------------------------------------------
// Project::buildOrderAt
//
// Returns the subproject at the given build-order index.
// Uses _buildOrder names if specified, otherwise falls back to _subprojects order.
//
//-------------------------------------------------------------------------------------------------
ProjectSubproject*
Project::buildOrderAt(int index)
{
    if (_buildOrder.entries() > 0) {
        if (index < 0 || index >= (int)_buildOrder.entries()) return NULL;
        return findSubproject(_buildOrder.at(index));
    }
    return subprojectAt(index);
}


//-------------------------------------------------------------------------------------------------
// Project::projectName
//
//-------------------------------------------------------------------------------------------------
CxString
Project::projectName(void)
{
    return (_projectName );
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


//-------------------------------------------------------------------------------------------------
// Project::subprojectCount
//
//-------------------------------------------------------------------------------------------------
int
Project::subprojectCount(void)
{
    return (int)_subprojects.entries();
}


//-------------------------------------------------------------------------------------------------
// Project::subprojectAt
//
//-------------------------------------------------------------------------------------------------
ProjectSubproject*
Project::subprojectAt(int index)
{
    if (index < 0 || index >= (int)_subprojects.entries()) {
        return NULL;
    }
    return _subprojects.at(index);
}


//-------------------------------------------------------------------------------------------------
// Project::findSubproject
//
//-------------------------------------------------------------------------------------------------
ProjectSubproject*
Project::findSubproject(CxString name)
{
    for (int i = 0; i < (int)_subprojects.entries(); i++) {
        ProjectSubproject *sub = _subprojects.at(i);
        if (sub->name == name) {
            return sub;
        }
    }
    return NULL;
}


//-------------------------------------------------------------------------------------------------
// Project::getDefaultSubproject
//
// Returns the subproject marked as default, or the first subproject if none marked,
// or NULL if no subprojects exist.
//
//-------------------------------------------------------------------------------------------------
ProjectSubproject*
Project::getDefaultSubproject(void)
{
    // look for one marked as default
    for (int i = 0; i < (int)_subprojects.entries(); i++) {
        ProjectSubproject *sub = _subprojects.at(i);
        if (sub->isDefault) {
            return sub;
        }
    }

    // fall back to first subproject
    if (_subprojects.entries() > 0) {
        return _subprojects.at(0);
    }

    return NULL;
}


//-------------------------------------------------------------------------------------------------
// Project::resolveFilePath
//
// Resolve a filename relative to a subproject's directory.
// Returns: baseDirectory + "/" + sub->directory + "/" + filename
//
//-------------------------------------------------------------------------------------------------
CxString
Project::resolveFilePath(ProjectSubproject *sub, CxString filename)
{
    if (sub == NULL) {
        return filename;
    }

    CxString path = _baseDirectory;

    if (sub->directory.length() > 0) {
        path += CxString("/");
        path += sub->directory;
    }

    path += CxString("/");
    path += filename;

    return path;
}


//-------------------------------------------------------------------------------------------------
// Project::getMakeDirectory
//
// Returns the directory where make should be run for a subproject.
// Returns: baseDirectory + "/" + sub->directory
//
//-------------------------------------------------------------------------------------------------
CxString
Project::getMakeDirectory(ProjectSubproject *sub)
{
    if (sub == NULL) {
        return _baseDirectory;
    }

    CxString path = _baseDirectory;

    if (sub->directory.length() > 0) {
        path += CxString("/");
        path += sub->directory;
    }

    return path;
}


//-------------------------------------------------------------------------------------------------
// Project::getBaseDirectory
//
//-------------------------------------------------------------------------------------------------
CxString
Project::getBaseDirectory(void)
{
    return _baseDirectory;
}
