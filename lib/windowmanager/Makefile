CFLAGS=$(shell pkg-config --cflags x11 2> /dev/null || echo -I/usr/X11R6/include -I/usr/local/include)
LDFLAGS=$(shell pkg-config --libs x11 2> /dev/null || echo -L/usr/X11R6/lib -L/usr/local/lib -lX11)

#CFLAGS+=-g -I/usr/local/include/db45
#LDFLAGS+=-L/usr/local/lib/db45 -ldb
#CFLAGS+=-fPIC

LDFLAGS+=-lxdo
CFLAGS+=-I/usr/local/include

all: main

clean:
	rm *.o || true

windowmanager.o: windowmanager.h

main: windowmanager.o main.o
	$(CC) $(CFLAGS) -o $@ windowmanager.o main.o $(LDFLAGS)

