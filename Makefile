# Makefile for mmaltest
# [24/01/2014]

SHELL = /bin/bash

VPATH = .:./gpu_fft

SUBDIRS = gpu_fft

CC      = gcc

OBJS  = log.o dbg_image.o fft.o fft_gpu.o
GOBJS = gpu_fft.c gpu_fft_shaders.c gpu_fft_twiddles.c hello_fft.c mailbox.c


ifdef OPTIM
  export CFLAGS  = -O3 -Wall -DPROGRAM_VERSION=\"1.0\" -DPROGRAM_NAME=\"mmaltest\" -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads/ -I/opt/vc/include/interface/vmcs_host/linux/
  export LDFLAGS = -L/opt/vc/lib -L./gpu_fft -lmmal -lmmal_core -lmmal_util -lbcm_host -lvcos -lgd -lfftw3f -lgpu_fft
else
  export CFLAGS  = -g -Wall -DPROGRAM_VERSION=\"1.0\" -DPROGRAM_NAME=\"mmalyuv\" -I/home/pi/src/userland -I/home/pi/src/userland/host_applications/linux/libs/bcm_host/include/ -I/opt/vc/include/interface/vcos/pthreads/ -I/opt/vc/include/interface/vmcs_host/linux/
  export LDFLAGS = -L/home/pi/src/userland/build/lib -L./gpu_fft -lmmal -lmmal_core -lmmal_util -lbcm_host -lvcos -lgd -lfftw3f -lgpu_fft
endif


all: mmalyuv $(SUBDIRS)

mmaltest: mmaltest.o $(OBJS)
	$(CC) -o mmaltest mmaltest.o $(OBJS) $(LDFLAGS)

mmalyuv: mmalyuv.o $(OBJS) libgpu_fft.a
	$(CC) -o mmalyuv mmalyuv.o $(OBJS) $(LDFLAGS)
	sudo chown root mmalyuv
	sudo chmod u+s mmalyuv
	
libgpu_fft.a: gpu_fft

$(SUBDIRS)::
	$(MAKE) -C $@ $(MAKECMDGOALS)


.PHONY : clean 

clean: $(SUBDIRS)
	-rm -f core* $(OBJS) mmalyuv mmaltest

