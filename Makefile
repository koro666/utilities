ifdef STATIC
CC=musl-gcc
LDFLAGS=-static
endif

CFLAGS+=-Wall -O3 -ffunction-sections -fdata-sections -fvisibility=hidden
LDFLAGS+=-Wl,--gc-sections -Wl,--build-id=none

.PHONY: build rebuild clean

build: README.html bt2mt hexx iphm setlogcons sleepuntil sqlstat takeover tsvstat uidmapshift vipcheck

rebuild: clean build

clean:
	rm -rf *.html *.o *.zip bt2mt hexx iphm setlogcons sleepuntil sqlite3 sqlstat takeover tsvstat uidmapshift vipcheck

%.html: %.md
	markdown $< > $@

sqlite3.zip:
	curl -f -s -L -o sqlite3.zip https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
	echo '0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9 sqlite3.zip' | sha256sum --quiet -c

sqlite3:
	mkdir -p $@

sqlite3/sqlite3.c: sqlite3.zip | sqlite3
	unzip -q -j sqlite3.zip sqlite-amalgamation-3540100/sqlite3.c -d sqlite3

sqlite3/sqlite3.h: sqlite3.zip | sqlite3
	unzip -q -j sqlite3.zip sqlite-amalgamation-3540100/sqlite3.h -d sqlite3

bt2mt: CPPFLAGS+=-D_GNU_SOURCE
iphm: LDLIBS+=-lm
sleepuntil: CPPFLAGS+=-D_XOPEN_SOURCE
ifdef STATIC
sqlstat: sqlstat.o sqlite3/sqlite3.o
sqlstat.o: CFLAGS+=-Isqlite3
sqlstat.o: sqlite3/sqlite3.h
else
sqlstat: LDLIBS+=-lsqlite3
endif
takeover: CPPFLAGS+=-D_GNU_SOURCE
tsvstat: CPPFLAGS+=-D_XOPEN_SOURCE=500
uidmapshift: CPPFLAGS+=-D_XOPEN_SOURCE=500
