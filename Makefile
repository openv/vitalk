CC?=gcc
PORT=3083

version.c: .git/HEAD .git/index
	echo "const char *version = \"$(shell git describe --long --tags --dirty --always --match 'v[0-9]\.[0-9]')\";" > $@

vitalk: vitalk.c vito_io.c vito_io.h vito_parameter.c vito_parameter.h telnet.c telnet.h version.h version.c
	$(CC) -D PORT=$(PORT) -Wall -o vitalk \
	    vitalk.c vito_io.c vito_parameter.c telnet.c version.c

all: vitalk

clean:
	rm -f vitalk version.c

