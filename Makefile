RM ?= rm -f
PREFIX ?= /usr/local/
BINDIR ?= ${PREFIX}/bin
MANDIR ?= ${PREFIX}/man


.PHONY: all
all: ttb

install: all
	install -d ${BINDIR} ${MANDIR}/man1
	install -m755 ttb ${BINDIR}
	install -m644 ttb.1 ${MANDIR}/man1

uninstall:
	${RM} ${BINDIR}/ttb
	${RM} ${MANDIR}/man1/ttb.1

.PHONY: clean
clean:
	${RM} ttb

README: ttb.1
	mandoc -Ios=ttb -Tascii ttb.1 | col -b > README
