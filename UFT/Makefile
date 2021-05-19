RM                          ?= rm -rf
CXX                         ?= g++

UDT_ROOT_DIRECTORY          ?= ../udt/src

CXXFLAGS                    += -Wall -std=c++17 -O3
CXXFLAGS                    += -I$(UDT_ROOT_DIRECTORY)

LDLIBS                      += $(UDT_ROOT_DIRECTORY)/libudt.a -lpthread -lz

SOURCE_FILES                = UFTSocket.cpp
SOURCE_FILES_CLIENT         = $(SOURCE_FILES) uft_client.cpp
SOURCE_FILES_SERVER         = $(SOURCE_FILES) uft_server.cpp

OBJECT_FILES_CLIENT         = $(SOURCE_FILES_CLIENT:.cpp=.o)
OBJECT_FILES_SERVER         = $(SOURCE_FILES_SERVER:.cpp=.o)

all: uft_client uft_server

uft_client: $(OBJECT_FILES_CLIENT)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

uft_server: $(OBJECT_FILES_SERVER)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(OBJECT_FILES_CLIENT)
	$(RM) $(OBJECT_FILES_SERVER)
