client: client.cc rpc.o network.o my_rpc.o
	g++ -O2 -pthread -Wall -Werror client.cc network.o rpc.o my_rpc.o -o client

server: server.cc rpc.o network.o spinlock.o my_rpc.o
	g++ -O2 -pthread -Wall -Werror server.cc network.o rpc.o spinlock.o my_rpc.o -o server

rpc.o: rpc.h rpc.cc
	g++ -O2 -c rpc.cc

network.o: network.h network.cc
	g++ -O2 -c network.cc

spinlock.o: spinlock.h spinlock.cc
	g++ -O2 -c spinlock.cc

my_rpc.o: my_rpc.h my_rpc.cc
	g++ -O2 -c my_rpc.cc

