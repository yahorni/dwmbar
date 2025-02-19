# dwmbar - simple bar for dwm
# See LICENSE file for copyright and license details.

include config.mk

SRC = dwmbar.c
OBJ = ${SRC:.c=.o}

all: options dwmbar

options:
	@echo dwmbar build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@

dwmbar: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f dwmbar ${OBJ} dwmbar-${VERSION}.tar.gz

dist: clean
	mkdir -p dwmbar-${VERSION}
	cp -R LICENSE Makefile README.md config.def.h config.mk\
		blocks/ dwmbar.1 ${SRC} dwmbar-${VERSION}
	tar -cf dwmbar-${VERSION}.tar dwmbar-${VERSION}
	gzip dwmbar-${VERSION}.tar
	rm -rf dwmbar-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwmbar ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwmbar
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < dwmbar.1 > ${DESTDIR}${MANPREFIX}/man1/dwmbar.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwmbar.1
	mkdir -p ${BLOCKSPREFIX}
	cp -u blocks/* ${BLOCKSPREFIX}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwmbar
	rm -f ${DESTDIR}${MANPREFIX}/man1/dwmbar.1
	rm -rf ${DESTDIR}${BLOCKSPREFIX}

.PHONY: all options clean dist install uninstall
