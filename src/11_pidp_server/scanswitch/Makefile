CFLAGS ?= -O2
LIBGPIOD_LIBS := $(shell pkg-config --libs libgpiod)

scansw: scansw.c ../gpiohdr/gpiohdr.c ../gpiohdr/gpiohdr.h
	$(CC) $(CFLAGS) -o scansw scansw.c ../gpiohdr/gpiohdr.c $(LIBGPIOD_LIBS)
