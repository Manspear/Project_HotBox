#include "CircularBuffer.h"

void circularBuffer::initCircBuffer(LPCWSTR msgBuffName, const size_t & buffSize, const int& role, const size_t & chunkSize, LPCWSTR varBuffBuffName)
{
	msgFileMap = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		nullptr,
		PAGE_READWRITE,
		0,
		buffSize,
		msgBuffName
	);
	if (msgFileMap == nullptr)
	{
		LPCWSTR error = TEXT("ERROR: Failed to create FileMap for messages\n");
		OutputDebugString(error);
		abort();
	}
	varFileMap = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		nullptr,
		PAGE_READWRITE,
		0,
		sizeof(sSharedVars),
		varBuffBuffName
	);
	if (varFileMap == nullptr)
	{
		LPCWSTR error = TEXT("ERROR: Failed to create FileMap for varBuffiables\n");
		OutputDebugString(error);
		abort();
	}
	msgBuff = (char*)MapViewOfFile(
		msgFileMap,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		buffSize
	);
	varBuff = (sSharedVars*)MapViewOfFile(
		varFileMap,
		FILE_MAP_ALL_ACCESS,
		0,
		0,
		sizeof(sSharedVars)
	);

	this->buffSize = &buffSize;
	this->chunkSize = &chunkSize;
	varBuff->headPos = 0;
	varBuff->tailPos = 0;
	varBuff->freeMem = buffSize;

	lTail = varBuff->tailPos;
	msgCounter = 0;
	mutex1 = Mutex(LPCWSTR(TEXT("Mutex1")));

	//making producer wait for clients to join, giving them 400 ms each at max
	if (role == PRODUCER)
	{
		size_t old = varBuff->clientCounter;
		int ticker = 0;
		while (ticker < 4)
		{
			if (old < varBuff->clientCounter)
			{
				old = varBuff->clientCounter;
				ticker = 0;
			}
			else
				ticker++;
			Sleep(300);
		}
		this->clientCount = varBuff->clientCounter;
		varBuff->producerExist = true;
	}
	//consumer
	if (role == CONSUMER)
	{
		varBuff->clientCounter++;
		while (varBuff->producerExist == false)
		{
			Sleep(300);
		}
		varBuff->clientCounter--;
	}
}

void circularBuffer::stopCircBuffer()
{
}

bool circularBuffer::push(const void * msg, size_t length)
{
	bool res = false;
	if (varBuff->clientCounter == 0)
	{
		size_t padding = *const_cast<size_t*>(chunkSize) - ((length + sizeof(sMsgHeader)) % *const_cast<size_t*>(chunkSize));
		size_t totMsgLen = sizeof(sMsgHeader) + length + padding;
		//if there's enough space for the message
		if (varBuff->freeMem >= totMsgLen)
		{
			//if there's enough space at end of buffer, and if head is in front of tail
			if (varBuff->headPos >= varBuff->tailPos && (totMsgLen <= (*buffSize - varBuff->headPos)))
			{
				res = pushMsg(false, false, msg, length, padding, totMsgLen);
			}else 
			//if head is in front of tail, and if the space between tail and start of buffer is a fit for the msg
			if (varBuff->headPos >= varBuff->tailPos && varBuff->tailPos >= totMsgLen)
			{
				res =  pushMsg(true, true, msg, length, padding, totMsgLen);
			}
			else
			//if the head is behind the tail, the space between them is freeMem 
			if (varBuff->headPos < varBuff->tailPos)
			{
				res = pushMsg(false, false, msg, length, padding, totMsgLen);
			}
		}	
	}
	return res;
}

bool circularBuffer::pop(char * msg, size_t & length)
{
	bool res = false;
	mutex1.lock();
	if (varBuff->clientCounter == 0)
	{
		if (lTail == varBuff->headPos && varBuff->freeMem == 0)
		{
			res = procMsg(msg, &length);
		}
		else if (lTail != varBuff->headPos && varBuff->freeMem > 0)
		{
			res = procMsg(msg, &length);
		}
	}
	mutex1.unlock();
	return res;
}

bool circularBuffer::procMsg(char * msg, size_t * length)
{
	char* tempCast = msgBuff;
	tempCast += lTail;
	sMsgHeader* readMsg = (sMsgHeader*)tempCast;
	bool dummyMessage = false;
	if (readMsg->id == -1)
	{
		dummyMessage = true;
		readMsg->consumerPile--;
		lTail = 0;
	}
	else
	{
		*length = readMsg->length - sizeof(sMsgHeader);
		tempCast += sizeof(sMsgHeader);
		memcpy(msg, tempCast, *length);
		printf("%d ", readMsg->id);
		readMsg->consumerPile--;
	}
	if (readMsg->consumerPile == 0)
	{
		varBuff->freeMem += readMsg->length + readMsg->padding;

		size_t nextPos = varBuff->tailPos + readMsg->length + readMsg->padding;
		

		if(nextPos % *buffSize > 0)
		{
			varBuff->tailPos += readMsg->length + readMsg->padding;
		}
		if (nextPos % *buffSize == 0)
		{
			varBuff->tailPos = 0;
		}
	}
	//If the tail jumps to the end of the buffer
	if (dummyMessage)
	{
		return false;
	}
	else
	{
		size_t nextTailPos = lTail + readMsg->length + readMsg->padding;
		if (nextTailPos % *buffSize == 0)
		{
			lTail = 0;
			return true;
		}
		//if the tail jump is within the buffer
		else
		{
			lTail += readMsg->length + readMsg->padding;
			return true;
		}
	}
	return true;
}

bool circularBuffer::pushMsg(bool reset, bool start, const void * msg, size_t & length, size_t& padding, size_t& totMsgLength)
{
	sMsgHeader* newMsg = (sMsgHeader*)msgBuff;

	size_t lHeadPos = varBuff->headPos;
	if (!reset)
	{
		char* tempCast = (char*)newMsg;
		tempCast += lHeadPos;
		newMsg = (sMsgHeader*)tempCast;
		newMsg->consumerPile = clientCount;
		newMsg->id = msgCounter;
		msgCounter++;

		newMsg->length = sizeof(sMsgHeader) + length;
		newMsg->padding = padding;
		
		char* msgPointer = (char*)newMsg;
		msgPointer += sizeof(sMsgHeader);
		memcpy(msgPointer, msg, length);

		
		lHeadPos += totMsgLength;
		//mutex1.lock();
		varBuff->headPos = lHeadPos;
		//mutex1.unlock();
		varBuff->freeMem -= totMsgLength;

		if (varBuff->headPos >= *buffSize)
			varBuff->headPos = 0;
		return true;
	}
	if (reset)
	{
		char* tempCast = (char*)newMsg;
		tempCast += lHeadPos;
		newMsg = (sMsgHeader*)tempCast;
		newMsg->consumerPile = clientCount;
		newMsg->id = -1;
		
		newMsg->length = sizeof(sMsgHeader);
		newMsg->padding = (*buffSize - lHeadPos) - sizeof(sMsgHeader);
		
		varBuff->freeMem -= sizeof(sMsgHeader) + newMsg->padding;
		//Move the head to the start
		newMsg = (sMsgHeader*)msgBuff;
		lHeadPos = 0;

		//Make a message at the start
		newMsg->consumerPile = clientCount;
		newMsg->id = msgCounter;
		msgCounter++;
		newMsg->length = sizeof(sMsgHeader) + length;
		newMsg->padding = padding;

		char* msgPointer = (char*)newMsg;
		msgPointer += sizeof(sMsgHeader);
		memcpy(msgPointer, msg, length);

		lHeadPos += totMsgLength;
		//mutex1.lock();
		varBuff->headPos = lHeadPos;
		//mutex1.unlock();
		varBuff->freeMem -= newMsg->length + newMsg->padding;

		return true;
	}
	return false;
}

circularBuffer::circularBuffer()
{
}

circularBuffer::~circularBuffer()
{
	CloseHandle(msgFileMap);
	CloseHandle(varFileMap);
}
