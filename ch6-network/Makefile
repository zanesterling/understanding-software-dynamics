client: client.cc rpc.o network.o
	g++ -O2 -pthread -Wall -Werror client.cc network.o rpc.o -o client

server: server.cc rpc.o network.o
	g++ -O2 -pthread -Wall -Werror server.cc network.o rpc.o -o server

rpc.o: rpc.h rpc.cc
	g++ -O2 -c rpc.cc

network.o: network.h network.cc
	g++ -O2 -c network.cc
