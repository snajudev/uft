#ifndef WIN32
   #include <arpa/inet.h>
   #include <netdb.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
#endif
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <udt.h>
#include "cc.h"

using namespace std;

int main(int argc, char* argv[])
{
   if (argc != 9) {
      cout << "usage: recvfile server_ip server_port remote_filename local_filename use_blast(0 or 1) udt_sendbuff udp_sendbuff mss" << endl;
      return 1;
   }

   // use this function to initialize the UDT library
   UDT::startup();

   struct addrinfo hints, *peer;

   memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_STREAM;

   UDTSOCKET fhandle = UDT::socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol);

    int blast = atoi(argv[5]);
    int udt_recvbuff = atoi(argv[6]);
    int udp_recvbuff = atoi(argv[7]);
    int mss = atoi(argv[8]);
    //buffer_size = udt_recvbuff;


   // UDT Options
    if (blast)
        UDT::setsockopt(fhandle, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));

   UDT::setsockopt(fhandle, 0, UDT_MSS, &mss, sizeof(int));
   UDT::setsockopt(fhandle, 0, UDT_RCVBUF, &udt_recvbuff, sizeof(int));
   UDT::setsockopt(fhandle, 0, UDP_RCVBUF, &udp_recvbuff, sizeof(int));
   UDT::setsockopt(fhandle, 0, UDT_SNDBUF, &udt_recvbuff, sizeof(int));
   UDT::setsockopt(fhandle, 0, UDP_SNDBUF, &udp_recvbuff, sizeof(int));


   if (0 != getaddrinfo(argv[1], argv[2], &hints, &peer))
   {
      cout << "incorrect server/peer address. " << argv[1] << ":" << argv[2] << endl;
      return -1;
   }

   // connect to the server, implict bind
   if (UDT::ERROR == UDT::connect(fhandle, peer->ai_addr, peer->ai_addrlen))
   {
      cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
      return -1;
   }

   freeaddrinfo(peer);


   // send name information of the requested file
   int len = strlen(argv[3]);

   if (UDT::ERROR == UDT::send(fhandle, (char*)&len, sizeof(int), 0))
   {
      cout << "send: " << UDT::getlasterror().getErrorMessage() << endl;
      return -1;
   }

   if (UDT::ERROR == UDT::send(fhandle, argv[3], len, 0))
   {
      cout << "send: " << UDT::getlasterror().getErrorMessage() << endl;
      return -1;
   }

   // get size information
   int64_t size;

   if (UDT::ERROR == UDT::recv(fhandle, (char*)&size, sizeof(int64_t), 0))
   {
      cout << "send: " << UDT::getlasterror().getErrorMessage() << endl;
      return -1;
   }

   if (size < 0)
   {
      cout << "no such file " << argv[3] << " on the server\n";
      return -1;
   }

   // receive the file
   fstream ofs(argv[4], ios::out | ios::binary | ios::trunc);
   int64_t recvsize;
   int64_t offset = 0;

   if (UDT::ERROR == (recvsize = UDT::recvfile(fhandle, ofs, offset, size)))
   {
      cout << "recvfile: " << UDT::getlasterror().getErrorMessage() << endl;
      return -1;
   }

   UDT::close(fhandle);

   ofs.close();

   // use this function to release the UDT library
   UDT::cleanup();

   return 0;
}
