#include "Producer.h"

Producer::Producer()
{
}

Producer::Producer(int& delay, int& numMessages, size_t& maxMsgSize, int& msgSizeMode, size_t& buffSize, int& chunkSize, LPCWSTR varBuffName)
{
	this->delay = delay;
	this->requestedMessages = numMessages;
	this->msgSizeMode = msgSizeMode;
	this->maxMsgSize = maxMsgSize;
	messageCount = 0;
	messageID = 0;
	srand(time(NULL));
}

Producer::~Producer()
{
}

void Producer::makeMessage(char* msg, size_t msgLen)
{
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	for (auto i = 0; i < msgLen; ++i) {
		msg[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
	}
	msg[msgLen-1] = '\0';

	messageID++;
}

void Producer::runProducer(circularBuffer& buffInst)
{
	//printf("Producer!\n");
	char* msg;
	int messageLength;
	msg = new char[maxMsgSize];
	messageLength = maxMsgSize - sizeof(sMsgHeader);

	while (messageCount < requestedMessages)
	{
		if (msgSizeMode == RANDOM)
		{ //MessageLength is here a value between 0 and MESSAGE-size (not including header)
			messageLength = 2 + (rand() % (maxMsgSize - sizeof(sMsgHeader)-2));
		}
		makeMessage(msg, messageLength);

		Sleep(delay);
		while (!buffInst.push(msg, messageLength))
		{
			Sleep(1);
		}

		printf("%d %s\n", messageCount, msg);
		messageCount++;
	}
	delete[]msg;
}
