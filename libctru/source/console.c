#include <stdio.h>
#include <string.h>
#include <sys/iosupport.h>
#include <3ds/gfx.h>
#include <3ds/console.h>
#include <3ds/svc.h>

#include "default_font_bin.h"

//set up the palette for color printing
static u16 colorTable[] = {
	RGB8_to_565(  0,  0,  0),	// black
	RGB8_to_565(128,  0,  0),	// red
	RGB8_to_565(  0,128,  0),	// green
	RGB8_to_565(128,128,  0),	// yellow
	RGB8_to_565(  0,  0,128),	// blue
	RGB8_to_565(128,  0,128),	// magenta
	RGB8_to_565(  0,128,128),	// cyan
	RGB8_to_565(192,192,192),	// white

	RGB8_to_565(128,128,128),	// bright black
	RGB8_to_565(255,  0,  0),	// bright red
	RGB8_to_565(  0,255,  0),	// bright green
	RGB8_to_565(255,255,  0),	// bright yellow
	RGB8_to_565(  0,  0,255),	// bright blue
	RGB8_to_565(255,  0,255),	// bright magenta
	RGB8_to_565(  0,255,255),	// bright cyan
	RGB8_to_565(255,255,255),	// bright white

	RGB8_to_565(  0,  0,  0),	// faint black
	RGB8_to_565( 64,  0,  0),	// faint red
	RGB8_to_565(  0, 64,  0),	// faint green
	RGB8_to_565( 64, 64,  0),	// faint yellow
	RGB8_to_565(  0,  0, 64),	// faint blue
	RGB8_to_565( 64,  0, 64),	// faint magenta
	RGB8_to_565(  0, 64, 64),	// faint cyan
	RGB8_to_565( 96, 96, 96),	// faint white
};

static const u8 colorCube[] = {
	0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff,
};

static const u8 grayScale[] = {
	0x08, 0x12, 0x1c, 0x26, 0x30, 0x3a, 0x44, 0x4e,
	0x58, 0x62, 0x6c, 0x76, 0x80, 0x8a, 0x94, 0x9e,
	0xa8, 0xb2, 0xbc, 0xc6, 0xd0, 0xda, 0xe4, 0xee,
};

PrintConsole defaultConsole =
{
	//Font:
	{
		(u8*)default_font_bin, //font gfx
		0, //first ascii character in the set
		256 //number of characters in the font set
	},
	(u16*)NULL,
	0,0,	//cursorX cursorY
	0,0,	//prevcursorX prevcursorY
	40,		//console width
	30,		//console height
	1,		//window x
	1,		//window y
	40,		//window width
	30,		//window height
	3,		//tab size
	7,		// foreground color
	0,		// background color
	0,		// flags
	0,		//print callback
	false	//console initialized
};

PrintConsole currentCopy;

PrintConsole* currentConsole = &currentCopy;

PrintConsole* consoleGetDefault(void){return &defaultConsole;}

void consolePrintChar(int c);
void consoleDrawChar(int c);

//---------------------------------------------------------------------------------
static void consoleCls(int mode) {
//---------------------------------------------------------------------------------

	int i = 0;
	int colTemp,rowTemp;

	switch (mode)
	{
		case 0:
		{
			colTemp = currentConsole->cursorX ;
			rowTemp = currentConsole->cursorY ;

			while(i++ < ((currentConsole->windowHeight * currentConsole->windowWidth) - (rowTemp * currentConsole->windowWidth + colTemp)))
				consolePrintChar(' ');

			currentConsole->cursorX  = colTemp;
			currentConsole->cursorY  = rowTemp;
			break;
		}
		case 1:
		{
			colTemp = currentConsole->cursorX ;
			rowTemp = currentConsole->cursorY ;

			currentConsole->cursorY  = 1;
			currentConsole->cursorX  = 1;

			while (i++ < (rowTemp * currentConsole->windowWidth + colTemp))
				consolePrintChar(' ');

			currentConsole->cursorX  = colTemp;
			currentConsole->cursorY  = rowTemp;
			break;
		}
		case 2:
		{
			currentConsole->cursorY  = 1;
			currentConsole->cursorX  = 1;

			while(i++ < currentConsole->windowHeight * currentConsole->windowWidth)
				consolePrintChar(' ');

			currentConsole->cursorY  = 1;
			currentConsole->cursorX  = 1;
			break;
		}
	}
	gfxFlushBuffers();
}
//---------------------------------------------------------------------------------
static void consoleClearLine(int mode) {
//---------------------------------------------------------------------------------

	int i, colTemp;

	switch (mode)
	{
		case 0:
			colTemp = currentConsole->cursorX;

			for (i=0; i < currentConsole->windowWidth - colTemp + 1; i++) {
				consolePrintChar(' ');
			}

			currentConsole->cursorX  = colTemp;

			break;
		case 1:
			colTemp = currentConsole->cursorX ;

			currentConsole->cursorX  = 1;

			for(i=0; i < colTemp - 1; i++) {
				consolePrintChar(' ');
			}

			currentConsole->cursorX  = colTemp;

			break;
		case 2:
			colTemp = currentConsole->cursorX ;

			currentConsole->cursorX  = 1;

			for(i=0; i < currentConsole->windowWidth; i++) {
				consolePrintChar(' ');
			}

			currentConsole->cursorX  = colTemp;

			break;
	}
	gfxFlushBuffers();
}


//---------------------------------------------------------------------------------
static inline void consolePosition(int x, int y) {
//---------------------------------------------------------------------------------
	// invalid position
	if(x < 0 || y < 0)
		return;

	// 1-based, but we'll take a 0
	if(x < 1)
		x = 1;
	if(y < 1)
		y = 1;

	// clip to console edge
	if(x > currentConsole->windowWidth)
		x = currentConsole->windowWidth;
	if(y > currentConsole->windowHeight)
		y = currentConsole->windowHeight;

	currentConsole->cursorX = x;
	currentConsole->cursorY = y;
}

#define _ANSI_MAXARGS 16

static struct
{
	struct
	{
		int flags;
		u32 fg;
		u32 bg;
	} color;
	int argIdx;
	int args[_ANSI_MAXARGS];
	int colorArgCount;
	unsigned int colorArgs[3];
	bool hasArg;
	enum
	{
		ESC_NONE,
		ESC_START,
		ESC_BUILDING_UNKNOWN,
		ESC_BUILDING_FORMAT_FG,
		ESC_BUILDING_FORMAT_BG,
		ESC_BUILDING_FORMAT_FG_NONRGB,
		ESC_BUILDING_FORMAT_BG_NONRGB,
		ESC_BUILDING_FORMAT_FG_RGB,
		ESC_BUILDING_FORMAT_BG_RGB,
	} state;
} escapeSeq;

static void consoleSetColorState(int code)
{
	switch(code)
	{
	case 0: // reset
		escapeSeq.color.flags = 0;
		escapeSeq.color.bg    = 0;
		escapeSeq.color.fg    = 7;
		break;

	case 1: // bold
		escapeSeq.color.flags &= ~CONSOLE_COLOR_FAINT;
		escapeSeq.color.flags |= CONSOLE_COLOR_BOLD;
		break;

	case 2: // faint
		escapeSeq.color.flags &= ~CONSOLE_COLOR_BOLD;
		escapeSeq.color.flags |= CONSOLE_COLOR_FAINT;
		break;

	case 3: // italic
		escapeSeq.color.flags |= CONSOLE_ITALIC;
		break;

	case 4: // underline
		escapeSeq.color.flags |= CONSOLE_UNDERLINE;
		break;
	case 5: // blink slow
		escapeSeq.color.flags &= ~CONSOLE_BLINK_FAST;
		escapeSeq.color.flags |= CONSOLE_BLINK_SLOW;
		break;
	case 6: // blink fast
		escapeSeq.color.flags &= ~CONSOLE_BLINK_SLOW;
		escapeSeq.color.flags |= CONSOLE_BLINK_FAST;
		break;
	case 7: // reverse video
		escapeSeq.color.flags |= CONSOLE_COLOR_REVERSE;
		break;
	case 8: // conceal
		escapeSeq.color.flags |= CONSOLE_CONCEAL;
		break;
	case 9: // crossed-out
		escapeSeq.color.flags |= CONSOLE_CROSSED_OUT;
		break;
	case 21: // bold off
		escapeSeq.color.flags &= ~CONSOLE_COLOR_BOLD;
		break;

	case 22: // normal color
		escapeSeq.color.flags &= ~CONSOLE_COLOR_BOLD;
		escapeSeq.color.flags &= ~CONSOLE_COLOR_FAINT;
		break;

	case 23: // italic off
		escapeSeq.color.flags &= ~CONSOLE_ITALIC;
		break;

	case 24: // underline off
		escapeSeq.color.flags &= ~CONSOLE_UNDERLINE;
		break;

	case 25: // blink off
		escapeSeq.color.flags &= ~CONSOLE_BLINK_SLOW;
		escapeSeq.color.flags &= ~CONSOLE_BLINK_FAST;
		break;

	case 27: // reverse off
		escapeSeq.color.flags &= ~CONSOLE_COLOR_REVERSE;
		break;

	case 29: // crossed-out off
		escapeSeq.color.flags &= ~CONSOLE_CROSSED_OUT;
		break;

	case 30 ... 37: // writing color
		escapeSeq.color.flags &= ~CONSOLE_FG_CUSTOM;
		escapeSeq.color.fg     = code - 30;
		break;

	case 38: // custom foreground color
		escapeSeq.state = ESC_BUILDING_FORMAT_FG;
		escapeSeq.colorArgCount = 0;
		break;

	case 39: // reset foreground color
		escapeSeq.color.flags &= ~CONSOLE_FG_CUSTOM;
		escapeSeq.color.fg     = 7;
		break;
	case 40 ... 47: // screen color
		escapeSeq.color.flags &= ~CONSOLE_BG_CUSTOM;
		escapeSeq.color.bg = code - 40;
		break;
	case 48: // custom background color
		escapeSeq.state = ESC_BUILDING_FORMAT_BG;
		escapeSeq.colorArgCount = 0;
		break;
	case 49: // reset background color
		escapeSeq.color.flags &= ~CONSOLE_BG_CUSTOM;
		escapeSeq.color.bg = 0;
		break;
	case 90 ... 97: // bright foreground
		escapeSeq.color.flags &= ~CONSOLE_COLOR_FAINT;
		escapeSeq.color.flags |= CONSOLE_COLOR_FG_BRIGHT;
		escapeSeq.color.flags &= ~CONSOLE_BG_CUSTOM;
		escapeSeq.color.fg = code - 90;
		break;
	case 100 ... 107: // bright background
		escapeSeq.color.flags &= ~CONSOLE_COLOR_FAINT;
		escapeSeq.color.flags |= CONSOLE_COLOR_BG_BRIGHT;
		escapeSeq.color.flags &= ~CONSOLE_BG_CUSTOM;
		escapeSeq.color.bg = code - 100;
		break;
	}
}

static void consoleHandleColorEsc(int argCount)
{
	escapeSeq.color.bg = currentConsole->bg;
	escapeSeq.color.fg = currentConsole->fg;
	escapeSeq.color.flags = currentConsole->flags;

	for (int arg = 0; arg < argCount; arg++)
	{
		int code = escapeSeq.args[arg];
		switch (escapeSeq.state)
		{
		case ESC_BUILDING_UNKNOWN:
			consoleSetColorState(code);
			break;
		case ESC_BUILDING_FORMAT_FG:
			if (code == 5)
				escapeSeq.state = ESC_BUILDING_FORMAT_FG_NONRGB;
			else if (code == 2)
				escapeSeq.state = ESC_BUILDING_FORMAT_FG_RGB;
			else
				escapeSeq.state = ESC_BUILDING_UNKNOWN;
			break;
		case ESC_BUILDING_FORMAT_BG:
			if (code == 5)
				escapeSeq.state = ESC_BUILDING_FORMAT_BG_NONRGB;
			else if (code == 2)
				escapeSeq.state = ESC_BUILDING_FORMAT_BG_RGB;
			else
				escapeSeq.state = ESC_BUILDING_UNKNOWN;
			break;
		case ESC_BUILDING_FORMAT_FG_NONRGB:
			if (code <= 15) {
				escapeSeq.color.fg  = code;
				escapeSeq.color.flags &= ~CONSOLE_FG_CUSTOM;
			} else if (code <= 231) {
				code -= 16;
				unsigned int r = code / 36;
				unsigned int g = (code - r * 36) / 6;
				unsigned int b = code - r * 36 - g * 6;

				escapeSeq.color.fg  = RGB8_to_565 (colorCube[r], colorCube[g], colorCube[b]);
				escapeSeq.color.flags |= CONSOLE_FG_CUSTOM;
			} else if (code <= 255) {
				code -= 232;

				escapeSeq.color.fg  = RGB8_to_565 (grayScale[code], grayScale[code], grayScale[code]);
				escapeSeq.color.flags |= CONSOLE_FG_CUSTOM;
			}
			escapeSeq.state = ESC_BUILDING_UNKNOWN;
			break;
		case ESC_BUILDING_FORMAT_BG_NONRGB:
			if (code <= 15) {
				escapeSeq.color.bg  = code;
				escapeSeq.color.flags &= ~CONSOLE_BG_CUSTOM;
			} else if (code <= 231) {
				code -= 16;
				unsigned int r = code / 36;
				unsigned int g = (code - r * 36) / 6;
				unsigned int b = code - r * 36 - g * 6;

				escapeSeq.color.bg  = RGB8_to_565 (colorCube[r], colorCube[g], colorCube[b]);
				escapeSeq.color.flags |= CONSOLE_BG_CUSTOM;
			} else if (code <= 255) {
				code -= 232;

				escapeSeq.color.bg  = RGB8_to_565 (grayScale[code], grayScale[code], grayScale[code]);
				escapeSeq.color.flags |= CONSOLE_BG_CUSTOM;
			}
			escapeSeq.state = ESC_BUILDING_UNKNOWN;
			break;
		case ESC_BUILDING_FORMAT_FG_RGB:
			escapeSeq.colorArgs[escapeSeq.colorArgCount++] = code;
			if(escapeSeq.colorArgCount == 3)
			{
				escapeSeq.color.fg = RGB8_to_565(escapeSeq.colorArgs[0], escapeSeq.colorArgs[1], escapeSeq.colorArgs[2]);
				escapeSeq.color.flags |= CONSOLE_FG_CUSTOM;
				escapeSeq.state = ESC_BUILDING_UNKNOWN;
			}
			break;
		case ESC_BUILDING_FORMAT_BG_RGB:
			escapeSeq.colorArgs[escapeSeq.colorArgCount++] = code;
			if(escapeSeq.colorArgCount == 3)
			{
				escapeSeq.color.bg = RGB8_to_565(escapeSeq.colorArgs[0], escapeSeq.colorArgs[1], escapeSeq.colorArgs[2]);
				escapeSeq.color.flags |= CONSOLE_BG_CUSTOM;
				escapeSeq.state = ESC_BUILDING_UNKNOWN;
			}
		default:
			break;
		}
	}
	escapeSeq.argIdx = 0;

	currentConsole->bg = escapeSeq.color.bg;
	currentConsole->fg = escapeSeq.color.fg;
	currentConsole->flags = escapeSeq.color.flags;

}


//---------------------------------------------------------------------------------
ssize_t con_write(struct _reent *r,void *fd,const char *ptr, size_t len) {
//---------------------------------------------------------------------------------

	char chr;

	int i, count = 0;
	char *tmp = (char*)ptr;

	if(!tmp) return -1;

	i = 0;

	while(i<len) {

		chr = *(tmp++);
		i++; count++;
		switch (escapeSeq.state)
		{
		case ESC_NONE:
			if (chr == 0x1b)
				escapeSeq.state = ESC_START;
			else
				consolePrintChar(chr);
			break;
		case ESC_START:
			if (chr == '[')
			{
				escapeSeq.state = ESC_BUILDING_UNKNOWN;
				escapeSeq.hasArg = false;
				memset(escapeSeq.args, 0, sizeof(escapeSeq.args));
				escapeSeq.color.bg = currentConsole->bg;
				escapeSeq.color.fg = currentConsole->fg;
				escapeSeq.color.flags = currentConsole->flags;
				escapeSeq.argIdx = 0;
			}
			else
			{
				consolePrintChar(0x1b);
				consolePrintChar(chr);
				escapeSeq.state = ESC_NONE;
			}
			break;
		case ESC_BUILDING_UNKNOWN:
			switch (chr)
			{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				escapeSeq.hasArg = true;
				escapeSeq.args[escapeSeq.argIdx] = escapeSeq.args[escapeSeq.argIdx] * 10 + (chr - '0');
				break;
			case ';':
				if (escapeSeq.hasArg) {
					if (escapeSeq.argIdx < _ANSI_MAXARGS) {
						escapeSeq.argIdx++;
					}
				}
				escapeSeq.hasArg = false;
				break;
			//---------------------------------------			// Cursor directional movement
			//---------------------------------------
			case 'A':
				if (!escapeSeq.hasArg && !escapeSeq.argIdx)
					escapeSeq.args[0] = 1;
				currentConsole->cursorY  =  currentConsole->cursorY - escapeSeq.args[0];
				if (currentConsole->cursorY < 1)
					currentConsole->cursorY = 1;
				escapeSeq.state = ESC_NONE;
				break;
			case 'B':
				if (!escapeSeq.hasArg && !escapeSeq.argIdx)
					escapeSeq.args[0] = 1;
				currentConsole->cursorY  =  currentConsole->cursorY + escapeSeq.args[0];
				if (currentConsole->cursorY > currentConsole->windowHeight)
					currentConsole->cursorY = currentConsole->windowHeight;
				escapeSeq.state = ESC_NONE;
				break;
			case 'C':
				if (!escapeSeq.hasArg && !escapeSeq.argIdx)
					escapeSeq.args[0] = 1;
				currentConsole->cursorX  =  currentConsole->cursorX  + escapeSeq.args[0];
				if (currentConsole->cursorX > currentConsole->windowWidth)
					currentConsole->cursorX = currentConsole->windowWidth;
				escapeSeq.state = ESC_NONE;
				break;
			case 'D':
				if (!escapeSeq.hasArg && !escapeSeq.argIdx)
					escapeSeq.args[0] = 1;
				currentConsole->cursorX  =  currentConsole->cursorX  - escapeSeq.args[0];
				if (currentConsole->cursorX < 1)
					currentConsole->cursorX = 1;
				escapeSeq.state = ESC_NONE;
				break;
			//---------------------------------------
			// Cursor position movement
			//---------------------------------------
			case 'H':
			case 'f':
				consolePosition(escapeSeq.args[1], escapeSeq.args[0]);
				escapeSeq.state = ESC_NONE;
				break;
			//---------------------------------------
			// Screen clear
			//---------------------------------------
			case 'J':
				if (escapeSeq.argIdx == 0 && !escapeSeq.hasArg) {
					escapeSeq.args[0] = 0;
				}
				consoleCls(escapeSeq.args[0]);
				escapeSeq.state = ESC_NONE;
				break;
			//---------------------------------------
			// Line clear
			//---------------------------------------
			case 'K':
				if (escapeSeq.argIdx == 0 && !escapeSeq.hasArg) {
					escapeSeq.args[0] = 0;
				}
				consoleClearLine(escapeSeq.args[0]);
				escapeSeq.state = ESC_NONE;
				break;
			//---------------------------------------
			// Save cursor position
			//---------------------------------------
			case 's':
				currentConsole->prevCursorX  = currentConsole->cursorX ;
				currentConsole->prevCursorY  = currentConsole->cursorY ;
				escapeSeq.state = ESC_NONE;
				break;
			//---------------------------------------
			// Load cursor position
			//---------------------------------------
			case 'u':
				currentConsole->cursorX  = currentConsole->prevCursorX ;
				currentConsole->cursorY  = currentConsole->prevCursorY ;
				escapeSeq.state = ESC_NONE;
				break;
			//---------------------------------------
			// Color scan codes
			//---------------------------------------
			case 'm':
				if (escapeSeq.argIdx == 0 && !escapeSeq.hasArg) escapeSeq.args[escapeSeq.argIdx++] = 0;
				if (escapeSeq.hasArg) escapeSeq.argIdx++;
				consoleHandleColorEsc(escapeSeq.argIdx);
				escapeSeq.state = ESC_NONE;
				break;
			default:
				// some sort of unsupported escape; just gloss over it
				escapeSeq.state = ESC_NONE;
				break;
			}
		default:
			break;
		}
	}

	return count;
}

static const devoptab_t dotab_stdout = {
	"con",
	0,
	NULL,
	NULL,
	con_write,
	NULL,
	NULL,
	NULL
};

//---------------------------------------------------------------------------------
static ssize_t debug_write(struct _reent *r, void *fd, const char *ptr, size_t len) {
//---------------------------------------------------------------------------------
	svcOutputDebugString(ptr,len);
	return len;
}

static const devoptab_t dotab_svc = {
	"svc",
	0,
	NULL,
	NULL,
	debug_write,
	NULL,
	NULL,
	NULL
};


static const devoptab_t dotab_null = {
	"null",
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

//---------------------------------------------------------------------------------
PrintConsole* consoleInit(gfxScreen_t screen, PrintConsole* console) {
//---------------------------------------------------------------------------------

	static bool firstConsoleInit = true;

	if(firstConsoleInit) {
		devoptab_list[STD_OUT] = &dotab_stdout;
		devoptab_list[STD_ERR] = &dotab_stdout;

		setvbuf(stdout, NULL , _IONBF, 0);
		setvbuf(stderr, NULL , _IONBF, 0);

		memset(&escapeSeq, 0, sizeof(escapeSeq));

		firstConsoleInit = false;
	}

	if(console) {
		currentConsole = console;
	} else {
		console = currentConsole;
	}

	*currentConsole = defaultConsole;

	console->consoleInitialised = 1;

	gfxSetScreenFormat(screen,GSP_RGB565_OES);
	gfxSetDoubleBuffering(screen,false);
	gfxSwapBuffersGpu();
	gspWaitForVBlank();

	console->frameBuffer = (u16*)gfxGetFramebuffer(screen, GFX_LEFT, NULL, NULL);

	if(screen==GFX_TOP) {
		bool isWide = gfxIsWide();
		console->consoleWidth = isWide ? 100 : 50;
		console->windowWidth = isWide ? 100 : 50;
	}

	consoleCls(2);

	return currentConsole;

}

//---------------------------------------------------------------------------------
void consoleDebugInit(debugDevice device){
//---------------------------------------------------------------------------------

	int buffertype = _IONBF;

	switch(device)
	{
		case debugDevice_SVC:
			devoptab_list[STD_ERR] = &dotab_svc;
			buffertype = _IOLBF;
			break;
		case debugDevice_CONSOLE:
			devoptab_list[STD_ERR] = &dotab_stdout;
			break;
		case debugDevice_NULL:
			devoptab_list[STD_ERR] = &dotab_null;
			break;
	}
	setvbuf(stderr, NULL , buffertype, 0);

}

//---------------------------------------------------------------------------------
PrintConsole *consoleSelect(PrintConsole* console){
//---------------------------------------------------------------------------------
	PrintConsole *tmp = currentConsole;
	currentConsole = console;
	return tmp;
}

//---------------------------------------------------------------------------------
void consoleSetFont(PrintConsole* console, ConsoleFont* font){
//---------------------------------------------------------------------------------

	if(!console) console = currentConsole;

	console->font = *font;

}

//---------------------------------------------------------------------------------
static void newRow() {
//---------------------------------------------------------------------------------


	currentConsole->cursorY ++;


	if(currentConsole->cursorY  > currentConsole->windowHeight)  {
		currentConsole->cursorY = currentConsole->windowHeight;
		u16 *dst = &currentConsole->frameBuffer[((currentConsole->windowX - 1 ) * 8 * 240) + (239 - ((currentConsole->windowY) * 8))];
		u16 *src = dst - 8;

		int i,j;

		for (i=0; i<(currentConsole->windowWidth)*8; i++) {
			u32 *from = (u32*)((int)src & ~3);
			u32 *to = (u32*)((int)dst & ~3);
			for (j=0;j<(((currentConsole->windowHeight-2)*8)/2);j++) *(to--) = *(from--);
			dst += 240;
			src += 240;
		}

		consoleClearLine(2);
	}
}
//---------------------------------------------------------------------------------
void consoleDrawChar(int c) {
//---------------------------------------------------------------------------------
	c -= currentConsole->font.asciiOffset;
	if ( c < 0 || c > currentConsole->font.numChars ) return;

	u8 *fontdata = currentConsole->font.gfx + (8 * c);

	u16 fg = currentConsole->fg;
	u16 bg = currentConsole->bg;

	if (!(currentConsole->flags & CONSOLE_FG_CUSTOM)) {
		if (currentConsole->flags & (CONSOLE_COLOR_BOLD | CONSOLE_COLOR_FG_BRIGHT)) {
			fg += 8;
		} else if (currentConsole->flags & CONSOLE_COLOR_FAINT) {
			fg += 16;
		}
		fg = colorTable[fg];
	}

	if (!(currentConsole->flags & CONSOLE_BG_CUSTOM)) {
		if (currentConsole->flags & CONSOLE_COLOR_BG_BRIGHT) bg +=8;
		bg = colorTable[bg];
	}

	if (currentConsole->flags & CONSOLE_COLOR_REVERSE) {
		u16 tmp = fg;
		fg = bg;
		bg = tmp;
	}

	u8 b1 = *(fontdata++);
	u8 b2 = *(fontdata++);
	u8 b3 = *(fontdata++);
	u8 b4 = *(fontdata++);
	u8 b5 = *(fontdata++);
	u8 b6 = *(fontdata++);
	u8 b7 = *(fontdata++);
	u8 b8 = *(fontdata++);

	if (currentConsole->flags & CONSOLE_UNDERLINE) b8 = 0xff;

	if (currentConsole->flags & CONSOLE_CROSSED_OUT) b4 = 0xff;

	u8 mask = 0x80;


	int i;

	int x = (currentConsole->cursorX - 1 + currentConsole->windowX - 1 ) * 8;
	int y = ((currentConsole->cursorY - 1 + currentConsole->windowY - 1 ) *8 );

	u16 *screen = &currentConsole->frameBuffer[(x * 240) + (239 - (y + 7))];

	for (i=0;i<8;i++) {
		if (b8 & mask) { *(screen++) = fg; }else{ *(screen++) = bg; }
		if (b7 & mask) { *(screen++) = fg; }else{ *(screen++) = bg; }
		if (b6 & mask) { *(screen++) = fg; }else{ *(screen++) = bg; }
		if (b5 & mask) { *(screen++) = fg; }else{ *(screen++) = bg; }
		if (b4 & mask) { *(screen++) = fg; }else{ *(screen++) = bg; }
		if (b3 & mask) { *(screen++) = fg; }else{ *(screen++) = bg; }
		if (b2 & mask) { *(screen++) = fg; }else{ *(screen++) = bg; }
		if (b1 & mask) { *(screen++) = fg; }else{ *(screen++) = bg; }
		mask >>= 1;
		screen += 240 - 8;
	}

}

//---------------------------------------------------------------------------------
void consolePrintChar(int c) {
//---------------------------------------------------------------------------------
	int tabspaces;

	if (c==0) return;

	if(currentConsole->PrintChar)
		if(currentConsole->PrintChar(currentConsole, c))
			return;

	switch(c) {
		/*
		The only special characters we will handle are tab (\t), carriage return (\r), line feed (\n)
		and backspace (\b).
		Carriage return & line feed will function the same: go to next line and put cursor at the beginning.
		For everything else, use VT sequences.

		Reason: VT sequences are more specific to the task of cursor placement.
		The special escape sequences \b \f & \v are archaic and non-portable.
		*/
		case 8:
			currentConsole->cursorX--;

			if(currentConsole->cursorX < 1) {
				if(currentConsole->cursorY > 1) {
					currentConsole->cursorX = currentConsole->windowWidth;
					currentConsole->cursorY--;
				} else {
					currentConsole->cursorX = 1;
				}
			}

			consoleDrawChar(' ');
			break;

		case 9:
			tabspaces = currentConsole->tabSize - ((currentConsole->cursorX - 1) % currentConsole->tabSize);
			for(int i=0; i<tabspaces; i++) consolePrintChar(' ');
			break;
		case 10:
			newRow();
		case 13:
			currentConsole->cursorX  = 1;
			gfxFlushBuffers();
			break;
		default:
			if(currentConsole->cursorX  > currentConsole->windowWidth) {
				currentConsole->cursorX  = 1;

				newRow();
			}
			consoleDrawChar(c);
			++currentConsole->cursorX ;
			break;
	}
}

//---------------------------------------------------------------------------------
void consoleClear(void) {
//---------------------------------------------------------------------------------
	consoleCls(2);
}

//---------------------------------------------------------------------------------
void consoleSetWindow(PrintConsole* console, int x, int y, int width, int height){
//---------------------------------------------------------------------------------

	if(!console) console = currentConsole;

	if (x < 1) x = 1;
	if (y < 1) y = 1;

	console->windowWidth = width;
	console->windowHeight = height;
	console->windowX = x;
	console->windowY = y;

	console->cursorX = 1;
	console->cursorY = 1;

}
