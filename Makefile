CFLAGS=-O2

all: WTFserver.o WTFclient.o
	$(CC) WTFserver.o -o WTFserver -pthread
	$(CC) WTFclient.o -o WTFclient -pthread

%.o: %.c
	$(CC) $< -c -o $@ $(CFLAGS)

clean:
	rm -f *.o WTFserver WTFclient
