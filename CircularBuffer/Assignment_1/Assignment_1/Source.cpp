#include <cstdlib>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include "CircularBuffer.h"
#include "Consumer.h"
#include "Producer.h"
#include "Mutex.h"
#include <time.h> //use time as a kernel for the rand() function. 
#define CHUNKSIZE 256
//#define NUMCLIENTS 5
//Sometimes program gets stuck in an infinite loop somewhere. 
using namespace std;
enum {
	PRODUCER = 0,
	CONSUMER = 1,
	RANDOM = 0,
	MSGSIZE = 1
};

int main(int argc, char* argv[]) {
	int delay = atoi(argv[2]);
	size_t fileMapSize = atoi(argv[3]); 
	fileMapSize = fileMapSize << 20; //converts to bytes
	int numMessages = atoi(argv[4]);
	int role;
	int msgSizeMode;
	size_t four = 4;
	size_t maxMsgSize = fileMapSize / four;
	//check if this executable is a producer or a consumer
	if (strcmp("producer", argv[1]) == 0)
	{
		role = PRODUCER;
	}
	else if (strcmp("consumer", argv[1]) == 0)
	{
		role = CONSUMER;
	}
	else {
		LPCWSTR error = TEXT("argv[1] doesn't supply a valid string");
		OutputDebugString(error);
		return 0;
	}
	if (strcmp("random", argv[5]) == 0)
	{
		msgSizeMode = RANDOM;
	}
	else if (atoi(argv[5]) <= (fileMapSize / four))
	{
		msgSizeMode = MSGSIZE;
		maxMsgSize = atoi(argv[5]);
	}
	else {
		LPCWSTR error = TEXT("argv[5] doesn't supply a valid string");
		OutputDebugString(error);
		return 0;
	}
	circularBuffer cirB;
	LPCWSTR msgBuffName = TEXT("MessageBuffer");
	LPCWSTR varBuffName = TEXT("VarBuffer");
	int chunkSize = 256;
	cirB.initCircBuffer(msgBuffName, fileMapSize, role, chunkSize, varBuffName);
	
	if (role == PRODUCER)
	{
		Producer producer = Producer(delay, numMessages, maxMsgSize, msgSizeMode, fileMapSize, chunkSize, varBuffName);
		producer.runProducer(cirB);
	}
	if (role == CONSUMER)
	{
		Consumer consumer = Consumer(delay, numMessages, maxMsgSize, fileMapSize, chunkSize, varBuffName);
		consumer.runConsumer(cirB);
	}
	return 0;
}