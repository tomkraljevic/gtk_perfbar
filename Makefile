# Targets for solaris and linux. feel free to add others
# Thanks to Michael Rumpf for helping to simplify this

CFLAGS := $(shell pkg-config --cflags glib-2.0 gtk+-2.0) -I/usr/local/cuda-8.0/include
LDFLAGS := $(shell pkg-config --libs glib-2.0 gtk+-2.0) -L/usr/lib/nvidia-370 -lnvidia-ml

all:   cuda_perfbar

perfbar.o: perfbar.c Makefile
	gcc -c -O -DCUDA $(CFLAGS) $(LDFLAGS) -o perfbar.o perfbar.c

cuda_perfbar: perfbar.o Makefile
	gcc perfbar.o -O -DCUDA $(CFLAGS) $(LDFLAGS) -o cuda_perfbar

solaris_sparc_perfbar: perfbar.c Makefile
	gcc -O -DSOLARIS $(CFLAGS) $(LDFLAGS) -lkstat -o solaris_sparc_perfbar perfbar.c 
solaris_x86_perfbar: perfbar.c Makefile
	gcc -O -DSOLARIS $(CFLAGS) $(LDFLAGS) -lkstat -o solaris_x86_perfbar perfbar.c 

clean:
	rm -f perfbar.o cuda_perfbar linux_perfbar
