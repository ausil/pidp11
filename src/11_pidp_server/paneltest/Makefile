CFLAGS ?= -O2 -Wall
LIBGPIOD_LIBS := $(shell pkg-config --libs libgpiod)

paneltest: paneltest.c ../gpiohdr/gpiohdr.c ../gpiohdr/gpiohdr.h
	$(CC) $(CFLAGS) -o paneltest paneltest.c ../gpiohdr/gpiohdr.c $(LIBGPIOD_LIBS)

clean:
	rm -f paneltest
