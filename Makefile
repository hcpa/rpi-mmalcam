# Makefile for mmaltest
# [24/01/2014]

SHELL = /bin/bash

prefix      = 
exec_prefix = 
bindir      = /usr/local/bin
mandir      = /usr/local/man

CC      = gcc
#CFLAGS  = -g -Wall -DPROGRAM_VERSION=\"1.0\" -DPROGRAM_NAME=\"mmaltest\" -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads/ -I/opt/vc/include/interface/vmcs_host/linux/
CFLAGS  = -g -Wall -DPROGRAM_VERSION=\"1.0\" -DPROGRAM_NAME=\"mmalyuv\" -I../userland -I../userland/host_applications/linux/libs/bcm_host/include/ -I/opt/vc/include/interface/vcos/pthreads/ -I/opt/vc/include/interface/vmcs_host/linux/

#LDFLAGS = -L/opt/vc/lib -lmmal -lmmal_core -lmmal_util -lbcm_host -lvcos -lgd -lfftw3f
LDFLAGS = -L/home/pi/src/userland/build/lib -lmmal -lmmal_core -lmmal_util -lbcm_host -lvcos -lgd -lfftw3f

OBJS  = log.o dbg_image.o fft.o

all: mmaltest mmalyuv

install: all
	mkdir -p ${DESTDIR}${bindir}
	install -m 755 test ${DESTDIR}${bindir}

mmaltest: mmaltest.o $(OBJS)
	$(CC) -o mmaltest mmaltest.o $(OBJS) $(LDFLAGS)

mmalyuv: mmalyuv.o $(OBJS) mmalyuv.h
	$(CC) -o mmalyuv mmalyuv.o $(OBJS) $(LDFLAGS)

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -f core* *.o test

distclean: clean
	rm -rf *~ *.jpg

