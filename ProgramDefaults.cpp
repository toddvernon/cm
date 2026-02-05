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
#if defined(_OSX_) || defined(_LINUX_)
    _liveStatusLine(TRUE)
#else
    _liveStatusLine(FALSE)
#endif
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
			parseBooleanField( object, "jumpscroll", &_jumpscroll );
            parseBooleanField( object, "showLineNumbers", &_showLineNumbers );
            parseBooleanField( object, "autoSaveOnBufferChange", &_autoSaveOnBufferChange );
            parseBooleanField( object, "colorizeSyntax", &_colorizeSyntax );
            parseBooleanField( object, "liveStatusLines", &_liveStatusLine );
            
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
                    parseColorFromJSON(colorObject, "commentTextColor", &_commentTextColor, FALSE);
                    parseColorFromJSON(colorObject, "statusBarTextColor", &_statusBarTextColor, FALSE);
                    parseColorFromJSON(colorObject, "statusBarBackgroundColor", &_statusBarBackgroundColor, TRUE);
                    parseColorFromJSON(colorObject, "includeTextColor", &_includeTextColor, FALSE);
                    parseColorFromJSON(colorObject, "lineNumberTextColor", &_lineNumberTextColor, FALSE);
                    parseColorFromJSON(colorObject, "commandLineMessageTextColor", &_commandLineMessageTextColor, FALSE);
                    parseColorFromJSON(colorObject, "cppLanguageTypesTextColor", &_cppLanguageTypesTextColor, FALSE);
                    parseColorFromJSON(colorObject, "cppLanguageElementsTextColor", &_cppLanguageElementsTextColor, FALSE);
                    parseColorFromJSON(colorObject, "cppLanguageMethodDefinitionTextColor", &_cppLanguageMethodDefinitionTextColor, FALSE);

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
// ProgramDefaults::parseBooleanField
//
// Parse a boolean field from a JSON object and assign it to the target pointer.
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseBooleanField( CxJSONObject *obj, const char *fieldName, int *target )
{
    CxJSONMember *member = obj->find( fieldName );
    if (member != NULL && member->object()->type() == CxJSONBase::BOOLEAN) {
        CxJSONBoolean *value = (CxJSONBoolean *) member->object();
        *target = value->get();
        return( TRUE );
    }
    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseColorFromJSON
//
// Parse a color field from a JSON object and assign it to the target pointer.
// isBackground: FALSE = foreground, TRUE = background
//-------------------------------------------------------------------------------------------------
int
ProgramDefaults::parseColorFromJSON( CxJSONObject *obj, const char *fieldName,
                                     CxColor **target, int isBackground )
{
    CxJSONMember *member = obj->find( fieldName );
    if (member != NULL && member->object()->type() == CxJSONBase::STRING) {
        CxJSONString *value = (CxJSONString *) member->object();
        *target = ProgramDefaults::parseColor( value->get(), isBackground );
        return( TRUE );
    }
    return( FALSE );
}


//-------------------------------------------------------------------------------------------------
// ProgramDefaults::parseColor
//
// Unified color parser for both foreground and background colors.
// isBackground: FALSE = foreground, TRUE = background
//-------------------------------------------------------------------------------------------------
/* static */
CxColor *
ProgramDefaults::parseColor( CxString colorString, int isBackground )
{
    // if there is no colon the color selector is not present
    if (colorString.firstChar(":") == -1) {
        if (isBackground)
            return new CxAnsiBackgroundColor( CxAnsiBackgroundColor::NONE );
        return new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    }

    // extract the color type selector before the colon
    CxString colorTypeSelector = colorString.nextToken(" :\t\n\r");

    // proactively get the next three arguments
    CxString item1 = colorString.nextToken(" ,\n\r\t:");
    CxString item2 = colorString.nextToken(" ,\n\r\t:");
    CxString item3 = colorString.nextToken(" ,\n\r\t:");

    // if the color type selector is empty
    if (colorTypeSelector.length() == 0) {
        if (isBackground)
            return new CxAnsiBackgroundColor( CxAnsiBackgroundColor::NONE );
        return new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
    }

    // ANSI color type
    if (CxString::toUpper(colorTypeSelector) == "ANSI") {
        if (isBackground)
            return new CxAnsiBackgroundColor( item1 );
        return new CxAnsiForegroundColor( item1 );
    }

    // XTERM256 color type
    if (CxString::toUpper(colorTypeSelector) == "XTERM256") {
        if (isBackground)
            return new CxXterm256BackgroundColor( item1 );
        return new CxXterm256ForegroundColor( item1 );
    }

    // RGB color type
    if (CxString::toUpper(colorTypeSelector) == "RGB") {
        int red   = item1.toInt();
        int green = item2.toInt();
        int blue  = item3.toInt();
        if (isBackground)
            return new CxRGBBackgroundColor( red, green, blue );
        return new CxRGBForegroundColor( red, green, blue );
    }

    // fallback: return NONE color of the correct type
    if (isBackground)
        return new CxAnsiBackgroundColor( CxAnsiBackgroundColor::NONE );
    return new CxAnsiForegroundColor( CxAnsiForegroundColor::NONE );
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

    parseColorFromJSON(colorSet, "commentTextColor", &colors->commentTextColor, FALSE);
    parseColorFromJSON(colorSet, "includeTextColor", &colors->includeTextColor, FALSE);
    parseColorFromJSON(colorSet, "keywordTextColor", &colors->keywordTextColor, FALSE);
    parseColorFromJSON(colorSet, "typeTextColor", &colors->typeTextColor, FALSE);
    parseColorFromJSON(colorSet, "constantTextColor", &colors->constantTextColor, FALSE);
    parseColorFromJSON(colorSet, "methodDefinitionTextColor", &colors->methodDefinitionTextColor, FALSE);
    parseColorFromJSON(colorSet, "stringTextColor", &colors->stringTextColor, FALSE);
    parseColorFromJSON(colorSet, "numberTextColor", &colors->numberTextColor, FALSE);

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

    // platform-specific default color values
#if defined(_OSX_) || defined(_LINUX_)
    const char *hdr  = "# Uses RGB true color - requires 24-bit color terminal support";
    const char *statusFg  = "RGB:250,250,245";
    const char *statusBg  = "RGB:60,70,100";
    const char *lineNum   = "RGB:100,100,110";
    const char *cmdMsg    = "RGB:180,150,220";
    const char *comment   = "RGB:130,140,150";
    const char *include   = "RGB:255,150,130";
    const char *keyword   = "RGB:200,150,255";
    const char *type      = "RGB:100,220,220";
    const char *constant  = "RGB:255,180,100";
    const char *methodDef = "RGB:130,220,130";
    const char *strColor  = "RGB:150,230,150";
    const char *number    = "RGB:180,220,255";
    const char *liveStatus = "true";
#elif defined(_SOLARIS6_) || defined(_SOLARIS10_) || defined(_IRIX6_)
    const char *hdr  = "# Uses XTERM 256 color palette for broad terminal compatibility";
    const char *statusFg  = "XTERM256:White";
    const char *statusBg  = "XTERM256:DarkBlue";
    const char *lineNum   = "XTERM256:Grey42";
    const char *cmdMsg    = "XTERM256:MediumOrchid1";
    const char *comment   = "XTERM256:Grey54";
    const char *include   = "XTERM256:Salmon1";
    const char *keyword   = "XTERM256:MediumPurple1";
    const char *type      = "XTERM256:DarkTurquoise";
    const char *constant  = "XTERM256:Orange1";
    const char *methodDef = "XTERM256:LightGreen";
    const char *strColor  = "XTERM256:PaleGreen1";
    const char *number    = "XTERM256:LightSteelBlue1";
    const char *liveStatus = "false";
#else
    const char *hdr  = "# Uses ANSI 16-color palette for maximum terminal compatibility";
    const char *statusFg  = "ANSI:BRIGHT_WHITE";
    const char *statusBg  = "ANSI:BLUE";
    const char *lineNum   = "ANSI:BRIGHT_BLACK";
    const char *cmdMsg    = "ANSI:BRIGHT_CYAN";
    const char *comment   = "ANSI:BRIGHT_BLACK";
    const char *include   = "ANSI:BRIGHT_MAGENTA";
    const char *keyword   = "ANSI:BRIGHT_YELLOW";
    const char *type      = "ANSI:BRIGHT_CYAN";
    const char *constant  = "ANSI:BRIGHT_MAGENTA";
    const char *methodDef = "ANSI:BRIGHT_GREEN";
    const char *strColor  = "ANSI:GREEN";
    const char *number    = "ANSI:CYAN";
    const char *liveStatus = "false";
#endif

    file.printf("# .cmrc defaults file\n");
    file.printf("%s\n", hdr);
    file.printf("# color syntax is ANSI:<name>, XTERM256:<name>, RGB:<R>,<G>,<B>\n");
    file.printf("# --------------------------------------------------------------------------------\n");
    file.printf("\n");
    file.printf("{\n");
    file.printf("    \"tabs\": 4,\n");
    file.printf("    \"jumpscroll\": true,\n");
    file.printf("    \"showLineNumbers\": true,\n");
    file.printf("    \"colorizeSyntax\": true,\n");
    file.printf("    \"liveStatusLines\": %s,\n", liveStatus);
    file.printf("    \"autoSaveOnBufferChange\": false,\n");
    file.printf("\n");

    // UI colors
    file.printf("    \"colors\": {\n");
    file.printf("        \"statusBarTextColor\": \"%s\",\n", statusFg);
    file.printf("        \"statusBarBackgroundColor\": \"%s\",\n", statusBg);
    file.printf("        \"lineNumberTextColor\": \"%s\",\n", lineNum);
    file.printf("        \"commandLineMessageTextColor\": \"%s\"\n", cmdMsg);
    file.printf("    },\n");
    file.printf("\n");

    // syntax colors
    file.printf("    \"syntaxColors\": {\n");
    file.printf("        \"default\": {\n");
    file.printf("            \"commentTextColor\": \"%s\",\n", comment);
    file.printf("            \"includeTextColor\": \"%s\",\n", include);
    file.printf("            \"keywordTextColor\": \"%s\",\n", keyword);
    file.printf("            \"typeTextColor\": \"%s\",\n", type);
    file.printf("            \"constantTextColor\": \"%s\",\n", constant);
    file.printf("            \"methodDefinitionTextColor\": \"%s\",\n", methodDef);
    file.printf("            \"stringTextColor\": \"%s\",\n", strColor);
    file.printf("            \"numberTextColor\": \"%s\"\n", number);
    file.printf("        },\n");
    file.printf("        \"c\": {\n");
    file.printf("        },\n");
    file.printf("        \"cpp\": {\n");
    file.printf("        },\n");

    // per-language overrides vary by platform
#if defined(_OSX_) || defined(_LINUX_)
    file.printf("        \"swift\": {\n");
    file.printf("            \"keywordTextColor\": \"RGB:255,120,130\",\n");
    file.printf("            \"typeTextColor\": \"RGB:130,200,255\",\n");
    file.printf("            \"constantTextColor\": \"RGB:255,200,100\"\n");
    file.printf("        },\n");
    file.printf("        \"python\": {\n");
    file.printf("            \"keywordTextColor\": \"RGB:255,200,100\",\n");
    file.printf("            \"methodDefinitionTextColor\": \"RGB:100,180,255\"\n");
    file.printf("        },\n");
    file.printf("        \"javascript\": {\n");
    file.printf("            \"keywordTextColor\": \"RGB:255,150,180\",\n");
    file.printf("            \"constantTextColor\": \"RGB:255,200,130\"\n");
    file.printf("        },\n");
    file.printf("        \"go\": {\n");
    file.printf("            \"keywordTextColor\": \"RGB:100,200,255\",\n");
    file.printf("            \"typeTextColor\": \"RGB:180,230,180\"\n");
    file.printf("        },\n");
    file.printf("        \"rust\": {\n");
    file.printf("            \"keywordTextColor\": \"RGB:255,150,100\",\n");
    file.printf("            \"typeTextColor\": \"RGB:150,220,200\"\n");
    file.printf("        },\n");
    file.printf("        \"java\": {\n");
    file.printf("            \"keywordTextColor\": \"RGB:255,130,100\",\n");
    file.printf("            \"typeTextColor\": \"RGB:130,200,230\"\n");
    file.printf("        },\n");
    file.printf("        \"shell\": {\n");
    file.printf("            \"keywordTextColor\": \"RGB:130,200,255\",\n");
    file.printf("            \"constantTextColor\": \"RGB:255,220,130\"\n");
    file.printf("        }\n");
#elif defined(_SOLARIS6_) || defined(_SOLARIS10_) || defined(_IRIX6_)
    file.printf("        \"swift\": {\n");
    file.printf("            \"keywordTextColor\": \"XTERM256:IndianRed1\",\n");
    file.printf("            \"typeTextColor\": \"XTERM256:SkyBlue1\",\n");
    file.printf("            \"constantTextColor\": \"XTERM256:Gold1\"\n");
    file.printf("        },\n");
    file.printf("        \"python\": {\n");
    file.printf("            \"keywordTextColor\": \"XTERM256:Gold1\",\n");
    file.printf("            \"methodDefinitionTextColor\": \"XTERM256:DodgerBlue1\"\n");
    file.printf("        },\n");
    file.printf("        \"javascript\": {\n");
    file.printf("            \"keywordTextColor\": \"XTERM256:HotPink\",\n");
    file.printf("            \"constantTextColor\": \"XTERM256:NavajoWhite1\"\n");
    file.printf("        },\n");
    file.printf("        \"go\": {\n");
    file.printf("            \"keywordTextColor\": \"XTERM256:SteelBlue1\",\n");
    file.printf("            \"typeTextColor\": \"XTERM256:DarkSeaGreen1\"\n");
    file.printf("        },\n");
    file.printf("        \"rust\": {\n");
    file.printf("            \"keywordTextColor\": \"XTERM256:DarkOrange\",\n");
    file.printf("            \"typeTextColor\": \"XTERM256:Aquamarine1\"\n");
    file.printf("        },\n");
    file.printf("        \"java\": {\n");
    file.printf("            \"keywordTextColor\": \"XTERM256:Coral\",\n");
    file.printf("            \"typeTextColor\": \"XTERM256:LightSkyBlue1\"\n");
    file.printf("        },\n");
    file.printf("        \"shell\": {\n");
    file.printf("            \"keywordTextColor\": \"XTERM256:CornflowerBlue\",\n");
    file.printf("            \"constantTextColor\": \"XTERM256:LightGoldenrod1\"\n");
    file.printf("        }\n");
#else
    file.printf("        \"swift\": {\n");
    file.printf("        },\n");
    file.printf("        \"python\": {\n");
    file.printf("            \"methodDefinitionTextColor\": \"ANSI:BRIGHT_BLUE\"\n");
    file.printf("        },\n");
    file.printf("        \"javascript\": {\n");
    file.printf("        },\n");
    file.printf("        \"go\": {\n");
    file.printf("        },\n");
    file.printf("        \"rust\": {\n");
    file.printf("        },\n");
    file.printf("        \"java\": {\n");
    file.printf("        },\n");
    file.printf("        \"shell\": {\n");
    file.printf("            \"keywordTextColor\": \"ANSI:BRIGHT_CYAN\",\n");
    file.printf("            \"constantTextColor\": \"ANSI:BRIGHT_YELLOW\"\n");
    file.printf("        }\n");
#endif

    file.printf("    }\n");
    file.printf("}\n");

    file.close();
}
