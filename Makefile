CFLAGS=`pkg-config --cflags x11 2> /dev/null || echo -I/usr/X11R6/include -I/usr/local/include`
LDFLAGS=`pkg-config --libs x11 2> /dev/null || echo -L/usr/X11R6/lib -L/usr/local/lib -lX11 -lXtst`

#CFLAGS+=-Wuninitialized -O
CFLAGS=-Wall
LDFLAGS+=-ldb

all: test
clean:
	rm *.o test || true
	make -C wmlib clean

CFLAGS+=-g

%.o: %.c
	gcc $(CFLAGS) -c -o $@  $<

wmlib/wmlib.o: wmlib/wmlib.c
	make -C wmlib wmlib.o

test: test.o wmlib/wmlib.o
	gcc $(LDFLAGS) -o $@ test.o wmlib/wmlib.o
