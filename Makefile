CFLAGS=-g

all: WTFserver.o WTFclient.o utility.o md5.o fdstruct.o
	$(CC) WTFserver.o utility.o md5.o fdstruct.o -o WTFserver -pthread -lncurses
	$(CC) WTFclient.o utility.o fdstruct.o md5.o -o WTFclient -pthread -lncurses
	cp WTFclient ./client/WTFclient
	cp WTFserver ./server/WTFserver

unittest: utility.o unittest.o md5.o fdstruct.o
	$(CC) unittest.o utility.o md5.o fdstruct.o -o unittest -lncurses

%.o: %.c
	$(CC) $< -c -o $@ $(CFLAGS)

clean:
	rm -f *.o WTFserver WTFclient

clean ckot:
	rm -rf client/test_checkout
