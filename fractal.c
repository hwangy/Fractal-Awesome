#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#include "SDL_gfxPrimitives_font.h"
#include "draw.h"

const int rmax = 30, iterations = 100;
static SDL_Window* window;
SDL_Renderer* render;
SDL_Texture* texture;
SDL_Surface* surface;
uint32_t* display, *rawDisplay, *bufferedDisplay;
char* displayText;
const unsigned char* fontData = gfxPrimitivesFontdata;

//Threading for image buffering
sem_t generateBuff;
pthread_t threadID;
pthread_mutex_t mutex;
void thread(void* simon);
int updated = 0;
int tileRenderDistance = 5; 				//Odd numbers preferred

struct timespec start = {.tv_nsec = 0, .tv_sec = 0};
struct timespec end ={.tv_nsec = 0, .tv_sec = 0};
struct timespec p = {.tv_nsec = 0, .tv_sec = 0};

int dimX, dimY;

int resizeMode = 0, mouseDown = 0;
int offX, offY;
int mouseX, mouseY, startX, startY;
int startX, startY, realEX, realEY;
double MinR, MaxR, MinI, MaxI;

int getCount(double x, double y);
void update();
void resize();
void recalc();
void shift();

int main(int argc, char** argv) {
	dimX = dimY = 600; //Cool C stuff

	displayText = malloc(sizeof(char)*100);

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	window = SDL_CreateWindow("Fractal", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, dimX, dimY, 0);
	render = SDL_CreateRenderer(window, -1, 0);
	texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, dimX, dimY);
	
	display = malloc(sizeof(uint32_t)*dimX*dimY);
	rawDisplay = malloc(sizeof(uint32_t)*dimX*dimY);
	bufferedDisplay = malloc(sizeof(uint32_t)*dimX*dimY*tileRenderDistance*tileRenderDistance);
	
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
			else if (events.type == SDL_KEYUP && events.key.keysym.sym == SDLK_LSHIFT) resizeMode = 0;
			if (events.type == SDL_MOUSEBUTTONDOWN) {
				mouseDown = 1;
				SDL_GetMouseState(&startX, &startY);
			} else if (events.type == SDL_MOUSEBUTTONUP) {
				mouseDown = 0;
				SDL_GetMouseState(&mouseX, &mouseY);
				if (resizeMode && updated) {
					updated = 0;
					resize();
					recalc();
				
					//Routine to ensure buffered image has loaded
					sem_post(&generateBuff);
				}

				resizeMode = 0;
			}
		}
		SDL_GetMouseState(&mouseX, &mouseY);

		update();

		if (mouseDown && !resizeMode) {
			SDL_GetMouseState(&mouseX, &mouseY);
			shift();
		}

		clock_gettime(CLOCK_MONOTONIC, &end);
		long int sleep = 25000000 - (end.tv_nsec - start.tv_nsec);
		if (sleep > 0) {
			p.tv_nsec = sleep;
			nanosleep(&p, NULL);
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
			puts("STARTED");
			buffMinR = MinR - (MaxR - MinR)*mult;
			buffMaxR = MaxR + (MaxR - MinR)*mult;
			buffMinI = MinI - (MaxI - MinI)*mult;
			buffMaxI = MaxI + (MaxI - MinI)*mult;

			ref = (buffMaxR-buffMinR)/(dimX*tileRenderDistance); imf = (buffMaxI - buffMinI)/(dimY*tileRenderDistance);
	
			int x, y;
			for (x = 0; x < dimX*tileRenderDistance; x++) {
				for (y = 0; y < dimY*tileRenderDistance; y++) {
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
			puts("FINISHED");
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
	//memcpy(display, rawDisplay, sizeof(uint32_t)*dimX*dimY);

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

	if (!updated) {
		snprintf(displayText, 100, "%s", "BUFFERING>>>");
		drawText(display, dimX, dimY, fontData, displayText, 1, 0, 0, 0xFFFFFF);
	}	

	SDL_UpdateTexture(texture, NULL, display, dimX*4);
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
}

void shift() {
	double ShiftR = (double)(mouseX - startX)/dimX*(MaxR - MinR);
	double ShiftI = (double)(mouseY - startY)/dimY*(MaxI - MinI);

	MinR -= ShiftR; MaxR -= ShiftR;
	MinI += ShiftI; MaxI += ShiftI;

	offX -= (mouseX - startX);
	offY -= (mouseY - startY);
	
	startX = mouseX;
	startY = mouseY;


	//recalc();
	update();
}
