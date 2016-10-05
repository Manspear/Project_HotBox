#include "MayaHeader.h"
#include "MessageReader.h"

HMessageReader::HMessageReader()
{
	bufferSize = 1 << 20;
	chunkSize = 256;
	maxSize = bufferSize / 4;

	delayTime = 0;

	numMessages = 1;

	messageCount = 0;

	LPCWSTR msgBuffName = TEXT("MessageBuffer");
	LPCWSTR varBuffName = TEXT("VarBuffer");

	circBuff.initCircBuffer(msgBuffName, bufferSize, 1, chunkSize, varBuffName);
}

HMessageReader::~HMessageReader()
{
}

void HMessageReader::read(circularBuffer& circBuff, std::vector<HMessageReader::MessageType>& enumList)
{
	char* msg = new char[maxSize];
	while (messageCount < numMessages)
	{
		size_t length;
		Sleep(delayTime);

		while (!circBuff.pop(msg, length))
		{
			Sleep(1);
		}

		HMessageReader::MessageType msgType;

		processMessage(msg, msgType);

		enumList.push_back(msgType);

		messageCount++;
	}
}

void HMessageReader::processMessage(char* messageData, HMessageReader::MessageType &msgType)
{
	/*Here the engine will act as a CONSUMER, to read the messages,
	Should return the message type we want to use in the update() function.*/

	/*Get the message type.*/
	msgType = eDefault;

	/*Read the main header to check what type of message to process.*/
	hMainHeader mainHeader;

	mainHeader.meshCount = 1;
	mainHeader.lightCount = 0;
	mainHeader.cameraCount = 0;
	mainHeader.materialCount = 0;
	mainHeader.transformCount = 0;

	if (mainHeader.meshCount > 0)
	{
		for (int i = 0; i < mainHeader.meshCount; i++)
		{
			/*Process meshdata.*/
			processMesh(messageData);
		}

		msgType = eNewMesh;
	}

	if (mainHeader.lightCount > 0)
	{
		for (int i = 0; i < mainHeader.lightCount; i++)
		{
			/*Process lightdata.*/
			processLight(messageData);
		}

		msgType = eNewLight;
	}

	if (mainHeader.cameraCount > 0)
	{
		for (int i = 0; i < mainHeader.cameraCount; i++)
		{
			/*Process cameradata.*/
			processCamera(messageData);
		}

		msgType = eNewCamera;
	}

	if (mainHeader.materialCount > 0)
	{
		for (int i = 0; i < mainHeader.materialCount; i++)
		{
			/*Process materialdata.*/
			processLight(messageData);
		}

		msgType = eNewMaterial;
	}

	if (mainHeader.transformCount > 0)
	{
		for (int i = 0; i < mainHeader.transformCount; i++)
		{
			/*Process transformdata*/
			processTransform(messageData);
		}

		msgType = eNewTransform;
	}
}

void HMessageReader::processMesh(char* messageData)
{
	/*Fill the meshlist vector for this process.*/
}

void HMessageReader::getNewMesh(char * meshName, void * vertexList, unsigned int & numVertices, unsigned int * indexList, unsigned int & numIndices)
{
	/*Use the meshlist vector to get the data.*/
}

void HMessageReader::getVertexUpdate(char * meshName, void * updatedVertexList, unsigned int * indexlist, unsigned int & numVerticesModified)
{
}

void HMessageReader::processMaterial(char* messageData)
{
	/*Fill the materiallist vector for this process.*/
}

void HMessageReader::getNewMaterial(char * materialName, char * texturePath, float ambient[3], float diffuse[3], float specular[3])
{
	/*Use the materiallist vector to get the data.*/
}

void HMessageReader::getChangedMaterial(char * meshName, char * materialName)
{
}

void HMessageReader::processLight(char* messageData)
{
	/*Fill the lightlist vector for this process.*/
}

void HMessageReader::getNewLight(float color[3], float range)
{
	/*Use the lightlist vector to get the data.*/
}

void HMessageReader::processTransform(char* messageData)
{
	/*Fill the transformlist vector for this process.*/
}

void HMessageReader::getNewTransform(char * childName, float translation[3], float scale[3], float rotation[4])
{
	/*Use the transformlist to get the data.*/
}

void HMessageReader::processCamera(char* messageData)
{
	/*Fill the cameralist for this process.*/
}

void HMessageReader::getNewCamera(char * cameraName, float cameraMatrix[4][4])
{
	/*Use the cameralist to get the data.*/
}

