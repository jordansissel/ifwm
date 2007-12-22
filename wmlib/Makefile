CFLAGS=`pkg-config --cflags x11 2> /dev/null || echo -I/usr/X11R6/include -I/usr/local/include`
LDFLAGS=`pkg-config --libs x11 2> /dev/null || echo -L/usr/X11R6/lib -L/usr/local/lib -lX11`

LDFLAGS+=-ldb
CFLAGS+=-fPIC
all: wmlib.so

clean:
	rm *.o || true

wmlib.c: wmlib.h

%.o: %.c
	gcc $(CFLAGS) -c -o $@  $<

%.so: %.o
	gcc -shared $(CFLAGS) $(LDFLAGS) $< -o $@

