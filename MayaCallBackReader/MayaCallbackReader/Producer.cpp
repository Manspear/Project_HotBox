#include "Producer.h"
#include "MayaHeader.h"
Producer::Producer()
{
}

Producer::Producer(int numMessages, int chunkSize, LPCWSTR varBuffName)
{
	//this->delay = delay;
	this->requestedMessages = numMessages;
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

void Producer::runProducer(circularBuffer* buffInst, char* msg, size_t packetSize)
{
	//printf("Producer!\n");
	//char* msg;
	//int messageLength;
	//messageLength = maxMsgSize - sizeof(sMsgHeader);
    //messageLength = packetSize;

	while (messageCount < requestedMessages)
	{
		//Sleep(delay);
		while (!buffInst->push(msg, packetSize))
		{
			MGlobal::displayInfo(MString("Push failed"));
			Sleep(1);
		}
		//This gives back the name. So: Either we do something wrong in the engine, 
		//or something happens in the circularbuffer
		//if (packetSize == (sizeof(hMainHeader) + sizeof(hTransformHeader)))
		//{
		//	hTransformHeader popo = *(hTransformHeader*)(msg + sizeof(hMainHeader));
		//	MGlobal::displayInfo(MString("Childname: ") + popo.childName);
		//}
		
		//printf("%d %s\n", messageCount, msg);
		messageCount++;
	}
	messageCount = 0;
}
