#include <string.h>
#include <stdio.h>
#include "textInput.h"
#ifndef DRAW_LIBRARY
	#include "draw.h"
#endif

void in_init(uint32_t* display, const unsigned char* fontData, int* dimX, int* dimY, char* iS, int length, int x, int y, int w, int h) {
	in_x = x; in_y = y; in_width = w; in_height = h;
	memcpy(in_displayText, iS, 100);

	in_frame = 0;
	in_length = length;
	in_cursorPos = length-1;
	in_selected = 0;
	in_blinkRate = 40;
	in_display = display;

	in_fontData = fontData;

	in_draw();
}

void in_enterKey(char in) {
	char b[100], a[100];
	
	memcpy(b, in_displayText, in_cursorPos);
	memcpy(a, in_displayText+in_cursorPos, in_length-in_cursorPos);
	b[in_cursorPos] = '\0';
	a[in_length-in_cursorPos] = '\0';

	snprintf(in_displayText, 100, "%s%c%s", b, in, a);

	in_length++;
	in_cursorPos++;
}

void in_cursorPlus() {in_cursorPos += (in_cursorPos < in_length)?1:0;}
void in_cursorMinus(){in_cursorPos -= (in_cursorPos > 0)?1:0;}

void in_delete() {
	if (in_cursorPos) {
		char b[100];
		char a[100];
	
		memcpy(b, in_displayText, in_cursorPos-1);
		memcpy(a, in_displayText+in_cursorPos, in_length-in_cursorPos);
		b[in_cursorPos-1] = '\0';
		a[in_length-in_cursorPos] = '\0';


		snprintf(in_displayText, 100, "%s%s", b, a);

		in_length--;
		in_cursorPos--;
	}
}

void in_setBlinkRate(int val) {
	in_blinkRate = val;
}

void in_draw() {
	if (in_frame%in_blinkRate>(in_blinkRate/2)) drawLine(in_display, *in_dimX, *in_dimY, in_cursorPos*9, in_y+in_height-4, in_cursorPos*9, in_y+in_height-16, 0x000000);
	
	drawText(in_display, *in_dimX, *in_dimY, in_fontData, in_displayText, 1, in_x, in_y+in_height-14, (in_selected)?0xFF0000:0x333333);
	drawLine(in_display, *in_dimX, *in_dimY, in_x, in_y, in_x, in_y+in_height, 0x000000);
	drawLine(in_display, *in_dimX, *in_dimY, in_x, in_y, in_x+in_width, in_y, 0x000000);
	drawLine(in_display, *in_dimX, *in_dimY, in_x+in_width, in_y+in_height, in_x, in_y+in_height, 0x000000);
	drawLine(in_display, *in_dimX, *in_dimY, in_x+in_width, in_y+in_height, in_x+in_width, in_y, 0x000000);

	if (in_selected) in_frame++;
	else in_frame = 0;
}
