### U&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;F&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;T
### UDP File Transfer

#
#### What is UFT?
UFT is a UDP based file transfer protocol built on top of UDT.
It was designed for transferring files quickly and effeciently while being light enough to run on a single board computer.

#
#### How does UFT work?
UFT works with the concept of two devices, one client and one server.
File transfers are initiated by sending the file size, timestamp and destination path.
If the file exists on the receiving end then the size is compared to the sender's.
If the receiver's file size is greater than the sender's then the file is deleted and transferred one chunk at a time.
If the receiver's file size is less than or equal to the sender's then the file is read in chunks and a hash is compared against the sender. Chunks are transmitted when invalid or missing.

#
#### How do I use UFT?
##### Build
```bash
make uft_client
make uft_server
```
##### Run server
```bash
./uft_server --local-host=127.0.0.1 --local-port=9000 --timeout={seconds}
```
##### Get file list
```bash
./uft_client --remote-host=127.0.0.1 --remote-port=9000 --command=get_file_list --path="{path}" --timeout={seconds}
```
##### Send file
```bash
./uft_client --remote-host=127.0.0.1 --remote-port=9000 --command=send_file --source="{source}" --destination="{destination}" --timeout={seconds}
```
##### Receive file
```bash
./uft_client --remote-host=127.0.0.1 --remote-port=9000 --command=receive_file --source="{source}" --destination="{destination}" --timeout={seconds}
```

#
#### What does UFT depend on?
* [UDT](https://udt.sourceforge.io/)
* [ZLIB](https://zlib.net/)

#
#### How do I build dependencies?
The UDT library is included with UFT but must be built independently due to support for multiple OS and CPU architectures.
The OS and CPU architecture must be specified through arguments. These are some combinations.

```bash
make -e os=LINUX arch=IA32
make -e os=LINUX arch=IA64
make -e os=LINUX arch=ARM
make -e os=LINUX arch=ARM64
make -e os=LINUX arch=AMD64
make -e os=LINUX arch=POWERPC
make -e os=LINUX arch=SPARC

make -e os=WIN32 arch=IA32
make -e os=WIN32 arch=IA64
make -e os=WIN32 arch=ARM
make -e os=WIN32 arch=ARM64
make -e os=WIN32 arch=AMD64
make -e os=WIN32 arch=POWERPC
make -e os=WIN32 arch=SPARC
```
