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

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>

#include <cx/base/string.h>
#include <cx/base/slist.h>
#include <cx/base/star.h>
#include <cx/base/hashmap.h>

#include <cx/editbuffer/editbuffer.h>
#include <cx/editbuffer/edithint.h>
#include <cx/editbuffer/stringutils.h>
#include <cx/keyboard/keyboard.h>
#include <cx/screen/screen.h>
#include <cx/functor/defercall.h>

#include "EditView.h"
#include "CommandLineView.h"
#include "ScreenEditor.h"
#include "Project.h"

//-------------------------------------------------------------------------------------------------
// main
//
//
//-------------------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
    CxString filePath;

     if (argc == 2) {
        filePath = argv[1];
    }
    
	int row=0;
	int col=0;

	// create the keyboard object
	CxKeyboard *keyboard = new CxKeyboard();

	// create the screen object
	CxScreen *screen     = new CxScreen();

	// get the current cursor position
	CxScreen::getCursorPosition( &row, &col );

	// save the cursor position
//	CxScreen::saveCursorPosition();

	// open alternate screen to preserve the existing screen    
    CxScreen::openAlternateScreen();
    CxScreen::clearScreen();
    
	// create ad run the editor
    ScreenEditor screenEditor( screen, keyboard, filePath );
    screenEditor.run();

	CxScreen::clearScreen();

	// switch back to the main screen
    CxScreen::closeAlternateScreen();

	// place the cursor where it was when started
	CxScreen::placeCursor(row, col);
//	printf("row=%d; col=%d\n", row, col);

	// restore the cursor position
//	CxScreen::restoreCursorPosition();
}




