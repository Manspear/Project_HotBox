#pragma once
#include <Windows.h>
#include <stdlib.h>
#include <cstdio>
#include <iostream>
#include "CircularBuffer.h"
#include "FileMapStructs.h"
#include <time.h>
class Producer
{
private:
	enum {
		PRODUCER = 0,
		CONSUMER = 1,
		RANDOM = 0,
		MSGSIZE = 1
	};
	size_t delay;
	size_t requestedMessages;
	size_t msgSizeMode;
	size_t maxMsgSize;
	size_t messageCount;
	size_t messageID;

public:
	Producer();
	Producer(int & delay, 
			 int & numMessages, 
			 size_t & maxMsgSize,
			 int& msgSizeMode,
			 size_t & buffSize, 
			 int & chunkSize, 
			 LPCWSTR varBuffName);
	~Producer();
	void makeMessage(char* msg, size_t msgLen);
	void runProducer(circularBuffer& buffInst);
};