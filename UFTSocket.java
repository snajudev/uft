package com.snaju.io;

public class UFTSocket
{
	// non-mutable, maintained by native code
	private long socket = 0;
	// non-mutable, maintained by native code
	private int timeout = 20 * (60 * 1000);
	// non-mutable, maintained by native code
	private boolean isBlockingMode = true;

	public boolean isBlocking()
	{
		return isBlockingMode;
	}

	public native boolean isListening();

	public native boolean isConnected();

	public int getTimeout()
	{
		return timeout;
	}

	public native boolean listen(long localHost, int localPort);

	// @return null if would block
	public native UFTSocket accept();

	public native boolean connect(long remoteHost, int remotePort);

	public native void close();

	// can be called at any time
	public native boolean setBlocking(boolean block);

	// -1 = infinite
	// can be called at any time
	public native boolean setTimeout(int milliseconds);

	// @return file size
	// @return 0 on connection lost
	// @return -2 on internal error
	// if connection is lost then disconnect() is automatically called
	public native long sendFile(String source, String destination);

	// @return file size
	// @return -1 if would block
	// @return 0 on connection lost
	// if connection is lost then disconnect() is automatically called
	public native long receiveFile(StringBuffer path);

	private void onSendProgress(long bytesSent, long fileSize)
	{
	}

	private void onReceiveProgress(long bytesReceived, long fileSize)
	{
	}
}
