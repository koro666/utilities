ifdef STATIC
CC=musl-gcc
LDFLAGS=-static
endif

CFLAGS+=-Wall -O3 -D_XOPEN_SOURCE

.PHONY: build rebuild clean

build: README.html sleepuntil

rebuild: clean build

clean:
	rm -f *.html sleepuntil

%.html: %.md
	markdown $< > $@
