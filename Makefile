CC=gcc
CFLAGS=-Wall -O2 -c -pthread $(DEBUG)

LFLAGS=`sdl2-config --cflags --libs`

fractal: draw.o 
	$(CC) $(DEBUG) draw.o fractal.c -o fractal $(LFLAGS)

debug: 
	$(MAKE) DEBUG="-g -O0"

remake:
	$(MAKE) clean
	$(MAKE)

clean:
	rm -f *.o fractal

static: draw.o
	gcc -o fractal fractal.c -L/usr/lib -L/usr/local/lib -Wl,-rpath,/usr/local/lib,-Bstatic -lSDL2 -lXxf86vm -lXi -lpng -lz -lstdc++ -Wl,-Bdynamic -lpthread -lGL -lX11 -lXext -ldl -lm draw.o
	
