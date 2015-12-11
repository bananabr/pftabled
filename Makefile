prefix=/usr/local
exec_prefix=${prefix}
bindir=${exec_prefix}/bin
sbindir=${exec_prefix}/sbin
mandir=${datarootdir}/man
datarootdir = ${prefix}/share

CC=cc
CFLAGS=-g -O2 -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CPPFLAGS=
LDFLAGS= -lrt
INSTALL=/usr/bin/install -c
LIBS=
NROFF=mandoc -Tascii -mandoc

SERVEROBJS=pftabled.o hmac.o sha1.o
CLIENTOBJS=pftabled-client.o hmac.o sha1.o

all: client server

server: pftabled pftabled.cat1

client: pftabled-client

pftabled: ${SERVEROBJS}
	${CC} ${LDFLAGS} -o $@ ${SERVEROBJS} ${LIBS}

pftabled.cat1: pftabled.1
	${NROFF} pftabled.1 > pftabled.cat1

pftabled-client: ${CLIENTOBJS}
	${CC} ${LDFLAGS} -o $@ ${CLIENTOBJS} ${LIBS}

install: client-install server-install

server-install: pftabled pftabled.cat1
	${INSTALL} -s -m 555 pftabled ${sbindir}
	${INSTALL} -m 444 pftabled.cat1 ${mandir}/cat1/pftabled.0

client-install: pftabled-client
	${INSTALL} -s -m 555 pftabled-client ${bindir}

clean:
	-rm -f pftabled pftabled-client *.o *.cat1

distclean: clean
	-rm -f Makefile config.log config.status config.cache config.h

cvsclean: clean
	-rm -f Makefile config.log config.status config.cache config.h
	-rm -f configure README config.h.in
	-rm -rf autom4te.cache

DISTDIR=pftabled-1.09
dist:
	-rm -f $(DISTDIR).tar $(DISTDIR).tar.gz
	-tar -I MANIFEST -s '/^/$(DISTDIR)\//' -cvf $(DISTDIR).tar
	-gzip -9 $(DISTDIR).tar

