// -----------------------------------------------------------------------------
// Program: Materials ISS Experiment (MISSE)
// File: Java_com_snaju_io_UFTSender.cpp
// Purpose: JNI connector for UFTSocket file transfers
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#include <jni.h>

#include "UFTSocket.hpp"

UFTSocket socket;

extern "C" JNIEXPORT jboolean Java_com_snaju_io_UFTSender_isConnected(JNIEnv* lpJNI, jobject object)
{
	return socket.IsConnected();
}

extern "C" JNIEXPORT jint Java_com_snaju_io_UFTSender_getTimeout(JNIEnv* lpJNI, jobject object)
{
	return socket.GetTimeout();
}

extern "C" JNIEXPORT jboolean Java_com_snaju_io_UFTSender_connect(JNIEnv* lpJNI, jobject object, jlong remoteHost, jint remotePort)
{
	return socket.Connect(
		static_cast<unsigned int>(remoteHost),
		static_cast<unsigned short>(remotePort)
	);
}

extern "C" JNIEXPORT void Java_com_snaju_io_UFTSender_disconnect(JNIEnv* lpJNI, jobject object)
{
	socket.Disconnect();
}

extern "C" JNIEXPORT jboolean Java_com_snaju_io_UFTSender_setTimeout(JNIEnv* lpJNI, jobject object, jint milliseconds)
{
	return socket.SetTimeout(
		milliseconds
	);
}

extern "C" JNIEXPORT jboolean Java_com_snaju_io_UFTSender_sendFile(JNIEnv* lpJNI, jobject object, jstring source, jstring destination)
{
	auto lpSource = lpJNI->GetStringUTFChars(
		source,
		nullptr
	);

	auto lpDestination = lpJNI->GetStringUTFChars(
		destination,
		nullptr
	);

	auto result = socket.SendFile(
		lpSource,
		lpDestination
	);

	lpJNI->ReleaseStringUTFChars(
		destination,
		lpDestination
	);

	lpJNI->ReleaseStringUTFChars(
		source,
		lpSource
	);

	return result;
}

extern "C" JNIEXPORT jboolean Java_com_snaju_io_UFTSender_sendFileChunk(JNIEnv* lpJNI, jobject object, jstring source, jstring destination, jlong index, jlong size)
{
	auto lpSource = lpJNI->GetStringUTFChars(
		source,
		nullptr
	);

	auto lpDestination = lpJNI->GetStringUTFChars(
		destination,
		nullptr
	);

	auto result = socket.SendFileChunk(
		lpSource,
		lpDestination,
		index,
		size
	);

	lpJNI->ReleaseStringUTFChars(
		destination,
		lpDestination
	);

	lpJNI->ReleaseStringUTFChars(
		source,
		lpSource
	);

	return result;
}
