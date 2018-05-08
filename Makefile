# Targets for solaris and linux. feel free to add others
# Thanks to Michael Rumpf for helping to simplify this

CFLAGS := $(shell pkg-config --cflags glib-2.0 gtk+-2.0) -std=gnu99
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
	rm -f cuda_perfbar.o linux_perfbar.o cuda_perfbar linux_perfbar

# ------------------------------------------------------------
# Docker wrapper for compiling the above code.
#
# make centos7_cuda8_in_docker
# ------------------------------------------------------------

ARCH := $(shell arch)
PLATFORM := $(ARCH)-centos7

ARCH_SUBST = arch-undefined
ifeq ($(ARCH),x86_64)
    FROM_SUBST = nvidia\/cuda:8.0-cudnn5-devel-centos7
endif
ifeq ($(ARCH),ppc64le)
    FROM_SUBST = nvidia\/cuda-ppc64le:8.0-cudnn5-devel-centos7
endif
CONTAINER_REPO_TAG = tomk/gtk_perfbar-$(ARCH)

Dockerfile-centos7.$(PLATFORM): Dockerfile-centos7.in
	cat $< \
	| sed 's/FROM_SUBST/$(FROM_SUBST)/g' \
	| sed 's/ARCH_SUBST/$(ARCH_SUBST)/g' \
	> $@

centos7_cuda8_in_docker: Dockerfile-centos7.$(PLATFORM)
		docker build \
		-t $(CONTAINER_REPO_TAG) \
		-f Dockerfile-centos7.$(PLATFORM) \
		.
	docker run \
		--rm \
		--init \
		-u `id -u`:`id -g` \
		-v `pwd`:/dot \
		-w /dot \
		--entrypoint /bin/bash \
		$(CONTAINER_REPO_TAG) \
		-c 'make'
