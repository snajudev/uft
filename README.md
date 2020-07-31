### U&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;F&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;T
### UDP File Transfer

#
#### What is UFT?
UFT is a UDP based file transfer protocol built on top of UDT.
It was designed for transferring quickly and effeciently, even if a connection is broken, while being light enough to run on a single board computer.

#
#### How does UFT work?
UFT works with the concept of two devices, one client and one server.
File transfers are initiated by sending the file size, timestamp and destination path.
If the file exists on the receiving end then the size is compared to the sender's.
If the receiver's file size is greater than the sender's then the file is deleted and transferred one chunk at a time.
If the receiver's file size is less than or equal to the sender's then the file is verified and sent one chunk at a time only as needed.
File chunks are compared using a 64 bit FNV 1a hash.
All transfers are synchronous, meaning if one end is sending then the other must be receiving.

#
#### Warning
The internal protocol has no concept of authentication and assumes all received data is valid.
This shouldn't be used on public networks or those with the possibility of a malicious sender.

#
#### What does UFT depend on?
* [UDT](https://udt.sourceforge.io/)
* [ZLIB](https://zlib.net/)
* JDK (only if using Java binding)

#
#### How do I use UFT?
##### Build
```bash
make sample_client
make sample_server
```
##### Send file
```bash
./sample_client 192.168.1.10 /path/to/local/source /path/on/remote/destination
```
##### Receive file(s)
```bash
./sample_server
```
##### Receive file(s) and send file on accept
```bash
./sample_server /path/to/local/source /path/on/remote/destination
```
##### Receive file(s) and send files on accept
```bash
./sample_server /path/to/local/source1 /path/on/remote/destination1 /path/to/local/source2 /path/on/remote/destination2
```

#
#### How do I use UFT from C++?
The included sample_client.cpp and sample_server.cpp files demonstrate API usage.

#
#### How do I use UFT from Java?
The included SampleUFTClient.java and SampleUFTServer.java files demonstrate API usage.

##### Build
```bash
make libJUFT.so
```

##### Note about stack size
The stack size should be at least 4MB.
This can be increased at start time through a JVM parameter ``java -Xss4m ...``
