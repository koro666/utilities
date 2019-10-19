ifdef STATIC
CC=musl-gcc
LDFLAGS=-static
endif

CFLAGS+=-Wall -O3

.PHONY: build rebuild clean

build: README.html iphm setlogcons sleepuntil takeover tsvstat uidmapshift vipcheck

rebuild: clean build

clean:
	rm -f *.html iphm setlogcons sleepuntil takeover tsvstat uidmapshift vipcheck

%.html: %.md
	markdown $< > $@

iphm: LDLIBS+=-lm
sleepuntil: CFLAGS+=-D_XOPEN_SOURCE
takeover: CFLAGS+=-D_GNU_SOURCE
tsvstat: CFLAGS+=-D_XOPEN_SOURCE=500
uidmapshift: CFLAGS+=-D_XOPEN_SOURCE=500
