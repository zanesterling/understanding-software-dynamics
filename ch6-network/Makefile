client: client.cc rpc.o
	g++ -O2 -pthread -Wall -Werror client.cc rpc.o -o client

server: server.cc rpc.o
	g++ -O2 -pthread -Wall -Werror server.cc rpc.o -o server

rpc.o: rpc.h rpc.cc
	g++ -O2 -c rpc.cc
