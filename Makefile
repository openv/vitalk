CC=arm-linux-gnueabihf-gcc-5
PORT=3083


vitalk: vitalk.c vito_io.c vito_io.h vito_parameter.c vito_parameter.h telnet.c telnet.h
	$(CC) -D PORT=$(PORT) -Wall -o vitalk \
	    vitalk.c vito_io.c vito_parameter.c telnet.c

clean:
	rm vitalk

copy:
	scp *.c *.h Makefile *.sh root@heizung:Heizung/ViTalk/

