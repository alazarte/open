in=main.c
out=open
bin_folder=$(HOME)/System/bin
cflags=-Wall -Wextra

all: clean build install

clean:
	rm -f $(out)

build:
	cc $(cflags) -o $(out) $(in)

install:
	cp $(out) $(bin_folder)
