#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <Imlib2.h>
#include <string.h>

#include "SDL_gfxPrimitives_font.h"
#include "draw.h"

#ifndef WINDOWS
	#include <X11/Xlib.h>
	#include <X11/cursorfont.h>
	#include <SDL_syswm.h>
	#include <dirent.h>
	#include <sys/types.h>
	#include <errno.h>

	/*
	 * Directory shenanigans that only work with posix
	 * compliant OS's, such as Linux and Mac OS
	 */
	typedef struct dirent dirent;
	dirent* dirList;
	DIR* cd;
	pthread_t dirThreadID;
	void dirThread(void* simon);
	
	//Some stuff to allow window moving in *nix
	SDL_SysWMinfo info;
	XWindowAttributes attrs;
	Window root, parent, *children;
	unsigned int assigned;
	int drag_x, drag_y, new_x, new_y;
	
	//Pretty much used for nothing except placeholders	
	int rtx, rty;
	Window child;
	unsigned int mnt;

	Cursor* xCursors;
	int isDragL = 0, isDragR = 0, isDragB = 0, isDragT = 0;					//THERE
												//ARE
	int currentID = 2;									//JUST
#endif												//TOO
												//MANY
#define RENDER_DISTANCE 5									//VVVARRRIAAABBLLEESS

struct WOO_Window {
	int dimX;
	int dimY;
	int borderLeft;
	int borderTop;
	int borderRight;
	int borderBot;
	SDL_Window* window;
	SDL_Renderer* render;
	SDL_Texture* texture;

	uint32_t* pubDisplay;
	uint32_t* display;
	uint32_t* buffer;

	char* title;
	int len;
	char* sub;
	
	int bufferX;
	int bufferY;
#ifndef WINDOWS
	XWindowAttributes attrs;
	Window root, parent, *children;
	int focus;

	uint32_t id;
#endif
};
typedef struct WOO_Window WOO_Window;

const int rmax = 30, iterations = 100;

WOO_Window mw;
WOO_Window sa;

uint32_t* miniMap;
char* displayText;
const unsigned char* fontData = gfxPrimitivesFontdata;

//Threading for image buffering
sem_t generateBuff;
pthread_t threadID;
pthread_mutex_t mutex;
void thread(void* simon);
int updated = 0;
int progress = 0;

struct timespec start = {.tv_nsec = 0, .tv_sec = 0};
struct timespec end ={.tv_nsec = 0, .tv_sec = 0};
struct timespec p = {.tv_nsec = 0, .tv_sec = 0};

int oDimX, oDimY;
int miniDimX, miniDimY, miniX, miniY;

int resizeMode = 0, mouseDown = 0, dragMode = 0;
int offX, offY;
int mouseX, mouseY, startX, startY;
int startX, startY, realEX, realEY;
double MinR, MaxR, MinI, MaxI;

WOO_Window* CD;

int getCount(double x, double y);
uint32_t getColor(int x, int y);
uint32_t getColor_debug(int x, int y);		//VERY verbose version of getColor
void update();
void resize();
void recalc();
void shift(int fast);
void generateMiniMap();
void createBorder(WOO_Window* win);
void createSaveAs();
void saveImg();

int main(int argc, char** argv) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	oDimX = oDimY = 600; //Cool C stuff

	int tmp_var[3] = {600, 15, 3};
	
	displayText = malloc(sizeof(char)*100);

	//Couldn't get system window resize events to work, going to make my own borders :P
	SDL_Window* window = SDL_CreateWindow("Fractal", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
			tmp_var[0]+2*tmp_var[2], tmp_var[0]+tmp_var[1]+tmp_var[2], SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE);

	SDL_Renderer* render = SDL_CreateRenderer(window, -1, 0);
	SDL_Texture* texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
			tmp_var[0]+2*tmp_var[2], tmp_var[0]+tmp_var[1] + tmp_var[2]);

	//display = malloc(sizeof(uint32_t)*tmp_var[0]*tmp_var[0]);
	
	/**
	 * Need to conver program to be relatively OO and use this 
	 * window struct. This will be helpful when making multiple windows,
	 * particuarily the Save As dialog for the fractal
	 *
	 * Weird Thing:
	 * 	Can't initialize and use variable swithin the structure
	 * 	initialization. Kind makes sense, but it's annoying.
	 * 	For now, I'm going to have to just declare tmp veriables
	 * 	outside of the structure and set them with those :P
	 *
	 * 	Not what I wanted to do....
	 */
	
	mw = (WOO_Window) {
		.dimX = tmp_var[0],
		.dimY = tmp_var[0],
		.borderTop = tmp_var[1],
		.borderLeft = tmp_var[2],
		.borderRight = tmp_var[2],
		.borderBot = tmp_var[2],
		.window = window,
		.render = render,
		.texture = texture,
		.pubDisplay = malloc(sizeof(uint32_t)*(tmp_var[0]+tmp_var[2]+tmp_var[2])*(tmp_var[0]+tmp_var[1]+tmp_var[2])),
		.display = malloc(sizeof(uint32_t)*(tmp_var[0] * tmp_var[0])),
		.bufferX = RENDER_DISTANCE * tmp_var[0],
	       	.bufferY = RENDER_DISTANCE * tmp_var[0],	
		.buffer = malloc(sizeof(uint32_t)*(RENDER_DISTANCE*tmp_var[0] * RENDER_DISTANCE*tmp_var[0])),
		.title = malloc(100),
		.len = 14,
		.sub = malloc(100),
		.focus = 1,
		.id = currentID
	};
	

	CD = &mw;
	currentID++;

	memcpy(mw.title, "FractalAwesome", 15);
	memcpy(mw.sub, "Save As", 8);

	createBorder(&mw);

#ifndef WINDOWS
	SDL_VERSION(&info.version);
	if (SDL_GetWindowWMInfo(mw.window, &info) > 0 && info.subsystem == SDL_SYSWM_X11) {
		XQueryTree(info.info.x11.display, info.info.x11.window, &mw.root, &mw.parent, &mw.children, &assigned);
		if (mw.children != NULL) XFree(mw.children);
		XGetWindowAttributes(info.info.x11.display, mw.root, &mw.attrs);
	}

	/**
	 * Creates cursors for all 4 border adjustments
	 * and the general cursror
	 *
	 * Top Border ->	XC_top_side	138
	 * Left Border ->	XC_left_side	70
	 * Right Border ->	XC_right_side	96
	 * Bottom Border ->	XC_bottom_side	16
	 * General ->		XC_left_ptr	68
	 */
	xCursors = malloc(sizeof(Cursor)*5);
	xCursors[0] = XCreateFontCursor(info.info.x11.display, 68);
	xCursors[1] = XCreateFontCursor(info.info.x11.display, 138);
	xCursors[2] = XCreateFontCursor(info.info.x11.display, 70);
	xCursors[3] = XCreateFontCursor(info.info.x11.display, 96);
	xCursors[4] = XCreateFontCursor(info.info.x11.display, 16);
#endif
	

	//Mini map variable initialization
	miniDimX = 150; miniDimY = 150; miniX = mw.dimX-miniDimX; miniY = mw.dimY-miniDimY; miniMap = malloc(sizeof(uint32_t)*miniDimX*miniDimY);
	
	MinR = -2.0;
	MaxR = 2.0;
	MinI = -2.0;
	MaxI = MinI + (MaxR - MinR)*(mw.dimY/mw.dimX);
	recalc();
	
	sem_init(&generateBuff, 0, 1);
	pthread_create(&threadID, NULL, (void* (*)(void*))&thread, (int*)1);
	//pthread_create(&dirThreadID, NULL, (void* (*)(void*))&dirThread, (int*)1);

	int startDrag = 0;

	while (1) {
		clock_gettime(CLOCK_MONOTONIC, &start);

		SDL_Event events;
		while (SDL_PollEvent(&events)) {

			if (events.type == SDL_QUIT) {
				SDL_DestroyWindow(CD->window);
				SDL_Quit();
				return 0;
			}
			
			if (events.type == SDL_KEYDOWN && events.key.keysym.sym == SDLK_LSHIFT) resizeMode = 1;
			else if (events.type == SDL_KEYDOWN && events.key.keysym.sym == SDLK_r) {updated = 0; recalc(); sem_post(&generateBuff);}
			else if (events.type == SDL_KEYUP && events.key.keysym.sym == SDLK_LSHIFT) resizeMode = 0;
			if (events.type == SDL_MOUSEBUTTONDOWN) {
				mouseDown = 1;
				SDL_GetMouseState(&startX, &startY);

				//Close CD->window button
				if (startX > (CD->dimX+CD->borderLeft+CD->borderRight-8-3-3) && startY < 3+8+3) {
					SDL_DestroyWindow(CD->window);
					SDL_Quit();
					return 0;
				} else if (startX > 14*8 + 25 && startX < 14*8 + 25 + 7*8 && startY < 15 && mw.focus) {
					//Save as dialogue selected
					createSaveAs();
					mouseDown = 0;
					continue;
				} else if ((startY < CD->borderTop || startY > CD->borderTop+CD->dimY) ||
					   (startX < CD->borderLeft || startX > CD->borderLeft+CD->dimX)) {	//Testing to see if grabbing border
					dragMode = 1;
					startDrag = 1;
				}

			} else if (events.type == SDL_MOUSEBUTTONUP) {
				mouseDown = 0;
				SDL_GetMouseState(&mouseX, &mouseY);
				if (resizeMode && updated && mw.focus) {
					updated = 0;
					resize();
					recalc();
				
					sem_post(&generateBuff);
				}

				if (isDragB+isDragL+isDragR+isDragT > 0 && mw.focus) {
					//Adjust maximums
					if (isDragB) {
						double ShiftI = (double)(CD->dimY - oDimY)/oDimY*(MaxI - MinI);
						MinI -= ShiftI;
					}
					if (isDragR) {
						double ShiftR = (double)(CD->dimX - oDimX)/oDimX*(MaxR - MinR);
						MaxR += ShiftR;
					}

					CD->buffer = realloc(CD->buffer, sizeof(uint32_t)*CD->dimX*CD->dimY*RENDER_DISTANCE*RENDER_DISTANCE);
					updated = 0;
					recalc();
					sem_post(&generateBuff);
				}

				resizeMode = 0;
				dragMode = 0;
				isDragB = isDragL = isDragR = isDragT = 0;
			}

		}
		SDL_GetMouseState(&mouseX, &mouseY);
		if (mouseY > CD->dimY+CD->borderTop)		XDefineCursor(info.info.x11.display, info.info.x11.window, xCursors[4]);
		else if (mouseY < 2)			XDefineCursor(info.info.x11.display, info.info.x11.window, xCursors[1]);
		else if (mouseX < CD->borderLeft)		XDefineCursor(info.info.x11.display, info.info.x11.window, xCursors[2]);
		else if (mouseX > CD->borderLeft+CD->dimX)	XDefineCursor(info.info.x11.display, info.info.x11.window, xCursors[3]);
		else 					XDefineCursor(info.info.x11.display, info.info.x11.window, xCursors[0]);

		update();

		if (mouseDown && !resizeMode && !dragMode) {
			SDL_GetMouseState(&mouseX, &mouseY);
			if (mouseX >= miniX && mouseX < miniX + miniDimX &&
			    mouseY >= miniY && mouseY < miniY + miniDimY && mw.focus) shift(1);
			else shift(0);
		} else if (mouseDown && !resizeMode && dragMode) {
#ifndef WINDOWS
			if (startDrag) XQueryPointer(info.info.x11.display, info.info.x11.window, &CD->root, &child, &drag_x, &drag_y, &rtx, &rty, &mnt);
			XGetWindowAttributes(info.info.x11.display, info.info.x11.window, &CD->attrs);
			XTranslateCoordinates(info.info.x11.display, info.info.x11.window, CD->root, 0, 0, &CD->attrs.x, &CD->attrs.y, &child);
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
			startDrag = 0;
			XQueryPointer(info.info.x11.display, info.info.x11.window, &CD->root, &child, &new_x, &new_y, &rtx, &rty, &mnt);
			if (startY > 2 && startY < CD->borderTop) {
				XMoveWindow(info.info.x11.display, info.info.x11.window, 
						CD->attrs.x + (new_x - drag_x), CD->attrs.y + (new_y - drag_y));
			} else if (drag_y > CD->borderTop + CD->dimY + CD->attrs.y || isDragB) {
				XMoveResizeWindow(info.info.x11.display, info.info.x11.window, 
						CD->attrs.x, CD->attrs.y, CD->attrs.width, CD->attrs.height + (new_y - drag_y));
				CD->dimY = CD->attrs.height - CD->borderTop - CD->borderBot;
				isDragB = 1;
			} else if (drag_x > CD->dimX + CD->borderLeft + CD->attrs.x || isDragR) {
				XMoveResizeWindow(info.info.x11.display, info.info.x11.window, 
						CD->attrs.x, CD->attrs.y, CD->attrs.width + (new_x - drag_x), CD->attrs.height);
				CD->dimX = CD->attrs.width - CD->borderLeft - CD->borderRight;
				isDragR = 1;
			}

			CD->display = realloc(CD->display, sizeof(uint32_t)*CD->dimX*CD->dimY);
			CD->pubDisplay = realloc(CD->pubDisplay, 
					sizeof(uint32_t)*(CD->dimX+CD->borderLeft+CD->borderRight)*(CD->dimY+CD->borderBot+CD->borderTop));
	
			CD->texture = SDL_CreateTexture(CD->render, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
				CD->dimX+CD->borderLeft+CD->borderRight, CD->dimY+CD->borderTop+CD->borderBot);

			drag_x = new_x;
			drag_y = new_y;
#endif
		}
	}

	return 0;
}

#ifndef WINDOWS
void dirThread(void* simon) {
	char* cmd = malloc(100);
	char* arg = malloc(100);

	printf("%s: %d\n", "[EACCES]", EACCES);
	printf("%s: %d\n", "[ELOOP]", ELOOP);
	printf("%s: %d\n", "[ENAMETOOLONG]", ENAMETOOLONG);
	printf("%s: %d\n", "[ENOENT]", ENOENT);
	printf("%s: %d\n", "[ENOTDIR]", ENOTDIR);
	printf("%s: %d\n", "[EMFILE]", EMFILE);
	printf("%s: %d\n", "[ENFILE]", ENFILE);

	while (1) {
		printf("Enter Command:   ");
		scanf("%s", cmd);
		
		if (!strcmp(cmd, "cd")) {
			printf("Enter Arguments: ");
			scanf("%s", arg);
			
			cd = opendir(arg);
			if (cd == NULL) printf("Cannot Open Directory, ERROR: %d\n", errno);
			else {
				while ((dirList = readdir(cd)) != NULL) {
					printf("%s\n", dirList->d_name);
				}
			}
		} else if (!strcmp(cmd, "ls")) {
			rewinddir(cd);
			while ((dirList = readdir(cd)) != NULL) {
				printf("%s\n", dirList->d_name);
			}
		}
	}
}
#endif

void thread(void* simon) {
	uint32_t* toTransmit = malloc(sizeof(uint32_t)*mw.dimX*mw.dimY*RENDER_DISTANCE*RENDER_DISTANCE);
	double buffMinR, buffMaxR, buffMinI, buffMaxI, ref, imf;
	double complex val, z;
	int count = 0;
	uint32_t color;

	struct timespec pause = {.tv_nsec = 0, .tv_sec = 0};

	//If RENDER_DISTANCE is not odd, this will make it odd
	float mult = (float)(RENDER_DISTANCE-1)/2.0f;
	
	while (1) {
		if (sem_trywait(&generateBuff) == 0) {
			if (oDimX != mw.dimX || oDimY != mw.dimY) 
				toTransmit = malloc(sizeof(uint32_t)*mw.dimX*mw.dimY*RENDER_DISTANCE*RENDER_DISTANCE);
			progress = 0;
			buffMinR = MinR - (MaxR - MinR)*mult;
			buffMaxR = MaxR + (MaxR - MinR)*mult;
			buffMinI = MinI - (MaxI - MinI)*mult;
			buffMaxI = MaxI + (MaxI - MinI)*mult;

			ref = (buffMaxR-buffMinR)/(mw.dimX*RENDER_DISTANCE); imf = (buffMaxI - buffMinI)/(mw.dimY*RENDER_DISTANCE);
	
			int x, y;
			for (x = 0; x < mw.dimX*RENDER_DISTANCE; x++) {
				for (y = 0; y < mw.dimY*RENDER_DISTANCE; y++) {
					progress = (int)(100.0f*(float)x/(mw.dimX*RENDER_DISTANCE));
					val = (buffMinR+x*ref) + (buffMaxI-y*imf)*I;
					z = val;
					count = 0;
					
					while (count < iterations && creal(z) + cimag(z) < rmax) {
					       z = z*z + val;
					       count++;
					}
					color = 0xFFFFFF*((count == iterations)?0:((double)count/iterations));
					
					toTransmit[x+y*(mw.dimX*RENDER_DISTANCE)] = color;
				}
			}

			pthread_mutex_lock(&mutex);
			mw.buffer = toTransmit;
			updated = 1;
			pthread_mutex_unlock(&mutex);
			generateMiniMap();

			oDimX = mw.dimX;
			oDimY = mw.dimY;
		}
	}
}

uint32_t getColor(int x, int y) {
	double ref = (MaxR-MinR)/mw.dimX, imf = (MaxI - MinI)/mw.dimY;

	double complex val = (MinR+x*ref) + (MaxI-y*imf)*I;
	double complex z = val;
	int count = 0;

	while (count < iterations && creal(z) + cimag(z) < rmax) {
	       z = z*z + val;
	       count++;
	}

	uint32_t color = 0xFFFFFF*((count == iterations)?0:((double)count/iterations));
	return color;
}

uint32_t getColor_debug(int x, int y) {
	double ref = (MaxR-MinR)/mw.dimX, imf = (MaxI - MinI)/mw.dimY;
	
	printf("REF: %f\nIMF: %f\n", ref, imf);

	double complex val = (MinR+x*ref) + (MaxI-y*imf)*I;
	double complex z = val;
	int count = 0;
	
	while (count < iterations && creal(z) + cimag(z) < rmax) {
		printf("%f, %f\n", creal(z), cimag(z));
	       z = z*z + val;
	       count++;
	}

	uint32_t color = 0xFFFFFF*((count == iterations)?0:((double)count/iterations));
	return color;
}

void update() {

	int x, y;
	for (x = 0; x < mw.dimX; x++) {
		for (y = 0; y < mw.dimY; y++) {
			mw.display[x + y*mw.dimX] = (x+offX > oDimX*RENDER_DISTANCE || y+offY > oDimY*RENDER_DISTANCE ||
					      x+offX < 0 || y+offY < 0)?
						0x333333:
						mw.buffer[(x+offX) + (y+offY)*oDimX*RENDER_DISTANCE];
		}
	}

	//Create zoom box
	if (mouseDown && resizeMode) {
		double proportion = (double)mw.dimX/mw.dimY;
		realEX = (abs(mouseX - startX) > abs(mouseY - startY))?
			abs(mouseX-startX):(int)((double)abs(mouseY - startY)*proportion);
		realEY = (abs(mouseY - startY) > abs(mouseX - startX))?
			abs(mouseY-startY):(int)((double)abs(mouseX - startX)*(1.0f/proportion));
		if (mouseX < startX) realEX*=-1;
		if (mouseY < startY) realEY*=-1;
		realEX += startX; realEY += startY;

		drawLine(mw.display, mw.dimX, mw.dimY, startX, startY, realEX, startY, 0xFFFFFF);
		drawLine(mw.display, mw.dimX, mw.dimY, startX, startY, startX, realEY, 0xFFFFFF);
		drawLine(mw.display, mw.dimX, mw.dimY, realEX, startY, realEX, realEY, 0xFFFFFF);
		drawLine(mw.display, mw.dimX, mw.dimY, startX, realEY, realEX, realEY, 0xFFFFFF);
	}

	miniX = mw.dimX - miniDimX;
	miniY = mw.dimY - miniDimY;
	//Show mini map
	for (x = miniX; x < miniX + miniDimX; x++) {
		for (y = miniY; y < miniY + miniDimY; y++) {
			mw.display[x + y*mw.dimX] = (x == miniX || y == miniY || 
						x == miniX+miniDimX-1 || y == miniY+miniDimY-1)?
						0x000000:miniMap[(x-miniX) + (y-miniY)*miniDimX];
		}
	}
	//Create focus box
	int sX = miniX + ((float)offX/(oDimX*RENDER_DISTANCE))*miniDimX;
	int sY = miniY + ((float)offY/(oDimY*RENDER_DISTANCE))*miniDimY;
	//int wX = miniDimX/RENDER_DISTANCE;
	//int wY = miniDimY/RENDER_DISTANCE;
	int wX = (float)mw.dimX/(oDimX*RENDER_DISTANCE)*miniDimX;
	int wY = (float)mw.dimY/(oDimY*RENDER_DISTANCE)*miniDimY;
	drawLine(mw.display, mw.dimX, mw.dimY, sX, sY, sX+wX, sY, 0xFF0000);
	drawLine(mw.display, mw.dimX, mw.dimY, sX, sY, sX, sY+wY, 0xFF0000);
	drawLine(mw.display, mw.dimX, mw.dimY, sX+wX, sY, sX+wX, sY+wY, 0xFF0000);
	drawLine(mw.display, mw.dimX, mw.dimY, sX, sY+wY, sX+wX, sY+wY, 0xFF0000);

	//Show buffer info
	if (!updated) {
		snprintf(displayText, 100, "BUFFERING: %d%%", progress);
		drawText(mw.display, mw.dimX, mw.dimY, fontData, displayText, 1, 0, 0, 0xFFFFFF);
	}

	createBorder(&mw);
	//Now put the mw.display onto the final one, which has borders and such
	for (x = 0; x < mw.dimX; x++) {
		for (y = 0; y < mw.dimY; y++) mw.pubDisplay[(x+mw.borderLeft)+
				(y+mw.borderTop)*(mw.dimX+mw.borderLeft+mw.borderRight)] = 
				(mw.focus)?mw.display[x+y*mw.dimX]:(mw.display[x+y*mw.dimX] & 0xFEFEFE) >> 1;
	}

	if (!mw.focus) {
		//Process save as window updates
		createBorder(&sa);

		for (x = 0; x < sa.dimX; x++) {
			for (y = 0; y < sa.dimY; y++) {
				sa.display[x+y*sa.dimX] = 0xFFFFFF;
			}
		}
	
		/*
		 * The maximum # of lines SA can show is sa.dimY/8
		 * However, we need to show the dialogue box which is for now
		 * 3+8+3 pixels tall and we need to have a gap between the lines.
		 * 2 pixels?
		 */
		int currentY = 0;
		rewinddir(cd);
		while ((dirList = readdir(cd)) != NULL) {
			if (currentY + 14 < sa.dimY) {
				drawText(sa.display, sa.dimX, sa.dimY, fontData, dirList->d_name, 1, 0, currentY, 0x000000);
				currentY+=10;
			}
		}
				
	
		for (x = 0; x < sa.dimX; x++) {
			for (y = 0; y < sa.dimY; y++) sa.pubDisplay[(x+sa.borderLeft)+
					(y+sa.borderTop)*(sa.dimX+sa.borderLeft+sa.borderRight)] = sa.display[x+y*sa.dimX];
		}

		SDL_UpdateTexture(sa.texture, NULL, sa.pubDisplay, (sa.dimX + sa.borderLeft + sa.borderRight)*4);
		SDL_RenderClear(sa.render);
		SDL_RenderCopy(sa.render, sa.texture, 0, 0);
		SDL_RenderPresent(sa.render);
	}

	SDL_UpdateTexture(mw.texture, NULL, mw.pubDisplay, (mw.dimX+mw.borderLeft+mw.borderRight)*4);
	SDL_RenderClear(mw.render);
	SDL_RenderCopy(mw.render, mw.texture, 0, 0);
	SDL_RenderPresent(mw.render);
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
	double NMinR = MinR + ((double)startX/mw.dimX)*(MaxR - MinR);
	double NMaxR = NMinR + ((double)realEX - startX)/mw.dimX*(MaxR - MinR);
	double NMinI = MinI + ((double)(mw.dimY-realEY)/mw.dimY)*(MaxI - MinI);
	double NMaxI = NMinI + (NMaxR - NMinR)*((double)mw.dimY/mw.dimX);
	MinR = NMinR; MaxR = NMaxR;
	MinI = NMinI; MaxI = NMaxI;
}

void recalc() {
	int x, y;
	for (x = 0; x < mw.dimX*RENDER_DISTANCE; x++) {
		for (y = 0; y < mw.dimY*RENDER_DISTANCE; y++) {
			if (x >= mw.dimX*(RENDER_DISTANCE-1.0f)/2.0f &&
			    x <  mw.dimX*(RENDER_DISTANCE+1.0f)/2.0f &&
			    y >= mw.dimY*(RENDER_DISTANCE-1.0f)/2.0f &&
			    y <  mw.dimY*(RENDER_DISTANCE+1.0f)/2.0f) {
				mw.buffer[x+y*mw.dimX*RENDER_DISTANCE] = getColor(x-mw.dimX*(RENDER_DISTANCE-1)/2,
										y-mw.dimY*(RENDER_DISTANCE-1)/2);
			} else {
				mw.buffer[x+y*mw.dimX*RENDER_DISTANCE] = 0x333333;
			}
		}
	}
	
	offX = mw.dimX*(RENDER_DISTANCE-1)/2;
	offY = mw.dimY*(RENDER_DISTANCE-1)/2;
	
	generateMiniMap();	
}

void shift(int fast) {
	double ShiftR = (double)(mouseX - startX)/mw.dimX*(MaxR - MinR);
	double ShiftI = (double)(mouseY - startY)/mw.dimY*(MaxI - MinI);
	
	if (fast) {
		ShiftR = ShiftR * -1.0 * (RENDER_DISTANCE*mw.dimX/miniDimX);
		ShiftI = ShiftI * -1.0 * (RENDER_DISTANCE*mw.dimY/miniDimY);
	}

	MinR -= ShiftR; MaxR -= ShiftR;
	MinI += ShiftI; MaxI += ShiftI;

	offX -= (fast)?RENDER_DISTANCE*-1*(mouseX - startX)*(mw.dimX/miniDimX):(mouseX - startX);
	offY -= (fast)?RENDER_DISTANCE*-1*(mouseY - startY)*(mw.dimY/miniDimY):(mouseY - startY);
	
	startX = mouseX;
	startY = mouseY;

	update();
}

void generateMiniMap() {
	//Obviously works best where 0 = mw.dimX mod miniDimX
	miniDimY = miniDimX*((float)mw.dimY/mw.dimX);
	int sfX = (int)(mw.dimX*RENDER_DISTANCE)/miniDimX;
	int sfY = (int)(mw.dimY*RENDER_DISTANCE)/miniDimY;

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
					avgColor += mw.buffer[(x*sfX+sX) + (y*sfY+sY)*mw.dimX*RENDER_DISTANCE]/aC;
				}
			}
			miniMap[x+y*miniDimX] = avgColor;
		}
	}
}

void createBorder(WOO_Window* win) {
	int x, y;
	for (x = 0; x < win->dimX + win->borderLeft+win->borderRight; x++) {
		for (y = 0; y < win->dimY + win->borderTop + win->borderBot; y++) {
			win->pubDisplay[x + y*(win->dimX+win->borderLeft+win->borderRight)] = 0x444444;
		}
	}
	drawText(win->pubDisplay, win->dimX+win->borderLeft+win->borderRight, win->dimY+win->borderTop+win->borderBot,
			fontData, "X", 1, (win->dimX+win->borderLeft+win->borderRight-8-3), 3, 0xFFFFFF);
	drawText(win->pubDisplay, win->dimX+win->borderLeft+win->borderRight, win->dimY+win->borderTop+win->borderBot,
			fontData, win->title, 1, 3, 3, 0xFF0000);
	drawText(win->pubDisplay, win->dimX+win->borderLeft+win->borderRight, win->dimY+win->borderTop+win->borderBot,
			fontData, win->sub, 1, win->len*8 + 25, 3, 0xFFFFFF);
}

void createSaveAs() {
	int tmp_var[3] = {300, 15, 3};
	SDL_Window* window = SDL_CreateWindow("Save As", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
			tmp_var[0]+2*tmp_var[2], tmp_var[0]+tmp_var[1]+tmp_var[2], SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE);

	SDL_Renderer* render = SDL_CreateRenderer(window, -1, 0);
	SDL_Texture* texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
			tmp_var[0]+2*tmp_var[2], tmp_var[0]+tmp_var[1] + tmp_var[2]);

	//This window is initialized w/o a buffer
	sa = (WOO_Window) {
		.dimX = tmp_var[0],
		.dimY = tmp_var[0],
		.borderTop = tmp_var[1],
		.borderLeft = tmp_var[2],
		.borderRight = tmp_var[2],
		.borderBot = tmp_var[2],
		.window = window,
		.render = render,
		.texture = texture,
		.pubDisplay = malloc(sizeof(uint32_t)*(tmp_var[0]+tmp_var[2]+tmp_var[2])*(tmp_var[0]+tmp_var[1]+tmp_var[2])),
		.display = malloc(sizeof(uint32_t)*(tmp_var[0] * tmp_var[0])),
		.bufferX = -1,
	       	.bufferY = -1,
		.title = malloc(100),
		.len = 8,
		.sub = malloc(100),
		.focus = 1,
		.id = currentID
	};

	currentID++;

	char tmp[10];
	memcpy(tmp, ".", 2);
	cd = opendir(tmp);

	memcpy(sa.title, "Save As", 8);
	memcpy(sa.sub, "", 1);
	
	saveImg();

	CD = &sa;

	mw.focus = 0;
}

void saveImg() {
	Imlib_Image img = imlib_create_image_using_data(RENDER_DISTANCE*mw.dimX, RENDER_DISTANCE*mw.dimY, mw.buffer);
	imlib_context_set_image(img);
	imlib_image_set_format("png");
	imlib_save_image("out.png");
	imlib_free_image();
}	
