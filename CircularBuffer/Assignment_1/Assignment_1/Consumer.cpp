#include "Consumer.h"

void Consumer::runConsumer(circularBuffer& buffInst)
{
	char* msg = new char[maxMsgSize];
	while (messageCount < requestedMessages)
	{
		size_t length;
		Sleep(delay);
		while (!buffInst.pop(msg, length))
		{
			Sleep(1);
		}
		printf("%s\n", msg);
		messageCount++;
	}
}

Consumer::Consumer()
{
}

Consumer::Consumer(int & delay, int & numMessages, size_t & maxMsgSize, size_t & buffSize, int & chunkSize, LPCWSTR varBuffName)
{
	this->delay = delay;
	this->requestedMessages = numMessages;
	this->maxMsgSize = maxMsgSize;
	messageCount = 0;
}

Consumer::~Consumer()
{
}
