CXX                ?= g++

#JDK_ROOT_DIRECTORY ?= /usr/lib/jvm/adoptopenjdk-8-hotspot-armhf
JDK_ROOT_DIRECTORY ?= /usr/lib/jvm/adoptopenjdk-8-hotspot-amd64
#JDK_ROOT_DIRECTORY ?= /usr/lib/jvm/java-8-openjdk-amd64

UDT_ROOT_DIRECTORY ?= ../udt/src

all: sender receiver
#all: sender receiver tester

tester:
	$(CXX) -Wall -std=c++14 $(CPPFLAGS) -I$(UDT_ROOT_DIRECTORY) UFTSocket.cpp UFTSenderTest.cpp $(LDFLAGS) $(LDLIBS) $(UDT_ROOT_DIRECTORY)/libudt.a -lpthread -o UFTSenderTest

sender:
	$(CXX) -Wall -fPIC -std=c++14 -shared $(CPPFLAGS) -I$(UDT_ROOT_DIRECTORY) -I$(JDK_ROOT_DIRECTORY)/include/ -I$(JDK_ROOT_DIRECTORY)/include/linux/ UFTSocket.cpp Java_com_snaju_io_UFTSender.cpp $(LDFLAGS) $(LDLIBS) $(UDT_ROOT_DIRECTORY)/libudt.a -lpthread -o libJUFTSender.so

receiver:
	$(CXX) -Wall -std=c++14 $(CPPFLAGS) -I$(UDT_ROOT_DIRECTORY) UFTSocket.cpp UFTReceiver.cpp $(LDFLAGS) $(LDLIBS) -lpthread $(UDT_ROOT_DIRECTORY)/libudt.a -o UFTReceiver

clean:
	rm -f UFTReceiver
	rm -f libJUFTSender.so
#	rm -f UFTSenderTest
