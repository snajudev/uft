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
#### What does UFT depend on?
* [UDT](https://udt.sourceforge.io/)
* [ZLIB](https://zlib.net/)
* JDK (only if using Java binding)

#
#### How do I use UFT?
##### Send File<br />
<pre>
TODO
</pre>
##### Receive File<br />
<pre>
TODO
</pre>