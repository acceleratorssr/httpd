all: httpd client
LIBS = -lpthread -std=gnu99 #-lsocket
httpd: httpd.c
	gcc -g -W -Wall $(LIBS) -o $@ $<

client: simpleclient.c
	gcc -W -Wall -o $@ $<
clean:
	rm httpd
