//-------------------------------------------------------------------------------------------------
//
//  ProgramDefaults.h
//  cmacs
//
//  Created by Todd Vernon on 6/24/22.
//  Copyright © 2022 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------

#include <stdio.h>

#ifndef ProgramDefaults_h
#define ProgramDefaults_h

#include <cx/base/string.h>
#include <cx/base/file.h>
#include <cx/json/json_factory.h>
#include <cx/screen/color.h>

//-------------------------------------------------------------------------------------------------
// Class ProgramDefaults
//
//-------------------------------------------------------------------------------------------------

class ProgramDefaults
{
public:
    
    ProgramDefaults( void );
    // constructor
    
    int loadDefaults( CxString fname );
    // load defaults from file
    
    int getTabSize(void);
    // get the tabsize

	int inColor(void);
	// is the UI in color
    
    CxColor *statusBarTextColor(void);
    CxColor *statusBarBackgroundColor(void);
    CxColor *commentTextColor(void);
    CxColor *includeTextColor(void);
    CxColor *lineNumberTextColor(void);
	CxColor *commandLineMessageTextColor(void);
    
    CxColor *cppLanguageMethodDefinitionTextColor(void);
    CxColor *cppLanguageElementsTextColor(void);
    CxColor *cppLanguageTypesTextColor(void);
    
    CxString getStatusBarForegroundColor(void);
    // get the status bar foreground color
    
    CxString getStatusBarBackgroundColor(void);
    // get the status bar background color
    
    int jumpScroll(void);
    // see if we should use jump scrolling
    
    int showLineNumbers(void);
	// should the app show line numbers by default
    
	int autoSaveOnBufferChange(void);
	// should the app autosave the file when changing screen to next buffer

    void writeDefaults(CxString fname);
	// write out default values to ~/.cmrc
    
    int colorizeSyntax(void);
    // return of syntax parsing is active
    
    int liveStatusLine(void);
    // should the status line have the current cursor position and percentage

private:
    
    int readFile( CxString fname );
    int parse( void );
    
    int parseColorizeSyntax( CxJSONObject *object );

	int parseTabs( CxJSONObject *object );
	int parseJumpScroll( CxJSONObject *object );
    int parseShowLineNumbers( CxJSONObject *object );
	int parseAutoSaveOnBufferChange( CxJSONObject *object );
    int parseLiveStatusLine( CxJSONObject *object );
    
    int parseCommentTextColor( CxJSONObject *object );
    int parseStatusBarTextColor( CxJSONObject *object );
    int parseStatusBarBackgroundColor( CxJSONObject *object );
    int parseIncludeTextColor( CxJSONObject *object );
    int parseLineNumberTextColor( CxJSONObject *object );
    int parseCommandLineMessageTextColor( CxJSONObject *object );
    int parseCppLanguageTypesTextColor( CxJSONObject *object );
    int parseCppLanguageElementsTextColor( CxJSONObject *object );
    int parseCppLanguageMethodDefinitionTextColor( CxJSONObject *object );
    
    
    static CxColor *parseForegroundColor( CxString colorName );
    static CxColor *parseBackgroundColor( CxString colorName );
    
    CxString _data;
        
    CxColor *_statusBarTextColor;
    CxColor *_statusBarBackgroundColor;
    CxColor *_commentTextColor;
    CxColor *_includeTextColor;
    CxColor *_lineNumberTextColor;
	CxColor *_commandLineMessageTextColor;

    CxColor *_cppLanguageMethodDefinitionTextColor;
    CxColor *_cppLanguageElementsTextColor;
    CxColor *_cppLanguageTypesTextColor;

	int _showLineNumbers;
    int _jumpscroll;
    int _tabSize;
	int _color;
	int _autoSaveOnBufferChange;
    int _colorizeSyntax;
    int _liveStatusLine;

    CxJSONBase *_baseNode;
};

#endif /* ProgramDefaults_hpp */
