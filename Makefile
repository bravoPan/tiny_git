CFLAGS=-g -Wall -Wextra

all: WTFserver.o WTFclient.o utility.o md5.o fdstruct.o
	$(CC) WTFserver.o utility.o md5.o fdstruct.o -o WTFserver -pthread
	$(CC) WTFclient.o utility.o fdstruct.o md5.o -o WTFclient -pthread
	cp WTFclient ./client/WTFclient
	cp WTFserver ./server/WTFserver

%.o: %.c
	$(CC) $< -c -o $@ $(CFLAGS)

clean:
	rm -f *.o WTFserver WTFclient

clean_ckot:
	rm -rf client/test_checkout
