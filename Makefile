CFLAGS=`pkg-config --cflags x11 2> /dev/null || echo -I/usr/X11R6/include -I/usr/local/include`
LDFLAGS=`pkg-config --libs x11 2> /dev/null || echo -L/usr/X11R6/lib -L/usr/local/lib -lX11 -lXtst`

all: test

CFLAGS+=-g

%.o: %.c
	gcc $(CFLAGS) -c -o $@  $<

test: test.o wmlib.o
	gcc $(LDFLAGS) -o $@ test.o wmlib.o
