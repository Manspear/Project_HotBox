#include "MessageReader.h"

HMessageReader::HMessageReader()
{
	bufferSize = 8 << 20;
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
	//messageData + sizeof(hMainHeader);

	hMainHeader mainHeader = *(hMainHeader*)messageData;
	if (mainHeader.meshCount > 0)
	{
		for (int i = 0; i < mainHeader.meshCount; i++)
		{
			/*Process meshdata.*/
			processMesh(messageData, mainHeader.meshCount);
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

struct sPoint
{
	float x, y, z;
};
struct sUV
{
	float u, v;
};
struct sNormal
{
	float x, y, z;
};
struct sBuiltVertex
{
	sPoint pnt;
	sUV uv;
	sNormal nor;
};
struct sMeshVertices
{
	std::vector<sBuiltVertex> vertices;
};

void HMessageReader::processMesh(char* messageData, unsigned int meshCount)
{
	meshList.resize(meshCount);
	meshVertexList.resize(meshCount);

	for (int meshIndex = 0; meshIndex < meshList.size(); meshIndex++)
	{
		/*Read the meshHeader from messageData.*/
		hMeshHeader meshHeader = *(hMeshHeader*)(messageData + sizeof(hMainHeader));
	
		/*From the messageData, obtain the data from the hMeshHeader.*/
		meshList[meshIndex].meshNameLen = meshHeader.meshNameLen;
		meshList[meshIndex].materialId = meshHeader.materialId;
		meshList[meshIndex].meshNameLen = meshHeader.meshNameLen;

		meshList[meshIndex].vertexCount = meshHeader.vertexCount;

		/*Resize the vertex list for each mesh with vertex count of each mesh.*/
		meshVertexList[meshIndex].vertexList.resize(meshHeader.vertexCount);

		/*These work*/
		meshList[meshIndex].meshName = new char[meshHeader.meshNameLen + 1];
		memcpy((void*)meshList[meshIndex].meshName, messageData + sizeof(hMainHeader) + sizeof(hMeshHeader), meshHeader.meshNameLen);
		meshList[meshIndex].meshName[meshHeader.meshNameLen] = '\0';
		meshList[meshIndex].prntTransName = new char[meshHeader.prntTransNameLen + 1];
		memcpy(meshList[meshIndex].prntTransName, messageData + sizeof(hMainHeader) + sizeof(hMeshHeader) + meshHeader.meshNameLen, meshHeader.prntTransNameLen);
		meshList[meshIndex].prntTransName = '\0';

		//meshList[meshIndex].meshName = meshName;
		//meshList[meshIndex].prntTransName = transName;
		
		std::vector<sBuiltVertex> meshVertices;
		memcpy(&meshVertexList[meshIndex].vertexList.front(), messageData + sizeof(hMainHeader) + sizeof(hMeshHeader) + meshHeader.meshNameLen + meshHeader.prntTransNameLen, meshHeader.vertexCount * sizeof(sBuiltVertex));
	
		meshVertexList[meshIndex].vertexList;
	}
}

void HMessageReader::getNewMesh(char * meshName, std::vector<hVertexHeader>& vertexList, unsigned int & numVertices, unsigned int * indexList, unsigned int & numIndices)
{
	/*Use the meshlist vector to get the data and the vertex list data for each mesh.*/
	for (int meshIndex = 0; meshIndex < meshList.size(); meshIndex++)
	{
		memcpy(meshName, meshList[meshIndex].meshName, meshList[meshIndex].meshNameLen+1);
		//meshName = meshList[meshIndex].meshName;

		numVertices = meshList[meshIndex].vertexCount;

		vertexList = meshVertexList[meshIndex].vertexList;
	}
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

