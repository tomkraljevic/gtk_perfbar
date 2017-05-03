# Targets for solaris and linux. feel free to add others
# Thanks to Michael Rumpf for helping to simplify this

CFLAGS := $(shell pkg-config --cflags glib-2.0 gtk+-2.0)
LDFLAGS := $(shell pkg-config --libs glib-2.0 gtk+-2.0)

all:   linux_perfbar

perfbar.o: perfbar.c Makefile
	gcc -c -O -DLINUX $(CFLAGS) $(LDFLAGS) -o perfbar.o perfbar.c

linux_perfbar: perfbar.o Makefile
	gcc perfbar.o -O -DLINUX $(CFLAGS) $(LDFLAGS) -o linux_perfbar

solaris_sparc_perfbar: perfbar.c Makefile
	gcc -O -DSOLARIS $(CFLAGS) $(LDFLAGS) -lkstat -o solaris_sparc_perfbar perfbar.c 
solaris_x86_perfbar: perfbar.c Makefile
	gcc -O -DSOLARIS $(CFLAGS) $(LDFLAGS) -lkstat -o solaris_x86_perfbar perfbar.c 

clean:
	rm -f perfbar.o linux_perfbar
