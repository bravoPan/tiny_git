CFLAGS=-g

all: WTFserver.o WTFclient.o utility.o md5.o test.o client/process_upgrade.o client/process_commit.o client/process_push.o client/process_remove.o fdstruct.o
	$(CC) WTFserver.o utility.o md5.o fdstruct.o client/process_upgrade.o client/process_commit.o client/process_push.o client/process_remove.o -o WTFserver -pthread
	$(CC) WTFclient.o utility.o fdstruct.o md5.o client/process_upgrade.o client/process_commit.o client/process_push.o client/process_remove.o -o WTFclient -pthread
	$(CC) test.o utility.o md5.o fdstruct.o -o test
	# $(CC)
	cp WTFclient ./client/WTFclient
	cp WTFserver ./server/WTFserver

%.o: %.c
	$(CC) $< -c -o $@ $(CFLAGS)

clean:
	rm -f *.o WTFserver WTFclient .DS_Store
	rm -f ./client/*.o ./client/.DS_Store

clean_ckot:
	rm -rf client/ttt

# one_test:
# 	./WTFserver
