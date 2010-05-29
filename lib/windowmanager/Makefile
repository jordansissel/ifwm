CFLAGS=$(shell pkg-config --cflags glib-2.0 x11 2> /dev/null || echo -I/usr/X11R6/include -I/usr/local/include)
LDFLAGS=$(shell pkg-config --libs glib-2.0 x11 2> /dev/null || echo -L/usr/X11R6/lib -L/usr/local/lib -lX11 -lglib-2.0)

LDFLAGS+=-lxdo
CFLAGS+=-I/usr/local/include

CFLAGS+=-g

all: main

clean:
	rm *.o || true

windowmanager.o: windowmanager.h

main: windowmanager.o main.o
	$(CC) $(CFLAGS) -o $@ windowmanager.o main.o $(LDFLAGS)

