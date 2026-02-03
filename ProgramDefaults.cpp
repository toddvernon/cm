//-------------------------------------------------------------------------------------------------
//
//  ProgramDefaults.cpp
//  cmacs
//
//  Created by Todd Vernon on 6/24/22.
//  Copyright © 2022 Todd Vernon. All rights reserved.
//
//
//-------------------------------------------------------------------------------------------------

#include "ProgramDefaults.h"


//-------------------------------------------------------------------------------------------------
// {
//     "tabs": 4,
//     "showLineNumbers":true,
//     "jumpscroll": true,
//     "colors": {
//        "statusBarTextColor":"BRIGHT_WHITE",
//        "statusBarBackgroundColor":"BRIGHT_BLUE",
//        "commentTextColor":"NONE",
//        "includeTextColor":"NONE",
//        "lineNumberTextColor":"WHITE"
//     }
// }
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::ProgramDefaults
//
//
//-------------------------------------------------------------------------------------------------
ProgramDefaults::ProgramDefaults(void)
:   _baseNode( NULL ),
    _tabSize(4),
    _jumpscroll(FALSE),
	_showLineNumbers(TRUE),
	_autoSaveOnBufferChange(FALSE),
    _colorizeSyntax(FALSE),
    _liveStatusLine(FALSE)
{
    _statusBarTextColor          = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    _statusBarBackgroundColor    = new CxAnsiBackgroundColor( CxAnsiForegroundColor::NONE );
    _lineNumberTextColor         = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
	_commandLineMessageTextColor = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );

    // legacy colors for backward compatibility
    _commentTextColor            = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    _includeTextColor            = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    _cppLanguageMethodDefinitionTextColor = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    _cppLanguageElementsTextColor = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    _cppLanguageTypesTextColor = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );

    // initialize per-language syntax color sets
    for (int i = 0; i < LANG_COUNT; i++) {
        initSyntaxColorSet( &_syntaxColors[i] );
    }
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parse
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parse( void )
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
// ProgramDefaults::readFile
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::readFile( CxString fname )
{
    CxFile inFile;
    
    if (!inFile.open(fname,"r")) {
        
        inFile.close();
        
        writeDefaults(fname);
        
        if( !inFile.open(fname,"r")) {
            return( false );
        }
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
// ProgramDefaults::loadDefaults
//
//
// 
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::loadDefaults( CxString fname )
{

    //---------------------------------------------------------------------------------------------
    // read the defaults file
    //
    //---------------------------------------------------------------------------------------------
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
			parseTabs( object );
			parseJumpScroll( object );
            parseShowLineNumbers( object );
            parseAutoSaveOnBufferChange( object );
            parseColorizeSyntax( object );
            parseLiveStatusLine( object );
            
            //-------------------------------------------------------------------------------------
            // get the colors member
            //
            //-------------------------------------------------------------------------------------
			CxJSONMember *colorsMember = object->find("colors");

            //-------------------------------------------------------------------------------------
            // make sure its there
            //
            //-------------------------------------------------------------------------------------
            if (colorsMember != NULL) {

                //---------------------------------------------------------------------------------
                // parse color node object
                //
                //---------------------------------------------------------------------------------
                if (colorsMember->object()->type() == CxJSONBase::OBJECT) {

                    //-----------------------------------------------------------------------------
                    // case to an object type
                    //
                    //-----------------------------------------------------------------------------
                    CxJSONObject *colorObject = (CxJSONObject *) colorsMember->object();
                    parseCommentTextColor( colorObject );
                    parseIncludeTextColor( colorObject );
                    parseStatusBarTextColor( colorObject );
                    parseStatusBarBackgroundColor( colorObject );
                    parseIncludeTextColor( colorObject );
                    parseLineNumberTextColor( colorObject );
                    parseCommandLineMessageTextColor( colorObject );
                    parseCppLanguageTypesTextColor( colorObject );
                    parseCppLanguageElementsTextColor( colorObject );
                    parseCppLanguageMethodDefinitionTextColor( colorObject );

                }
            }

            //-------------------------------------------------------------------------------------
            // parse syntaxColors section (new format)
            //
            //-------------------------------------------------------------------------------------
            parseSyntaxColors( object );
        }
    }

    return(true);
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseTabs
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseTabs( CxJSONObject *object )
{
    CxJSONMember *member = object->find("tabs");
    if (member != NULL) {
        if (member->object()->type() == CxJSONBase::NUMBER) {
                
            CxJSONNumber *value  = (CxJSONNumber *) member->object();
            _tabSize = (int) value->get();

            if ((_tabSize != 2) && (_tabSize !=4 ) && (_tabSize != 8)) {
                _tabSize = 4;
                return( TRUE );
            }
        }
	}

	return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseAutoSaveOnBufferChange
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseAutoSaveOnBufferChange( CxJSONObject *object )
{
    CxJSONMember *member = object->find("autoSaveOnBufferChange");
    if (member != NULL) {
        if (member->object()->type() == CxJSONBase::BOOLEAN) {
            CxJSONBoolean *value  = (CxJSONBoolean *) member->object();
            _autoSaveOnBufferChange = value->get();
			return( TRUE );
      	}
 	}

	return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseLiveStatusLine
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseLiveStatusLine( CxJSONObject *object )
{
    CxJSONMember *member = object->find("liveStatusLine");
    if (member != NULL) {
        if (member->object()->type() == CxJSONBase::BOOLEAN) {
            CxJSONBoolean *value  = (CxJSONBoolean *) member->object();
            _liveStatusLine = value->get();
            return( TRUE );
          }
     }

    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseColorizeSyntax
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseColorizeSyntax( CxJSONObject *object )
{
    CxJSONMember *member = object->find("colorizeSyntax");
    if (member != NULL) {
        if (member->object()->type() == CxJSONBase::BOOLEAN) {
            CxJSONBoolean *value  = (CxJSONBoolean *) member->object();
            _colorizeSyntax = value->get();
            return( TRUE );
          }
     }

    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseJumpScroll
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseJumpScroll( CxJSONObject *object )
{
    CxJSONMember *member = object->find("jumpscroll");
    if (member != NULL) {
        if (member->object()->type() == CxJSONBase::BOOLEAN) {
            CxJSONBoolean *value  = (CxJSONBoolean *) member->object();
            _jumpscroll = value->get();
			return( TRUE );
      	}
 	}

	return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseShowLineNumbers
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseShowLineNumbers( CxJSONObject *object)
{
    CxJSONMember *member = object->find("showLineNumbers");
    if (member != NULL) {
        if (member->object()->type() == CxJSONBase::BOOLEAN) {
            CxJSONBoolean *value  = (CxJSONBoolean *) member->object();
            _showLineNumbers = value->get();
            return( TRUE );
        }
    }
    return(FALSE);
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseCommentTextColor
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseCommentTextColor( CxJSONObject *colorObject )
{
    CxJSONMember *commentTextColorMember =
        colorObject->find("commentTextColor");

    if (commentTextColorMember != NULL ) {
        if (commentTextColorMember->object()->type() == CxJSONBase::STRING) {

            CxJSONString *value  =
                (CxJSONString *) commentTextColorMember->object();

            CxString commentTextColorName = value->get();
            
            _commentTextColor =
                ProgramDefaults::parseForegroundColor( commentTextColorName );
            
            return( TRUE );
        }
    }
    
    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseStatusBarTextColor
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseStatusBarTextColor( CxJSONObject *colorObject )
{
    CxJSONMember *statusBarTextColorMember =
        colorObject->find("statusBarTextColor");

    if ( statusBarTextColorMember != NULL ) {
        
        if (statusBarTextColorMember->object()->type() == CxJSONBase::STRING) {

            CxJSONString *value  =
                (CxJSONString *) statusBarTextColorMember->object();

            CxString statusBarTextColorName = value->get();
            
            _statusBarTextColor =
                ProgramDefaults::parseForegroundColor( statusBarTextColorName );

            return (TRUE );
        }
    }
    
    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseStatusBarBackgroundColor
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseStatusBarBackgroundColor( CxJSONObject *colorObject )
{
    CxJSONMember *statusBarBackgroundColorMember =
        colorObject->find("statusBarBackgroundColor");

    if ( statusBarBackgroundColorMember != NULL ) {

        if (statusBarBackgroundColorMember->object()->type() == CxJSONBase::STRING) {

            CxJSONString *value  =
                (CxJSONString *) statusBarBackgroundColorMember->object();

            CxString statusBarBackgroundColorName = value->get();
            
            _statusBarBackgroundColor =
                ProgramDefaults::parseBackgroundColor( statusBarBackgroundColorName );
            
            return( TRUE );
        }
    }
    
    return( FALSE );    
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseIncludeTextColor
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseIncludeTextColor( CxJSONObject *colorObject )
{
    CxJSONMember *includeTextColorMember =
        colorObject->find("includeTextColor");

    if ( includeTextColorMember != NULL ) {

        if (includeTextColorMember->object()->type() == CxJSONBase::STRING) {

            CxJSONString *value  =
                (CxJSONString *) includeTextColorMember->object();

            CxString includeTextColorName = value->get();
            
            _includeTextColor =
                ProgramDefaults::parseForegroundColor( includeTextColorName );
            
            return( TRUE );
        }
    }
    
    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseLineNumberTextColor
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseLineNumberTextColor( CxJSONObject *colorObject )
{
    CxJSONMember *lineNumberTextColorMember =
        colorObject->find("lineNumberTextColor");

    if ( lineNumberTextColorMember != NULL ) {

        if (lineNumberTextColorMember->object()->type() == CxJSONBase::STRING) {

            CxJSONString *value  =
                (CxJSONString *) lineNumberTextColorMember->object();

            CxString lineNumberTextColorName = value->get();
            
            _lineNumberTextColor =
                ProgramDefaults::parseForegroundColor( lineNumberTextColorName );
            
            return( TRUE );
        }
    }
    
    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseCommandLineMessageTextColor
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseCommandLineMessageTextColor( CxJSONObject *colorObject )
{
    CxJSONMember *commandLineMessageTextColorMember =
        colorObject->find("commandLineMessageTextColor");

    if ( commandLineMessageTextColorMember != NULL ) {

        if (commandLineMessageTextColorMember->object()->type() == CxJSONBase::STRING) {

            CxJSONString *value  =
                (CxJSONString *) commandLineMessageTextColorMember->object();

            CxString commandLineMessageTextColorName = value->get();
            
            _commandLineMessageTextColor =
                ProgramDefaults::parseForegroundColor( commandLineMessageTextColorName );
            
            return( TRUE );
        }
    }
    
    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseCppLanguageTypesTextColor
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseCppLanguageTypesTextColor( CxJSONObject *colorObject )
{
    CxJSONMember *_cppLanguageTypesTextColorMember =
        colorObject->find("cppLanguageTypesTextColor");


    if ( _cppLanguageTypesTextColorMember != NULL ) {

        if (_cppLanguageTypesTextColorMember->object()->type() == CxJSONBase::STRING) {

            CxJSONString *value  =
                (CxJSONString *) _cppLanguageTypesTextColorMember->object();

            CxString cppLanguageTypesTextColorName = value->get();
           
            _cppLanguageTypesTextColor =
                ProgramDefaults::parseForegroundColor( cppLanguageTypesTextColorName );
            
            return( TRUE );
        }
    }
    
    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseCppLanguageElementsTextColor
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseCppLanguageElementsTextColor( CxJSONObject *colorObject )
{
    CxJSONMember *_cppLanguageElementsTextColorMember =
        colorObject->find("cppLanguageElementsTextColor");

    if ( _cppLanguageElementsTextColorMember != NULL ) {

        if (_cppLanguageElementsTextColorMember->object()->type() == CxJSONBase::STRING) {

            CxJSONString *value  =
                (CxJSONString *) _cppLanguageElementsTextColorMember->object();

            CxString cppLanguageElementsTextColorName = value->get();
            
            _cppLanguageElementsTextColor =
                ProgramDefaults::parseForegroundColor( cppLanguageElementsTextColorName );
        }
        
        return( TRUE );
    }
    
    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseCppLanguageMethodDefinitionTextColor
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseCppLanguageMethodDefinitionTextColor( CxJSONObject *colorObject )
{
    CxJSONMember *cppLanguageMethodDefinitionTextColorMember =
        colorObject->find("cppLanguageMethodDefinitionTextColor");

    if ( cppLanguageMethodDefinitionTextColorMember != NULL ) {

        if (cppLanguageMethodDefinitionTextColorMember->object()->type() == CxJSONBase::STRING) {

            CxJSONString *value  =
                (CxJSONString *) cppLanguageMethodDefinitionTextColorMember->object();

            CxString cppLanguageMethodDefinitionTextColorName = value->get();
            
            _cppLanguageMethodDefinitionTextColor =
                ProgramDefaults::parseForegroundColor( cppLanguageMethodDefinitionTextColorName );
            
            return( TRUE );
        }
    }
    
    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseColor
//
//
//-------------------------------------------------------------------------------------------------
/* static */
CxColor *
ProgramDefaults::parseForegroundColor( CxString colorString )
{
    // if there is no colon that the color selector is not present and the
    // hte none color is assigned and returned
    if (colorString.firstChar(":") == -1) {
        CxAnsiColor *color = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
        return ( color );
    }
    
    // there is some string before the colon so we assume that is a color
    // type selector and grab it
    CxString colorTypeSelector = colorString.nextToken(" :\t\n\r");
    
    //printf("colorTypeSelector = %s\n", colorTypeSelector.data() );
    
    // proactively get the next three arguments.  There might be three however
    CxString item1 = colorString.nextToken(" ,\n\r\t:");
    CxString item2 = colorString.nextToken(" ,\n\r\t:");
    CxString item3 = colorString.nextToken(" ,\n\r\t:");
    
    //printf("item1 = %s\n", item1.data() );
    //printf("item2 = %s\n", item2.data() );
    //printf("item3 = %s\n", item3.data() );
    
    // if the color type selector is empty
    if (colorTypeSelector.length() == 0) {

        //printf("invalid selector\n");

        CxAnsiColor *color = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
        return ( color );
    }
    
    // Now test the color type selector for type ANSI
    if (CxString::toUpper(colorTypeSelector) == "ANSI") {
        
        //printf("ansi value = %s\n", item1.data());

        CxAnsiColor *color = new CxAnsiForegroundColor( item1 );
        return ( color );
    }
    
    // Now test the color type selector for the type XTERM256
    if (CxString::toUpper(colorTypeSelector) == "XTERM256") {
        
        //printf("xterm256 value = %s\n", item1.data());
        
        CxXterm256Color *color = new CxXterm256ForegroundColor( item1 );
        return( color );
    }
    
    // Now test the color type selector for RBG
    if (CxString::toUpper(colorTypeSelector) == "RGB") {
        
        // item is expected to be an rgb value
        int red   = item1.toInt();
        int green = item2.toInt();
        int blue  = item3.toInt();
        
        // create the color
        CxRGBColor *color = new CxRGBForegroundColor( red, green, blue );
        
        return( color );
    }
    
    // something went wrong somewhere so just return the none foreground color
    CxAnsiColor *color = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    return ( color );
}



//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseColor
//
//
//-------------------------------------------------------------------------------------------------
/* static */
CxColor *
ProgramDefaults::parseBackgroundColor( CxString colorString )
{
    // if there is no colon that the color selector is not present and the
    // hte none color is assigned and returned
    if (colorString.firstChar(":") == -1) {
        CxAnsiColor *color = new CxAnsiBackgroundColor( CxAnsiBackgroundColor::NONE );
        return ( color );
    }
    
    // there is some string before the colon so we assume that is a color
    // type selector and grab it
    CxString colorTypeSelector = colorString.nextToken(" :\t\n\r");
    
    //printf("colorTypeSelector = %s\n", colorTypeSelector.data() );
    
    // proactively get the next three arguments.  There might be three however
    CxString item1 = colorString.nextToken(" ,\n\r\t:");
    CxString item2 = colorString.nextToken(" ,\n\r\t:");
    CxString item3 = colorString.nextToken(" ,\n\r\t:");
    
    //printf("item1 = %s\n", item1.data() );
    //printf("item2 = %s\n", item2.data() );
    //printf("item3 = %s\n", item3.data() );
    
    // if the color type selector is empty
    if (colorTypeSelector.length() == 0) {

        //printf("invalid selector\n");

        CxAnsiColor *color = new CxAnsiBackgroundColor( CxAnsiBackgroundColor::NONE );
        return ( color );
    }
    
    // Now test the color type selector for type ANSI
    if (CxString::toUpper(colorTypeSelector) == "ANSI") {
        
        //printf("ansi value = %s\n", item1.data());

        CxAnsiColor *color = new CxAnsiBackgroundColor( item1 );
        return ( color );
    }
    
    // Now test the color type selector for the type XTERM256
    if (CxString::toUpper(colorTypeSelector) == "XTERM256") {
        
        //printf("xterm256 value = %s\n", item1.data());
        
        CxXterm256Color *color = new CxXterm256BackgroundColor( item1 );
        return( color );
    }
    
    // Now test the color type selector for RBG
    if (CxString::toUpper(colorTypeSelector) == "RGB") {
        
        // item is expected to be an rgb value
        int red   = item1.toInt();
        int green = item2.toInt();
        int blue  = item3.toInt();
        
        // create the color
        CxRGBColor *color = new CxRGBBackgroundColor( red, green, blue );
        
        return( color );
    }
    
    // something went wrong somewhere so just return the none foreground color
    CxAnsiColor *color = new CxAnsiForegroundColor( CxAnsiBackgroundColor::NONE );
    return ( color );
}





//-------------------------------------------------------------------------------------------------
// ProgramDefaults::liveStatusLine
//
//
//-------------------------------------------------------------------------------------------------

int
ProgramDefaults::liveStatusLine(void)
{
    return (_liveStatusLine );
}

//-------------------------------------------------------------------------------------------------
// ProgramDefaults::colorizeSyntax
//
//
//-------------------------------------------------------------------------------------------------

int
ProgramDefaults::colorizeSyntax(void)
{
    return( _colorizeSyntax );
}

//-------------------------------------------------------------------------------------------------
// ProgramDefaults::getTabSize
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::getTabSize(void)
{
    return (_tabSize );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::getTabSize
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::jumpScroll(void)
{
    return (_jumpscroll );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::showLineNumbers
//
//
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::showLineNumbers(void)
{
    return (_showLineNumbers );
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CxColor *
ProgramDefaults::statusBarTextColor(void)
{
    return( _statusBarTextColor );
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CxColor *
ProgramDefaults::statusBarBackgroundColor(void)
{
    return( _statusBarBackgroundColor );
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CxColor *
ProgramDefaults::commentTextColor(void)
{
    return( _commentTextColor );
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CxColor *
ProgramDefaults::includeTextColor(void)
{
    return( _includeTextColor );
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CxColor *
ProgramDefaults::lineNumberTextColor(void)
{
    return( _lineNumberTextColor );
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CxColor *
ProgramDefaults::commandLineMessageTextColor(void)
{
    return( _commandLineMessageTextColor );
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
CxColor *
ProgramDefaults::cppLanguageMethodDefinitionTextColor( void )
{
    return( _cppLanguageMethodDefinitionTextColor );
}


CxColor *
ProgramDefaults::cppLanguageElementsTextColor( void )
{
    return( _cppLanguageElementsTextColor);
}


CxColor *
ProgramDefaults::cppLanguageTypesTextColor( void )
{
    return( _cppLanguageTypesTextColor );
}

// should the app autosave the file when changing screen to next buffer
int
ProgramDefaults::autoSaveOnBufferChange(void)
{
    return( _autoSaveOnBufferChange );

}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::initSyntaxColorSet
//
// Initialize a syntax color set to NONE colors
//-------------------------------------------------------------------------------------------------
void
ProgramDefaults::initSyntaxColorSet( SyntaxColorSet *colorSet )
{
    colorSet->commentTextColor = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    colorSet->includeTextColor = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    colorSet->keywordTextColor = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    colorSet->typeTextColor = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    colorSet->constantTextColor = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    colorSet->stringTextColor = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    colorSet->methodDefinitionTextColor = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    colorSet->numberTextColor = new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::copySyntaxColorSet
//
// Copy colors from one set to another (for inheriting from default)
//-------------------------------------------------------------------------------------------------
void
ProgramDefaults::copySyntaxColorSet( SyntaxColorSet *dest, SyntaxColorSet *src )
{
    dest->commentTextColor = src->commentTextColor;
    dest->includeTextColor = src->includeTextColor;
    dest->keywordTextColor = src->keywordTextColor;
    dest->typeTextColor = src->typeTextColor;
    dest->constantTextColor = src->constantTextColor;
    dest->stringTextColor = src->stringTextColor;
    dest->methodDefinitionTextColor = src->methodDefinitionTextColor;
    dest->numberTextColor = src->numberTextColor;
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseSyntaxColors
//
// Parse the syntaxColors section with language-specific color sets
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseSyntaxColors( CxJSONObject *object )
{
    CxJSONMember *syntaxColorsMember = object->find("syntaxColors");
    if (syntaxColorsMember == NULL) {
        return( FALSE );
    }

    if (syntaxColorsMember->object()->type() != CxJSONBase::OBJECT) {
        return( FALSE );
    }

    CxJSONObject *syntaxColorsObject = (CxJSONObject *) syntaxColorsMember->object();

    // parse "default" color set first (index 0 = LANG_NONE used as default)
    CxJSONMember *defaultMember = syntaxColorsObject->find("default");
    if (defaultMember != NULL && defaultMember->object()->type() == CxJSONBase::OBJECT) {
        CxJSONObject *defaultColors = (CxJSONObject *) defaultMember->object();
        parseSyntaxColorSet( defaultColors, 0 );
    }

    // copy default colors to all language slots
    for (int i = 1; i < LANG_COUNT; i++) {
        copySyntaxColorSet( &_syntaxColors[i], &_syntaxColors[0] );
    }

    // parse language-specific overrides
    // language indices must match LanguageMode enum in MarkUp.h:
    // 1=C, 2=CPP, 3=SWIFT, 4=PYTHON, 5=JAVASCRIPT, 6=GO, 7=RUST, 8=JAVA, 9=SHELL, etc.

    struct { const char *name; int index; } langMap[] = {
        { "c",          1 },
        { "cpp",        2 },
        { "swift",      3 },
        { "python",     4 },
        { "javascript", 5 },
        { "go",         6 },
        { "rust",       7 },
        { "java",       8 },
        { "shell",      9 },
        { "makefile",  10 },
        { "html",      11 },
        { "css",       12 },
        { "json",      13 },
        { "markdown",  14 },
        { NULL,         0 }
    };

    for (int i = 0; langMap[i].name != NULL; i++) {
        CxJSONMember *langMember = syntaxColorsObject->find( langMap[i].name );
        if (langMember != NULL && langMember->object()->type() == CxJSONBase::OBJECT) {
            CxJSONObject *langColors = (CxJSONObject *) langMember->object();
            parseSyntaxColorSet( langColors, langMap[i].index );
        }
    }

    return( TRUE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseSyntaxColorSet
//
// Parse a single color set into the specified language index
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseSyntaxColorSet( CxJSONObject *colorSet, int langIndex )
{
    if (langIndex < 0 || langIndex >= LANG_COUNT) {
        return( FALSE );
    }

    SyntaxColorSet *colors = &_syntaxColors[langIndex];
    CxJSONMember *member;
    CxJSONString *value;

    // commentTextColor
    member = colorSet->find("commentTextColor");
    if (member != NULL && member->object()->type() == CxJSONBase::STRING) {
        value = (CxJSONString *) member->object();
        colors->commentTextColor = ProgramDefaults::parseForegroundColor( value->get() );
    }

    // includeTextColor
    member = colorSet->find("includeTextColor");
    if (member != NULL && member->object()->type() == CxJSONBase::STRING) {
        value = (CxJSONString *) member->object();
        colors->includeTextColor = ProgramDefaults::parseForegroundColor( value->get() );
    }

    // keywordTextColor
    member = colorSet->find("keywordTextColor");
    if (member != NULL && member->object()->type() == CxJSONBase::STRING) {
        value = (CxJSONString *) member->object();
        colors->keywordTextColor = ProgramDefaults::parseForegroundColor( value->get() );
    }

    // typeTextColor
    member = colorSet->find("typeTextColor");
    if (member != NULL && member->object()->type() == CxJSONBase::STRING) {
        value = (CxJSONString *) member->object();
        colors->typeTextColor = ProgramDefaults::parseForegroundColor( value->get() );
    }

    // constantTextColor
    member = colorSet->find("constantTextColor");
    if (member != NULL && member->object()->type() == CxJSONBase::STRING) {
        value = (CxJSONString *) member->object();
        colors->constantTextColor = ProgramDefaults::parseForegroundColor( value->get() );
    }

    // methodDefinitionTextColor
    member = colorSet->find("methodDefinitionTextColor");
    if (member != NULL && member->object()->type() == CxJSONBase::STRING) {
        value = (CxJSONString *) member->object();
        colors->methodDefinitionTextColor = ProgramDefaults::parseForegroundColor( value->get() );
    }

    // stringTextColor
    member = colorSet->find("stringTextColor");
    if (member != NULL && member->object()->type() == CxJSONBase::STRING) {
        value = (CxJSONString *) member->object();
        colors->stringTextColor = ProgramDefaults::parseForegroundColor( value->get() );
    }

    // numberTextColor
    member = colorSet->find("numberTextColor");
    if (member != NULL && member->object()->type() == CxJSONBase::STRING) {
        value = (CxJSONString *) member->object();
        colors->numberTextColor = ProgramDefaults::parseForegroundColor( value->get() );
    }

    return( TRUE );
}


//-------------------------------------------------------------------------------------------------
// Per-language syntax color accessors
//-------------------------------------------------------------------------------------------------
CxColor *
ProgramDefaults::keywordTextColor( int lang )
{
    if (lang < 0 || lang >= LANG_COUNT) lang = 0;
    return( _syntaxColors[lang].keywordTextColor );
}

CxColor *
ProgramDefaults::typeTextColor( int lang )
{
    if (lang < 0 || lang >= LANG_COUNT) lang = 0;
    return( _syntaxColors[lang].typeTextColor );
}

CxColor *
ProgramDefaults::constantTextColor( int lang )
{
    if (lang < 0 || lang >= LANG_COUNT) lang = 0;
    return( _syntaxColors[lang].constantTextColor );
}

CxColor *
ProgramDefaults::stringTextColor( int lang )
{
    if (lang < 0 || lang >= LANG_COUNT) lang = 0;
    return( _syntaxColors[lang].stringTextColor );
}

CxColor *
ProgramDefaults::methodDefinitionTextColor( int lang )
{
    if (lang < 0 || lang >= LANG_COUNT) lang = 0;
    return( _syntaxColors[lang].methodDefinitionTextColor );
}

CxColor *
ProgramDefaults::commentTextColor( int lang )
{
    if (lang < 0 || lang >= LANG_COUNT) lang = 0;
    return( _syntaxColors[lang].commentTextColor );
}

CxColor *
ProgramDefaults::includeTextColor( int lang )
{
    if (lang < 0 || lang >= LANG_COUNT) lang = 0;
    return( _syntaxColors[lang].includeTextColor );
}

CxColor *
ProgramDefaults::numberTextColor( int lang )
{
    if (lang < 0 || lang >= LANG_COUNT) lang = 0;
    return( _syntaxColors[lang].numberTextColor );
}


void
ProgramDefaults::writeDefaults(CxString path)
{
    CxFile file;

    if (!file.open(path, "w")) {
        return;
    }
    
    file.printf((char*) "# .cmrc defaults file: see README for more details\n");
    file.printf((char*) "# color sytax is ANSI:<ansicolorname>, XTERM256:<xterm256colorname>, RFB:<R><B><G>\n");
    file.printf((char*) "# not all models work on all computers, specifically older variations of Xterm\n");
    file.printf((char*) "# --------------------------------------------------------------------------------\n");
    file.printf((char*) "\n");
    file.printf((char*) "{\n");
    file.printf((char*) "    \"tabs\": 4,\n");
    file.printf((char*) "    \"jumpscroll\": true,\n");
    file.printf((char*) "    \"showLineNumbers\": true,\n");
    file.printf((char*) "    \"colorizeSyntax\": true,\n");
    file.printf((char*) "    \"liveStatusLines\": true,\n");
    file.printf((char*) "    \"colors\": {\n");
    file.printf((char*) "        \"statusBarTextColor\":\"ANSI:BRIGHT_WHITE\",\n");
    file.printf((char*) "        \"statusBarBackgroundColor\":\"ANSI:BLUE\",\n");
    file.printf((char*) "        \"commentTextColor\":\"ANSI:WHITE\",\n");
    file.printf((char*) "        \"includeTextColor\":\"ANSI:MAGENTA\",\n");
    file.printf((char*) "        \"lineNumberTextColor\":\"ANSI:BRIGHT_BLACK\",\n");
    file.printf((char*) "        \"commandLineMessageTextColor\":\"ANSI:MAGENTA\",\n");
    file.printf((char*) "        \"cppTypesTextColor\":\"ANSI:BRIGHT_BLUE\",\n");
    file.printf((char*) "        \"cppLanguageElementsTextColor\":\"ANSI:BRIGHT_YELLOW\",\n");
    file.printf((char*) "        \"cppLanguageMethodDefinitionTextColor\":\"ANSI:MAGENTA\"\n");
    file.printf((char*) "    }\n");
    file.printf((char*) "}\n");
        
    file.close();
}
