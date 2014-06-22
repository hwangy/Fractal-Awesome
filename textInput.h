#include <stdlib.h>
#include <stdint.h>

static char* in_displayText;	//The actual display text
const unsigned char* in_fontData;
uint32_t* in_display;
int in_length;
int in_cursorPos;		//The position of the cursor
int in_selected;		//If the field is operable
int in_blinkRate;		//Blink rate of the cursor
int* in_dimX, *in_dimY;

int in_x;
int in_y;
int in_width;
int in_height;

int in_frame;

void in_init(uint32_t* display, const unsigned char* fontData, int* dimX, int* dimY, char* iS, int length, int x, int y, int w, int h);

void in_enterKey(char in);

void in_cursorPlus();

void in_cursorMinus();

void in_delete();

void in_setBlinkRate(int val);

void in_draw();

