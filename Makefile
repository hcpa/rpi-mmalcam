# Makefile for mmaltest
# [24/01/2014]

SHELL = /bin/bash

prefix      = 
exec_prefix = 
bindir      = /usr/local/bin
mandir      = /usr/local/man

CC      = gcc
CFLAGS  = -g -Wall -DPROGRAM_VERSION=\"1.0\" -DPROGRAM_NAME=\"mmaltest\" -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads/ -I/opt/vc/include/interface/vmcs_host/linux/
LDFLAGS = -L/opt/vc/lib -lmmal -lbcm_host

OBJS  = mmaltest.o log.o

all: mmaltest

install: all
	mkdir -p ${DESTDIR}${bindir}
	install -m 755 test ${DESTDIR}${bindir}

mmaltest: $(OBJS)
	$(CC) -o mmaltest $(OBJS) $(LDFLAGS)

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -f core* *.o test

distclean: clean
	rm -rf *~ *.jpg

