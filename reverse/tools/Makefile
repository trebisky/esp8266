# Makefile for ESP8266 development
# Tom Trebisky  12-26-2015

all:	dumper wrap

install:
	cp dumper /home/tom/bin

# binary to elf "wrapper" follows
# from 2014 Mightyframe project.

wrap:	wrap.c
	cc -o wrap wrap.c

# dump selected parts of binary image.
dumper:	dumper.c
	cc -o dumper dumper.c

clean:
	rm -f wrap
	rm -f dumper
