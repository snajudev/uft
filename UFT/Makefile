RM                         ?= rm -rf
CXX                        ?= g++

UDT_ROOT_DIRECTORY         ?= ../udt/src

#JDK_ROOT_DIRECTORY       ?= /usr/lib/jvm/java-8-openjdk-armhf
#JDK_ROOT_DIRECTORY       ?= /usr/lib/jvm/adoptopenjdk-8-hotspot-armhf
JDK_ROOT_DIRECTORY         ?= /usr/lib/jvm/adoptopenjdk-8-hotspot-amd64

CXXFLAGS                   += -Wall -std=c++14 -O3
CXXFLAGS                   += -I$(UDT_ROOT_DIRECTORY)
CXXFLAGS                   += -fPIC

CXXFLAGS_SO                = $(CXXFLAGS)
CXXFLAGS_SO                += -shared

CXXFLAGS_JNI               = $(CXXFLAGS)
CXXFLAGS_JNI               += -I$(JDK_ROOT_DIRECTORY)/include/ -I$(JDK_ROOT_DIRECTORY)/include/linux/

LDLIBS                     += $(UDT_ROOT_DIRECTORY)/libudt.a -lpthread -lz

SOURCE_FILES               = UFTSocket.cpp
SOURCE_FILES_JNI           = $(SOURCE_FILES) Java_com_snaju_io_UFTSocket.cpp
SOURCE_FILES_SAMPLE_CLIENT = $(SOURCE_FILES) sample_client.cpp
SOURCE_FILES_SAMPLE_SERVER = $(SOURCE_FILES) sample_server.cpp

all: libUFT.so libJUFT.so sample_client sample_server

libUFT.so: $(SOURCE_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CXXFLAGS_SO) $^ -o $@ $(LDFLAGS) $(LDLIBS)

libJUFT.so: $(SOURCE_FILES_JNI)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CXXFLAGS_SO) $(CXXFLAGS_JNI) $^ -o $@ $(LDFLAGS) $(LDLIBS)

sample_client: $(SOURCE_FILES_SAMPLE_CLIENT)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

sample_server: $(SOURCE_FILES_SAMPLE_SERVER)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

clean:

delete:
	$(RM) libUFT.so
	$(RM) libJUFT.so
	$(RM) sample_client
	$(RM) sample_server
