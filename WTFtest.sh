#!/bin/sh
make
./server/WTF 6767 &
./client/WTF configure 127.0.0.1 6767
./client/WTF create new_pro
echo "this is a.txt" > ./client/new_pro/a.txt
./client/WTF add new_pro a.txt
./client/WTF update new_pro
./client/WTF upgrade new_pro
./client/WTF commit new_pro
./client/WTF push new_pro
./client/WTF history new_pro
./client/WTF rollback new_pro 0
./client/WTF currentversion new_pro
echo "this is b.txt" > ./client/new_pro/b.txt
mkdir ./client/new_pro/sub
echo "this is c.txt" > ./client/new_pro/sub/c.txt
./client/WTF add new_pro b.txt
./client/WTF add new_pro sub/c.txt
./client/WTF remove new_pro sub/c.txt
rm -rf ./client/new_pro
./client/WTF checkout new_pro
./client/WTF destroy new_pro
