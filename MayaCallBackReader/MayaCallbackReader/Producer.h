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
	size_t messageCount;
	size_t messageID;

public:
	Producer();
	Producer(int  numMessages, 
			 int  chunkSize, 
			 LPCWSTR varBuffName);
	~Producer();
	void makeMessage(char* msg, size_t msgLen);
    /*
    Can you have this always run? And have the producer read from a constant "stream" of messages?
    A stream that you feed things into?
    A stream could be a queue of void* msg that periodically get fed into the producer

    I think we need to do it this way, since otherwise we would've perhaps have gotten more than one producer...
    The messages put into the queue need to be global, otherwise they'll run out of scope when added to the queue.
    Or rather, the pointers need to be global... Right? 
    Or maybe the adress remains in the queue, and the data gotten from 'new' remain too, since no delete was called.
    */
    void runProducer(circularBuffer* buffInst, char* msg, size_t packetSize);
};