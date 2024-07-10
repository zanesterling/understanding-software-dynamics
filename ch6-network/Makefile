CXX=g++
CXXFLAGS=-O2 -pthread -Wall -Werror -std=c++17

client: client.cc rpc.o network.o my_rpc.o print_hex.o log.o
	$(CXX) $(CXXFLAGS) client.cc network.o rpc.o my_rpc.o print_hex.o log.o -o client

server: server.cc rpc.o network.o spinlock.o my_rpc.o print_hex.o log.o
	$(CXX) $(CXXFLAGS) server.cc network.o rpc.o spinlock.o my_rpc.o print_hex.o log.o -o server

dumplogfile: dumplogfile.cc rpc.o print_hex.o log.o network.o
	$(CXX) $(CXXFLAGS) dumplogfile.cc rpc.o print_hex.o log.o network.o -o dumplogfile

clean:
	rm -f client server *.o

rpc.o: rpc.h rpc.cc print_hex.h log.h network.h
	$(CXX) $(CXXFLAGS) -c rpc.cc

network.o: network.h network.cc
	$(CXX) $(CXXFLAGS) -c network.cc

spinlock.o: spinlock.h spinlock.cc
	$(CXX) $(CXXFLAGS) -c spinlock.cc

my_rpc.o: my_rpc.h my_rpc.cc
	$(CXX) $(CXXFLAGS) -c my_rpc.cc

print_hex.o: print_hex.h print_hex.cc
	$(CXX) $(CXXFLAGS) -c print_hex.cc

log.o: log.h log.cc rpc.h
	$(CXX) $(CXXFLAGS) -c log.cc

