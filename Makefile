.PHONY: build rebuild clean

build: README.html

rebuild: clean build

clean:
	rm -f *.html

%.html: %.md
	markdown $< > $@
