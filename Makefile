
#CXXFLAGS += -std=c++11
CXXFLAGS = -g -O2 -std=gnu++11

server:server.o
	g++ -o server server.cc

clean_sock:
	rm -rf server server.o client client.o

epoll: epoll_server epoll_client

epoll_server:epoll_server.o epoll_common.o epoll_message.o
	g++ -o epoll_server epoll_server.cc epoll_common.cc epoll_message.cc \
	-lboost_serialization
epoll_client:epoll_client.o epoll_common.o epoll_message.o
	g++ -o epoll_client epoll_client.cc epoll_common.cc epoll_message.cc -lboost_serialization
epoll_server.o:epoll_server.cc
	g++ ${CXXFLAGS} -c -g epoll_server.cc -o epoll_server.o
epoll_client.o:epoll_client.cc epoll_common.cc epoll_message.cc
	g++ ${CXXFLAGS} -c -g epoll_client.cc -o epoll_client.o
epoll_message.o:epoll_message.cc
	g++ ${CXXFLAGS} -c -g epoll_message.cc -o epoll_message.o
epoll_common.o:epoll_common.cc
	g++ ${CXXFLAGS} -c -g epoll_common.cc -o epoll_message.o


clean_epoll:
	rm -rf epoll_server epoll_client *.o

paxos_main:paxos_main.o
	g++ -std=c++11 paxos_main.cc paxos_messenger.cc paxos_message.cc paxos_common.cc -o paxos_main \
	-lstdc++ -lpthread \
	-lboost_iostreams -lboost_system -lboost_serialization

paxos_client:paxos_client.o
	g++ -std=c++11 paxos_client.cc paxos_messenger.cc paxos_message.cc -o paxos_client \
	-lstdc++ -lpthread \
	-lboost_iostreams -lboost_system -lboost_serialization

test:test.o epoll_message.o
	gcc -o test test.cc epoll_message.cc -lboost_iostreams -lboost_system -lboost_serialization -lstdc++

clean:
	rm -rf test paxos_main paxos_client *.o

