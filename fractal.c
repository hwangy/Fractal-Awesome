#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#include "SDL_gfxPrimitives_font.h"
#include "draw.h"

#ifndef WINDOWS
	#include <X11/Xlib.h>
	#include <SDL_syswm.h>
	
	//Some stuff to allow window moving in *nix
	SDL_SysWMinfo info;
	XWindowAttributes attrs;
	Window root, parent, *children;
	unsigned int assigned;
	int drag_x, drag_y, new_x, new_y;
	int window_posX, window_posY;
#endif

const int rmax = 30, iterations = 100;

static SDL_Window* window;
SDL_Renderer* render;
SDL_Texture* texture;
SDL_Surface* surface;
uint32_t* display, *rawDisplay, *bufferedDisplay, *miniMap, *finalDisplay;
char* displayText;
const unsigned char* fontData = gfxPrimitivesFontdata;

//Threading for image buffering
sem_t generateBuff;
pthread_t threadID;
pthread_mutex_t mutex;
void thread(void* simon);
int updated = 0;
int tileRenderDistance = 5; 				//Odd numbers preferred
int progress = 0;

struct timespec start = {.tv_nsec = 0, .tv_sec = 0};
struct timespec end ={.tv_nsec = 0, .tv_sec = 0};
struct timespec p = {.tv_nsec = 0, .tv_sec = 0};

int borderTop = 15, borderBot = 3, borderLeft = 3, borderRight = 3;
int dimX, dimY;
int miniDimX, miniDimY, miniX, miniY;

int resizeMode = 0, mouseDown = 0, dragMode = 0;
int offX, offY;
int mouseX, mouseY, startX, startY;
int startX, startY, realEX, realEY;
double MinR, MaxR, MinI, MaxI;

int getCount(double x, double y);
void update();
void resize();
void recalc();
void shift(int fast);
void generateMiniMap();
void createBorder();

Window getWindow();

void dragResize(int w, int h);

int main(int argc, char** argv) {
	dimX = dimY = 600; //Cool C stuff

	displayText = malloc(sizeof(char)*100);

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	
	//Couldn't get system window resize events to work, going to make my own borders :P
	window = SDL_CreateWindow("Fractal", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
			dimX+borderLeft+borderRight, dimY+borderTop+borderBot, SDL_WINDOW_BORDERLESS);

	render = SDL_CreateRenderer(window, -1, 0);
	texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
			dimX+borderLeft+borderRight, dimY+borderTop+borderBot);
	
	display = malloc(sizeof(uint32_t)*dimX*dimY);
	rawDisplay = malloc(sizeof(uint32_t)*dimX*dimY);
	bufferedDisplay = malloc(sizeof(uint32_t)*dimX*dimY*tileRenderDistance*tileRenderDistance);
	finalDisplay = malloc(sizeof(uint32_t)*(dimX+borderLeft+borderRight)*(dimY+borderBot+borderTop));
	createBorder();

#ifndef WINDOWS
	SDL_VERSION(&info.version);
	if (SDL_GetWindowWMInfo(window, &info) > 0 && info.subsystem == SDL_SYSWM_X11) {
		XQueryTree(info.info.x11.display, info.info.x11.window, &root, &parent, &children, &assigned);
		if (children != NULL) XFree(children);
		XGetWindowAttributes(info.info.x11.display, root, &attrs);
		printf("DISPLAY RES %dX%d\n", attrs.width, attrs.height);
	}
#endif
	

	//Mini map variable initialization
	miniDimX = 150; miniDimY = 150; miniX = dimX-miniDimX; miniY = dimY-miniDimY; miniMap = malloc(sizeof(uint32_t)*miniDimX*miniDimY);
	
	MinR = -2.0;
	MaxR = 2.0;
	MinI = -2.0;
	MaxI = MinI + (MaxR - MinR)*(dimY/dimX);
	recalc();
	
	sem_init(&generateBuff, 0, 1);
	pthread_create(&threadID, NULL, (void* (*)(void*))&thread, (int*)1);

	while (1) {
		clock_gettime(CLOCK_MONOTONIC, &start);

		SDL_Event events;
		while (SDL_PollEvent(&events)) {
			if (events.type == SDL_QUIT) {
				SDL_DestroyWindow(window);
				SDL_Quit();
				return 0;
			}
			if (events.type == SDL_KEYDOWN && events.key.keysym.sym == SDLK_LSHIFT) resizeMode = 1;
			else if (events.type == SDL_KEYDOWN && events.key.keysym.sym == SDLK_r) {updated = 0; recalc(); sem_post(&generateBuff);}
			else if (events.type == SDL_KEYUP && events.key.keysym.sym == SDLK_LSHIFT) resizeMode = 0;
			if (events.type == SDL_MOUSEBUTTONDOWN) {
				mouseDown = 1;
				SDL_GetMouseState(&startX, &startY);

				//Close window button
				if (startX > (dimX+borderLeft+borderRight-8-3-3) && startY < 3+8+3) {
					SDL_DestroyWindow(window);
					SDL_Quit();
					return 0;
				} else if (startY < borderTop) dragMode = 1;

			} else if (events.type == SDL_MOUSEBUTTONUP) {
				mouseDown = 0;
				SDL_GetMouseState(&mouseX, &mouseY);
				if (resizeMode && updated) {
					updated = 0;
					resize();
					recalc();
				
					sem_post(&generateBuff);
				}

				resizeMode = 0;
				dragMode = 0;
			}
		}
		SDL_GetMouseState(&mouseX, &mouseY);

		update();

		if (mouseDown && !resizeMode && !dragMode) {
			SDL_GetMouseState(&mouseX, &mouseY);
			if (mouseX >= miniX && mouseX < miniX + miniDimX &&
			    mouseY >= miniY && mouseY < miniY + miniDimY) shift(1);
			else shift(0);
		} else if (mouseDown && !resizeMode && dragMode) {
#ifndef WINDOWS
			int rtx, rty;
			Window child;
			unsigned int mnt;
			XQueryPointer(info.info.x11.display, info.info.x11.window, &root, &child, &drag_x, &drag_y, &rtx, &rty, &mnt);
			
			XGetWindowAttributes(info.info.x11.display, info.info.x11.window, &attrs);
			XTranslateCoordinates(info.info.x11.display, info.info.x11.window, root, 0, 0, &attrs.x, &attrs.y, &child);
#endif
		}

			

		clock_gettime(CLOCK_MONOTONIC, &end);
		long int sleep = 25000000 - (end.tv_nsec - start.tv_nsec);
		if (sleep > 0) {
			p.tv_nsec = sleep;
			nanosleep(&p, NULL);
		}

		if (mouseDown && !resizeMode && dragMode) {
#ifndef WINDOWS
			int rtx, rty;
			Window child;
			unsigned int mnt;
			XQueryPointer(info.info.x11.display, info.info.x11.window, &root, &child, &new_x, &new_y, &rtx, &rty, &mnt);
			XMoveWindow(info.info.x11.display, info.info.x11.window, attrs.x + (new_x - drag_x), attrs.y + (new_y - drag_y));
#endif
		}
	}

	return 0;
}

void thread(void* simon) {
	uint32_t* toTransmit = malloc(sizeof(uint32_t)*dimX*dimY*tileRenderDistance*tileRenderDistance);
	double buffMinR, buffMaxR, buffMinI, buffMaxI, ref, imf;
	double complex val, z;
	int count = 0;
	uint32_t color;

	struct timespec pause = {.tv_nsec = 0, .tv_sec = 0};

	//If tileRenderDistance is not odd, this will make it odd
	if (!tileRenderDistance%2) tileRenderDistance++;
	float mult = (float)(tileRenderDistance-1)/2.0f;
	
	while (1) {
		if (sem_trywait(&generateBuff) == 0) {
			progress = 0;
			buffMinR = MinR - (MaxR - MinR)*mult;
			buffMaxR = MaxR + (MaxR - MinR)*mult;
			buffMinI = MinI - (MaxI - MinI)*mult;
			buffMaxI = MaxI + (MaxI - MinI)*mult;

			ref = (buffMaxR-buffMinR)/(dimX*tileRenderDistance); imf = (buffMaxI - buffMinI)/(dimY*tileRenderDistance);
	
			int x, y;
			for (x = 0; x < dimX*tileRenderDistance; x++) {
				for (y = 0; y < dimY*tileRenderDistance; y++) {
					progress = (int)(100.0f*(float)x/(dimX*tileRenderDistance));
					val = (buffMinR+x*ref) + (buffMaxI-y*imf)*I;
					z = val;
					count = 0;
					
					while (count < iterations && creal(z) + cimag(z) < rmax) {
					       z = z*z + val;
					       count++;
					}
					color = 0xFFFFFF*((count == iterations)?0:((double)count/iterations));
					
					toTransmit[x+y*(dimX*tileRenderDistance)] = color;
				}
			}

			pthread_mutex_lock(&mutex);
			bufferedDisplay = toTransmit;
			updated = 1;
			pthread_mutex_unlock(&mutex);
			generateMiniMap();
		}
	}
}

uint32_t getColor(int x, int y) {
	double ref = (MaxR-MinR)/dimX, imf = (MaxI - MinI)/dimY;

	double complex val = (MinR+x*ref) + (MaxI-y*imf)*I;
	double complex z = val;
	int count = 0;

	while (count < iterations && creal(z) + cimag(z) < rmax) {
	       z = z*z + val;
	       count++;
	}

	uint32_t color = 0xFFFFFF*((count == iterations)?0:((double)count/iterations));
}

void update() {

	int x, y;
	for (x = 0; x < dimX; x++) {
		for (y = 0; y < dimY; y++) {
			display[x + y*dimX] = (x+offX > dimX*tileRenderDistance || y+offY > dimY*tileRenderDistance ||
					      x+offX < 0 || y+offY < 0)?
						0x333333:
						bufferedDisplay[(x+offX) + (y+offY)*dimX*tileRenderDistance];
		}
	}

	//Create zoom box
	if (mouseDown && resizeMode) {
		double proportion = (double)dimX/dimY;
		realEX = (abs(mouseX - startX) > abs(mouseY - startY))?abs(mouseX-startX):(int)((double)abs(mouseY - startY)*proportion);
		realEY = (abs(mouseY - startY) > abs(mouseX - startX))?abs(mouseY-startY):(int)((double)abs(mouseX - startX)*(1.0f/proportion));
		if (mouseX < startX) realEX*=-1;
		if (mouseY < startY) realEY*=-1;
		realEX += startX; realEY += startY;

		drawLine(display, dimX, dimY, startX, startY, realEX, startY, 0xFFFFFF);
		drawLine(display, dimX, dimY, startX, startY, startX, realEY, 0xFFFFFF);
		drawLine(display, dimX, dimY, realEX, startY, realEX, realEY, 0xFFFFFF);
		drawLine(display, dimX, dimY, startX, realEY, realEX, realEY, 0xFFFFFF);
	}
	
	//Show mini map
	for (x = miniX; x < miniX + miniDimX; x++) {
		for (y = miniY; y < miniY + miniDimY; y++) {
			display[x + y*dimX] = (x == miniX || y == miniY || 
					       x == miniX+miniDimX-1 || y == miniY+miniDimY-1)?0x000000:miniMap[(x-miniX) + (y-miniY)*miniDimX];
		}
	}
	//Create focus box
	int sX = miniX + ((float)offX/(dimX*tileRenderDistance))*miniDimX;
	int sY = miniY + ((float)offY/(dimY*tileRenderDistance))*miniDimY;
	int wX = miniDimX/tileRenderDistance;
	int wY = miniDimY/tileRenderDistance;
	drawLine(display, dimX, dimY, sX, sY, sX+wX, sY, 0xFF0000);
	drawLine(display, dimX, dimY, sX, sY, sX, sY+wY, 0xFF0000);
	drawLine(display, dimX, dimY, sX+wX, sY, sX+wX, sY+wY, 0xFF0000);
	drawLine(display, dimX, dimY, sX, sY+wY, sX+wX, sY+wY, 0xFF0000);
		

	//Show buffer info
	if (!updated) {
		snprintf(displayText, 100, "BUFFERING: %d%%", progress);
		drawText(display, dimX, dimY, fontData, displayText, 1, 0, 0, 0xFFFFFF);
	}

	//Now put the display onto the final one, which has borders and such
	for (x = 0; x < dimX; x++) {
		for (y = 0; y < dimY; y++) finalDisplay[(x+borderLeft)+(y+borderTop)*(dimX+borderLeft+borderRight)] = display[x+y*dimX];
	}

	SDL_UpdateTexture(texture, NULL, finalDisplay, (dimX+borderLeft+borderRight)*4);
	SDL_RenderClear(render);
	SDL_RenderCopy(render, texture, 0, 0);
	SDL_RenderPresent(render);
}

void resize() {
	if (startX > realEX) {
		int tmp = startX;
		startX = realEX;
		realEX = tmp;
	}
	if (startY > realEY) {
		int tmp = startY;
		startY = realEY;
		realEY = tmp;
	}
	double NMinR = MinR + ((double)startX/dimX)*(MaxR - MinR);
	double NMaxR = NMinR + ((double)realEX - startX)/dimX*(MaxR - MinR);
	double NMinI = MinI + ((double)(dimY-realEY)/dimY)*(MaxI - MinI);
	double NMaxI = NMinI + (NMaxR - NMinR)*((double)dimY/dimX);
	MinR = NMinR; MaxR = NMaxR;
	MinI = NMinI; MaxI = NMaxI;
}

void recalc() {
	int x, y;
	for (x = 0; x < dimX*tileRenderDistance; x++) {
		for (y = 0; y < dimY*tileRenderDistance; y++) {
			if (x >= dimX*(tileRenderDistance-1.0f)/2.0f &&
			    x <  dimX*(tileRenderDistance+1.0f)/2.0f &&
			    y >= dimY*(tileRenderDistance-1.0f)/2.0f &&
			    y <  dimY*(tileRenderDistance+1.0f)/2.0f) {
				bufferedDisplay[x+y*dimX*tileRenderDistance] = getColor(x-dimX*(tileRenderDistance-1)/2, y-dimY*(tileRenderDistance-1)/2);
			} else {
				bufferedDisplay[x+y*dimX*tileRenderDistance] = 0x333333;
			}
		}
	}
	
	offX = dimX*(tileRenderDistance-1)/2;
	offY = dimY*(tileRenderDistance-1)/2;
	
	generateMiniMap();	
}

void shift(int fast) {
	double ShiftR = (double)(mouseX - startX)/dimX*(MaxR - MinR);
	double ShiftI = (double)(mouseY - startY)/dimY*(MaxI - MinI);
	
	if (fast) {
		ShiftR = ShiftR * -1.0 * (tileRenderDistance*dimX/miniDimX);
		ShiftI = ShiftI * -1.0 * (tileRenderDistance*dimY/miniDimY);
	}

	MinR -= ShiftR; MaxR -= ShiftR;
	MinI += ShiftI; MaxI += ShiftI;

	offX -= (fast)?tileRenderDistance*-1*(mouseX - startX)*(dimX/miniDimX):(mouseX - startX);
	offY -= (fast)?tileRenderDistance*-1*(mouseY - startY)*(dimY/miniDimY):(mouseY - startY);
	
	startX = mouseX;
	startY = mouseY;


	//recalc();
	update();
}

void generateMiniMap() {
	//Obviously works best where 0 = dimX mod miniDimX
	int sfX = (int)(dimX*tileRenderDistance)/miniDimX;
	int sfY = (int)(dimY*tileRenderDistance)/miniDimY;

	//Calculate average pixel color for each segment
	int x, y;
	int aC = 0;
	for (y = 0; y < miniDimY; y++) {
		for (x = 0; x < miniDimX; x++) {
			int sX, sY;
			uint32_t avgColor = 0x000000;
			aC = 0;
			for (sX	= 0; sX < sfX; sX++) {
				for (sY = 0; sY < sfY; sY++) {
					//refactor average
					avgColor *= aC/++aC;
					avgColor += bufferedDisplay[(x*sfX+sX) + (y*sfY+sY)*dimY*tileRenderDistance]/aC;
				}
			}
			miniMap[x+y*miniDimX] = avgColor;
		}
	}
}

void dragResize(int w, int h) {
	
}

void createBorder() {
	int x, y;
	for (x = 0; x < dimX + borderLeft+borderRight; x++) {
		for (y = 0; y < dimY + borderTop + borderBot; y++) {
			finalDisplay[x + y*(dimX+borderLeft+borderRight)] = 0x444444;
		}
	}
	drawText(finalDisplay, dimX+borderLeft+borderRight, dimY+borderTop+borderBot, fontData, "X", 1, (dimX+borderLeft+borderRight-8-3), 3, 0xFFFFFF);
}

Window getWindow() {
	int toRevert;
	Window w;
	XGetInputFocus(info.info.x11.display, &w, &toRevert);

	return w;
}
