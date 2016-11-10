CC?=gcc
PORT=3083


vitalk: vitalk.c vito_io.c vito_io.h vito_parameter.c vito_parameter.h telnet.c telnet.h
	$(CC) -D PORT=$(PORT) -Wall -o vitalk \
	    vitalk.c vito_io.c vito_parameter.c telnet.c

all: vitalk

clean:
	rm -f vitalk

