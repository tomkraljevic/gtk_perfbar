# Targets for solaris and linux. feel free to add others
# Thanks to Michael Rumpf for helping to simplify this

CFLAGS := $(shell pkg-config --cflags glib-2.0 gtk+-2.0)
LDFLAGS := $(shell pkg-config --libs glib-2.0 gtk+-2.0)

all:   cuda_perfbar linux_perfbar

cuda_perfbar.o: perfbar.c Makefile
	gcc -c -O -DCUDA -I/usr/local/cuda-8.0/include $(CFLAGS) $(LDFLAGS) -o cuda_perfbar.o perfbar.c

cuda_perfbar: cuda_perfbar.o Makefile
	gcc cuda_perfbar.o -O -DCUDA $(CFLAGS) $(LDFLAGS) -L/usr/lib/nvidia-370 -lnvidia-ml -o cuda_perfbar

linux_perfbar.o: perfbar.c Makefile
	gcc -c -O -DLINUX $(CFLAGS) $(LDFLAGS) -o linux_perfbar.o perfbar.c

linux_perfbar: linux_perfbar.o Makefile
	gcc linux_perfbar.o -O -DLINUX $(CFLAGS) $(LDFLAGS) -o linux_perfbar

solaris_sparc_perfbar: perfbar.c Makefile
	gcc -O -DSOLARIS $(CFLAGS) $(LDFLAGS) -lkstat -o solaris_sparc_perfbar perfbar.c 
solaris_x86_perfbar: perfbar.c Makefile
	gcc -O -DSOLARIS $(CFLAGS) $(LDFLAGS) -lkstat -o solaris_x86_perfbar perfbar.c 

clean:
	rm -f perfbar.o cuda_perfbar linux_perfbar
