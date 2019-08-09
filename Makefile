ifdef STATIC
CC=musl-gcc
LDFLAGS=-static
endif

CFLAGS+=-Wall -O3 -D_XOPEN_SOURCE=700

.PHONY: build rebuild clean

build: README.html setlogcons sleepuntil vipcheck

rebuild: clean build

clean:
	rm -f *.html setlogcons sleepuntil vipcheck

%.html: %.md
	markdown $< > $@
