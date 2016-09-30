#pragma once
#include <Windows.h>
#include <stdlib.h>
#include <cstdio>
#include <iostream>
#include "CircularBuffer.h"
#include "FileMapStructs.h"
class Consumer
{
private:
	int delay;
	int requestedMessages;
	int maxMsgSize;
	int messageCount;

public:
	Consumer();
	Consumer(int & delay, 
			 int & numMessages, 
			 size_t & maxMsgSize, 
			 size_t & buffSize, 
			 int & chunkSize, 
			 LPCWSTR varBuffName);
	~Consumer();
	void runConsumer(circularBuffer& buffInst);
};