CXX                ?= g++

JDK_ROOT_DIRECTORY ?= /usr/lib/jvm/adoptopenjdk-8-hotspot-armhf
#JDK_ROOT_DIRECTORY ?= /usr/lib/jvm/adoptopenjdk-8-hotspot-amd64
#JDK_ROOT_DIRECTORY ?= /usr/lib/jvm/java-8-openjdk-amd64

UDT_ROOT_DIRECTORY ?= ../udt/src

all: socket
#all: socket server test

test:
	$(CXX) -O3 -Wall -std=c++14 $(CPPFLAGS) -I$(UDT_ROOT_DIRECTORY) UFTSocket.cpp UFTTest.cpp $(LDFLAGS) $(LDLIBS) $(UDT_ROOT_DIRECTORY)/libudt.a -lpthread -lz -o UFTTest

socket:
	$(CXX) -O3 -Wall -fPIC -std=c++14 -shared $(CPPFLAGS) -I$(UDT_ROOT_DIRECTORY) -I$(JDK_ROOT_DIRECTORY)/include/ -I$(JDK_ROOT_DIRECTORY)/include/linux/ UFTSocket.cpp Java_com_snaju_io_UFTSocket.cpp $(LDFLAGS) $(LDLIBS) $(UDT_ROOT_DIRECTORY)/libudt.a -lpthread -lz -o libJUFTSocket.so

server:
	$(CXX) -O3 -Wall -std=c++14 $(CPPFLAGS) -I$(UDT_ROOT_DIRECTORY) UFTSocket.cpp UFTServer.cpp $(LDFLAGS) $(LDLIBS) -lpthread -lz $(UDT_ROOT_DIRECTORY)/libudt.a -o UFTServer

clean:
#	rm -f UFTServer
	rm -f libJUFTSocket.so
#	rm -f UFTTest
