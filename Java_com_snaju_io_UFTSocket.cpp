// -----------------------------------------------------------------------------
// Program: Materials ISS Experiment (MISSE)
// File: Java_com_snaju_io_UFTSocket.cpp
// Purpose: JNI connector for UFTSocket file transfers
// Written by: F. Barney
// Date: 6/14/2020
// -----------------------------------------------------------------------------

#include <jni.h>

#include "UFTSocket.hpp"

inline UFTSocket* GetSocket(JNIEnv* lpJNI, jobject object)
{
	auto jClass = lpJNI->GetObjectClass(
		object
	);

	auto jFieldId = lpJNI->GetFieldID(
		jClass,
		"socket",
		"J"
	);

	auto jSocket = lpJNI->GetLongField(
		object,
		jFieldId
	);

	return reinterpret_cast<UFTSocket*>(
		jSocket
	);
}

inline void SetSocket(JNIEnv* lpJNI, jobject object, UFTSocket* lpSocket)
{
	auto jClass = lpJNI->GetObjectClass(
		object
	);

	auto jFieldId = lpJNI->GetFieldID(
		jClass,
		"socket",
		"J"
	);

	lpJNI->SetLongField(
		object,
		jFieldId,
		reinterpret_cast<jlong>(
			lpSocket
		)
	);
}

inline jint IsSocketBlocking(JNIEnv* lpJNI, jobject object)
{
	auto jClass = lpJNI->GetObjectClass(
		object
	);

	auto jFieldId = lpJNI->GetFieldID(
		jClass,
		"isBlockingMode",
		"Z"
	);

	auto jIsBlockingMode = lpJNI->GetBooleanField(
		object,
		jFieldId
	);

	return jIsBlockingMode;
}

inline jint GetTimeout(JNIEnv* lpJNI, jobject object)
{
	auto jClass = lpJNI->GetObjectClass(
		object
	);

	auto jFieldId = lpJNI->GetFieldID(
		jClass,
		"timeout",
		"I"
	);

	auto jTimeout = lpJNI->GetIntField(
		object,
		jFieldId
	);

	return jTimeout;
}

extern "C" JNIEXPORT jboolean Java_com_snaju_io_UFTSocket_isListening(JNIEnv* lpJNI, jobject object)
{
	if (auto lpSocket = GetSocket(lpJNI, object))
	{

		return lpSocket->IsListening();
	}
	
	return false;
}

extern "C" JNIEXPORT jboolean Java_com_snaju_io_UFTSocket_isConnected(JNIEnv* lpJNI, jobject object)
{
	if (auto lpSocket = GetSocket(lpJNI, object))
	{

		return lpSocket->IsConnected();
	}

	return false;
}

extern "C" JNIEXPORT jboolean Java_com_snaju_io_UFTSocket_listen(JNIEnv* lpJNI, jobject object, jlong localHost, jint localPort)
{
	UFTSocket* lpSocket;

	if ((lpSocket = GetSocket(lpJNI, object)) == nullptr)
	{
		lpSocket = new UFTSocket();

		if (lpSocket->Open())
		{
			if (lpSocket->Listen(localHost, localPort, 1) &&
				lpSocket->SetTimeout(GetTimeout(lpJNI, object)) &&
				lpSocket->SetBlocking(IsSocketBlocking(lpJNI, object)))
			{
				SetSocket(
					lpJNI,
					object,
					lpSocket
				);

				return true;
			}

			lpSocket->Close();
		}

		delete lpSocket;
	}

	return false;
}

extern "C" JNIEXPORT jobject Java_com_snaju_io_UFTSocket_accept(JNIEnv* lpJNI, jobject object)
{
	if (auto lpSocket = GetSocket(lpJNI, object))
	{
		auto lpClient = new UFTSocket();

		if (lpSocket->Accept(*lpClient))
		{
			auto jClass = lpJNI->GetObjectClass(
				object
			);

			auto jMethodId = lpJNI->GetMethodID(
				jClass,
				"<init>",
				"()V"
			);

			auto jObject = lpJNI->NewObject(
				jClass,
				jMethodId
			);

			SetSocket(
				lpJNI,
				jObject,
				lpClient
			);

			return jObject;
		}

		delete lpClient;
	}

	return nullptr;
}

extern "C" JNIEXPORT jboolean Java_com_snaju_io_UFTSocket_connect(JNIEnv* lpJNI, jobject object, jlong remoteHost, jint remotePort)
{
	UFTSocket* lpSocket;

	if ((lpSocket = GetSocket(lpJNI, object)) == nullptr)
	{
		lpSocket = new UFTSocket();

		if (lpSocket->Open())
		{
			if (lpSocket->Connect(static_cast<unsigned int>(remoteHost), static_cast<unsigned short>(remotePort)))
			{
				if (lpSocket->SetTimeout(GetTimeout(lpJNI, object)) && lpSocket->SetBlocking(false) &&
					lpSocket->SetBlocking(IsSocketBlocking(lpJNI, object)))
				{
					SetSocket(
						lpJNI,
						object,
						lpSocket
					);

					return true;
				}

				lpSocket->Disconnect();
			}

			lpSocket->Close();
		}

		delete lpSocket;
	}

	return false;
}

extern "C" JNIEXPORT void Java_com_snaju_io_UFTSocket_close(JNIEnv* lpJNI, jobject object)
{
	auto lpSocket = GetSocket(
		lpJNI,
		object
	);

	if (lpSocket->IsConnected())
	{

		lpSocket->Disconnect();
	}

	lpSocket->Close();

	delete lpSocket;

	SetSocket(
		lpJNI,
		object,
		nullptr
	);
}

extern "C" JNIEXPORT jboolean Java_com_snaju_io_UFTSocket_setBlocking(JNIEnv* lpJNI, jobject object, jboolean block)
{
	if (auto lpSocket = GetSocket(lpJNI, object))
	{
		if (!lpSocket->SetBlocking(block))
		{

			return false;
		}
	}

	auto jClass = lpJNI->GetObjectClass(
		object
	);

	auto jFieldId = lpJNI->GetFieldID(
		jClass,
		"isBlockingMode",
		"Z"
	);

	lpJNI->SetBooleanField(
		object,
		jFieldId,
		block
	);

	return true;
}

extern "C" JNIEXPORT jboolean Java_com_snaju_io_UFTSocket_setTimeout(JNIEnv* lpJNI, jobject object, jint milliseconds)
{
	if (auto lpSocket = GetSocket(lpJNI, object))
	{
		if (!lpSocket->SetTimeout(milliseconds))
		{

			return false;
		}
	}

	auto jClass = lpJNI->GetObjectClass(
		object
	);

	auto jFieldId = lpJNI->GetFieldID(
		jClass,
		"timeout",
		"I"
	);

	lpJNI->SetIntField(
		object,
		jFieldId,
		milliseconds
	);

	return true;
}

extern "C" JNIEXPORT jlong Java_com_snaju_io_UFTSocket_sendFile(JNIEnv* lpJNI, jobject object, jstring source, jstring destination)
{
	if (auto lpSocket = GetSocket(lpJNI, object))
	{
		auto lpSource = lpJNI->GetStringUTFChars(
			source,
			nullptr
		);

		auto lpDestination = lpJNI->GetStringUTFChars(
			destination,
			nullptr
		);

		struct OnSendParam
		{
			JNIEnv* lpJNI;
			jclass jClass;
			jobject jObject;
			jmethodID jMethodId;
		} param;

		param.lpJNI = lpJNI;
		param.jClass = lpJNI->GetObjectClass(
			object
		);
		param.jObject = object;
		param.jMethodId = lpJNI->GetMethodID(
			param.jClass,
			"onSendProgress",
			"(JJ)V"
		);

		UFTSocket_OnSendProgress onSend(
			[](UFTSocket_FileSize _bytesSent, UFTSocket_FileSize _fileSize, void* _lpParam)
			{
				auto lpParam = reinterpret_cast<OnSendParam*>(
					_lpParam
				);

				lpParam->lpJNI->CallVoidMethod(
					lpParam->jObject,
					lpParam->jMethodId,
					static_cast<jlong>(_bytesSent),
					static_cast<jlong>(_fileSize)
				);
			}
		);

		auto result = lpSocket->SendFile(
			lpSource,
			lpDestination,
			onSend,
			&param
		);

		lpJNI->ReleaseStringUTFChars(
			destination,
			lpDestination
		);

		lpJNI->ReleaseStringUTFChars(
			source,
			lpSource
		);

		if (result == 0)
		{

			Java_com_snaju_io_UFTSocket_close(
				lpJNI,
				object
			);
		}

		return result;
	}

	return 0;
}

extern "C" JNIEXPORT jlong Java_com_snaju_io_UFTSocket_receiveFile(JNIEnv* lpJNI, jobject object, jobject path)
{
	if (auto lpSocket = GetSocket(lpJNI, object))
	{
		char _path[255];
		long long result;

		struct OnRecvParam
		{
			JNIEnv* lpJNI;
			jclass jClass;
			jobject jObject;
			jmethodID jMethodId;
		} param;

		param.lpJNI = lpJNI;
		param.jClass = lpJNI->GetObjectClass(
			object
		);
		param.jObject = object;
		param.jMethodId = lpJNI->GetMethodID(
			param.jClass,
			"onReceiveProgress",
			"(JJ)V"
		);

		UFTSocket_OnReceiveProgress onReceive(
			[](UFTSocket_FileSize _bytesSent, UFTSocket_FileSize _fileSize, void* _lpParam)
			{
				auto lpParam = reinterpret_cast<OnRecvParam*>(
					_lpParam
				);

				lpParam->lpJNI->CallVoidMethod(
					lpParam->jObject,
					lpParam->jMethodId,
					static_cast<jlong>(_bytesSent),
					static_cast<jlong>(_fileSize)
				);
			}
		);

		if ((result = lpSocket->ReceiveFile(_path, onReceive, &param)) == 0)
		{
			Java_com_snaju_io_UFTSocket_close(
				lpJNI,
				object
			);
		}
		else if (result > 0)
		{
			auto jClass = lpJNI->GetObjectClass(
				path
			);

			auto jMethodId = lpJNI->GetMethodID(
				jClass,
				"append",
				"(Ljava/lang/String;)Ljava/lang/StringBuffer;"
			);

			auto jString = lpJNI->NewStringUTF(
				_path
			);

			lpJNI->CallVoidMethod(
				path,
				jMethodId,
				jString
			);

			lpJNI->ReleaseStringUTFChars(
				jString,
				_path
			);
		}

		return result;
	}

	return 0;
}
