#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "draw.h"

void drawBox(uint32_t* field, int dimX, int dimY, int x, int y, int w, int h, uint32_t color){ 
	int i, j;
	for (i = y; i < y+h; i++) {
		for (j = x; j < x+w; j++) {
			field[j+i*dimX] = color;
		}
	}
}

void drawText(uint32_t* field, int dimX, int dimY, const unsigned char* fontData, char* string, int size, int x, int y, uint32_t color) {
	int i = 0, j = 0;
	int test;
	int x2 = 0, y2 = 0;
	int tempX, tempY;
	uint8_t row;
	while (string[j] != '\0') {
		//i++;
		if (string[j] == '\n') {
			i = 0;
			j++;
			y+= size*9;
			continue;
		}
		tempY = y;
		tempX = x;
		for (y2 = 0; y2 < 8; y2++) {
			row = fontData[8*string[j]+y2];
			test = 128;
			tempX = x+i*9*size;
			for (x2 = 0; x2 < 8; x2++) {
				if (row & test) drawBox(field, dimX, dimY, tempX, tempY, size, size, color);
				tempX+=size;
				test/=2;
			}
			tempY += size;
		}
		j++;
		i++;
	}
}

void drawCircle(uint32_t* field, int dimX, int dimY, int x, int y, int r, uint32_t color) {
	int error = 1 - r, derY = -2*r, derX = 0;
	field[x+(y+r)*dimX] = color;
	field[x+(y-r)*dimX] = color;
	field[(x+r)+y*dimX] = color;
	field[(x-r)+y*dimX] = color;
	int i = r, j = 0;
	while (j < i) {
		if (error >= 0) {
			i--;
			derY += 2;
			error += derY;
		}

		j++;
		derX += 2;
		error += derX + 1;
		field[(x+j)+(y+i)*dimX] = color;
		field[(x-j)+(y+i)*dimX] = color;
		field[(x+j)+(y-i)*dimX] = color;
		field[(x-j)+(y-i)*dimX] = color;
		field[(x+i)+(y+j)*dimX] = color;
		field[(x-i)+(y+j)*dimX] = color;
		field[(x+i)+(y-j)*dimX] = color;
		field[(x-i)+(y-j)*dimX] = color;
	}
}

void drawLine(uint32_t* field, int dimX, int dimY, int x0, int y0, int x1, int y1, uint32_t color) {
	int dx = abs(x1-x0), sx = x0<x1?1:-1;
	int dy = abs(y1-y0), sy = y0<y1?1:-1;
	int error = ((dx>dy)?dx:-dy)/2, error2;

	while (1) {
		field[x0+y0*dimX] = color;
		if (x0==x1 && y0==y1) break;
		error2 = error;
		if (error2 > -dx) { error -= dy; x0 += sx; }
		if (error2 < dy) { error += dx; y0 += sy; }
	}
}
