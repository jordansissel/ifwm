CFLAGS=$(shell pkg-config --cflags x11 2> /dev/null || echo -I/usr/X11R6/include -I/usr/local/include)
LDFLAGS=$(shell pkg-config --libs x11 2> /dev/null || echo -L/usr/X11R6/lib -L/usr/local/lib -lX11)

#CFLAGS+=-g -I/usr/local/include/db45
#LDFLAGS+=-L/usr/local/lib/db45 -ldb
#CFLAGS+=-fPIC
all: main

clean:
	rm *.o || true

wmlib.o: wmlib.h

main: wmlib.o main.o
	$(CC) $(CFLAGS) -o $@ wmlib.o main.o $(LDFLAGS)

wmlib.so: wmlib.o

#.o: %.c
	#$(CC) $(CFLAGS) -c -o $@  $<

