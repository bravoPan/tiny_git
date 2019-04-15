CFLAGS=-O2

all: WTFserver.o WTFclient.o utility.o
	$(CC) WTFserver.o utility.o -o WTFserver -pthread
	$(CC) WTFclient.o utility.o -o WTFclient -pthread

unittest: utility.o unittest.o
	$(CC) unittest.o utility.o -o unittest -lncurses

%.o: %.c
	$(CC) $< -c -o $@ $(CFLAGS)

clean:
	rm -f *.o WTFserver WTFclient
