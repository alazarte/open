all: clean build

clean:
	if [ -f open ]; then rm open; fi

build:
	cc -Wall -Wextra -o open open.c

install:
	cp open ${HOME}/System/bin
