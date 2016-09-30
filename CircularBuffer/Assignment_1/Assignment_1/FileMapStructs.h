#pragma once
struct sMsgHeader
{
	size_t id;
	size_t length;
	size_t padding;
	size_t consumerPile;
};

struct sSharedVars
{
	size_t headPos;
	size_t tailPos;
	size_t freeMem;
	size_t clientCounter = 0;
	bool producerExist = false;
};