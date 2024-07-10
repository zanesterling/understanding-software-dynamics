client: client.cc rpc.o network.o my_rpc.o print_hex.o log.o
	g++ -O2 -pthread -Wall -Werror client.cc network.o rpc.o my_rpc.o print_hex.o log.o -o client

server: server.cc rpc.o network.o spinlock.o my_rpc.o print_hex.o log.o
	g++ -O2 -pthread -Wall -Werror server.cc network.o rpc.o spinlock.o my_rpc.o print_hex.o log.o -o server

dumplogfile: dumplogfile.cc rpc.o print_hex.o log.o network.o
	g++ -O2 -Wall -Werror dumplogfile.cc rpc.o print_hex.o log.o network.o -o dumplogfile

clean:
	rm -f client server *.o

rpc.o: rpc.h rpc.cc print_hex.h log.h network.h
	g++ -O2 -Wall -Werror -c rpc.cc

network.o: network.h network.cc
	g++ -O2 -Wall -Werror -c network.cc

spinlock.o: spinlock.h spinlock.cc
	g++ -O2 -Wall -Werror -c spinlock.cc

my_rpc.o: my_rpc.h my_rpc.cc
	g++ -O2 -Wall -Werror -c my_rpc.cc

print_hex.o: print_hex.h print_hex.cc
	g++ -O2 -Wall -Werror -c print_hex.cc

log.o: log.h log.cc rpc.h
	g++ -O2 -Wall -Werror -c log.cc

