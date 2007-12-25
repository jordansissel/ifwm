CFLAGS=`pkg-config --cflags x11 2> /dev/null || echo -I/usr/X11R6/include -I/usr/local/include`
LDFLAGS=`pkg-config --libs x11 2> /dev/null || echo -L/usr/X11R6/lib -L/usr/local/lib -lX11`

CFLAGS+=-g -I/usr/local/include/db45
LDFLAGS+=-L/usr/local/lib/db45 -ldb
CFLAGS+=-fPIC
all: wmlib.so

clean:
	rm *.o || true

wmlib.o: wmlib.h

%.o: %.c
	gcc $(CFLAGS) -c -o $@  $<

