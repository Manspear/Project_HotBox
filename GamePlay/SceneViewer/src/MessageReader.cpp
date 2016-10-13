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
	for (int meshIndex = 0; meshIndex < meshList.size(); meshIndex++)
	{

	}
}

void HMessageReader::fRead(circularBuffer& circBuff, MessageType& msgType)
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

		fProcessMessage(msg, msgType);

		messageCount++;
	}

	delete msg;
}

void HMessageReader::fProcessMessage(char* messageData, HMessageReader::MessageType &msgType)
{
	/*Here the engine will act as a CONSUMER, to read the messages,
	Should return the message type we want to use in the update() function.*/

	/*Get the message type.*/
	msgType = eDefault;

	/*Read the main header to check what type of message to process.*/
	hMainHeader mainHeader = *(hMainHeader*)messageData;

	if (mainHeader.meshCount > 0)
	{
		for (int i = 0; i < mainHeader.meshCount; i++)
		{
			/*Process meshdata.*/
			fProcessMesh(messageData, mainHeader.meshCount);
		}

		msgType = eNewMesh;
	}

	if (mainHeader.lightCount > 0)
	{
		for (int i = 0; i < mainHeader.lightCount; i++)
		{
			/*Process lightdata.*/
			fProcessLight(messageData);
		}

		msgType = eNewLight;
	}

	if (mainHeader.cameraCount > 0)
	{
		for (int i = 0; i < mainHeader.cameraCount; i++)
		{
			/*Process cameradata.*/
			fProcessCamera(messageData, mainHeader.cameraCount);
		}

		msgType = eNewCamera;
	}

	if (mainHeader.materialCount > 0)
	{
		for (int i = 0; i < mainHeader.materialCount; i++)
		{
			/*Process materialdata.*/
			fProcessLight(messageData);
		}

		msgType = eNewMaterial;
	}

	if (mainHeader.transformCount > 0)
	{
		for (int i = 0; i < mainHeader.transformCount; i++)
		{
			/*Process transformdata*/
			fProcessTransform(messageData);
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

void HMessageReader::fProcessMesh(char* messageData, unsigned int meshCount)
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
		(char)meshList[meshIndex].meshName[meshHeader.meshNameLen] = '\0';
		meshList[meshIndex].prntTransName = new char[meshHeader.prntTransNameLen + 1];
		memcpy((void*)meshList[meshIndex].prntTransName, messageData + sizeof(hMainHeader) + sizeof(hMeshHeader) + meshHeader.meshNameLen, meshHeader.prntTransNameLen);
		(char)meshList[meshIndex].prntTransName[meshHeader.prntTransNameLen] = '\0';

		//meshList[meshIndex].meshName = meshName;
		//meshList[meshIndex].prntTransName = transName;
		
		std::vector<sBuiltVertex> meshVertices;
		memcpy(&meshVertexList[meshIndex].vertexList.front(), messageData + sizeof(hMainHeader) + sizeof(hMeshHeader) + meshHeader.meshNameLen + meshHeader.prntTransNameLen, meshHeader.vertexCount * sizeof(sBuiltVertex));
	
		meshVertexList[meshIndex].vertexList;
	}
}

void HMessageReader::fGetNewMesh(char * meshName, std::vector<hVertexHeader>& vertexList, unsigned int & numVertices, unsigned int * indexList, unsigned int & numIndices)
{
	/*Use the meshlist vector to get the data and the vertex list data for each mesh.*/
	for (int meshIndex = 0; meshIndex < meshList.size(); meshIndex++)
	{
		memcpy(meshName, meshList[meshIndex].meshName, meshList[meshIndex].meshNameLen+1);

		numVertices = meshList[meshIndex].vertexCount;

		vertexList = meshVertexList[meshIndex].vertexList;
	}
}

void HMessageReader::fGetVertexUpdate(char * meshName, void * updatedVertexList, unsigned int * indexlist, unsigned int & numVerticesModified)
{
}

void HMessageReader::fProcessMaterial(char* messageData)
{
	/*Fill the materiallist vector for this process.*/
}

void HMessageReader::fGetNewMaterial(char * materialName, char * texturePath, float ambient[3], float diffuse[3], float specular[3])
{
	/*Use the materiallist vector to get the data.*/
}

void HMessageReader::fGetChangedMaterial(char * meshName, char * materialName)
{
}

void HMessageReader::fProcessLight(char* messageData)
{
	/*Fill the lightlist vector for this process.*/
}

void HMessageReader::fGetNewLight(float color[3], float range)
{
	/*Use the lightlist vector to get the data.*/
}

void HMessageReader::fProcessTransform(char* messageData)
{
	/*Fill the transformlist vector for this process.*/
}

void HMessageReader::fGetNewTransform(char * childName, float translation[3], float scale[3], float rotation[4])
{
	/*Use the transformlist to get the data.*/
}

void HMessageReader::fProcessCamera(char* messageData, unsigned int cameraCount)
{
	cameraList.resize(cameraCount);

	for (int cameraIndex = 0; cameraIndex < cameraList.size(); cameraIndex++)
	{
		/*Read the hCameraHeader from messageData.*/
		hCameraHeader cameraHeader = *(hCameraHeader*)(messageData + sizeof(hMainHeader));

		cameraList[cameraIndex].cameraNameLength = cameraHeader.cameraNameLength;

		memcpy(&cameraList[cameraIndex].projMatrix, &cameraHeader.projMatrix, sizeof(float) * 16);

		cameraList[cameraIndex].cameraName = new char[cameraHeader.cameraNameLength + 1];
		memcpy((void*)cameraList[cameraIndex].cameraName, messageData + sizeof(hMainHeader) + sizeof(hCameraHeader), cameraHeader.cameraNameLength);
		(char)cameraList[cameraIndex].cameraName[cameraHeader.cameraNameLength] = '\0';
	}
}

void HMessageReader::fGetNewCamera(char * cameraName, float cameraProjMatrix[16])
{
	/*Use the cameralist to get the data.*/
	for (int cameraIndex = 0; cameraIndex < cameraList.size(); cameraIndex++)
	{
		memcpy(cameraName, cameraList[cameraIndex].cameraName, cameraList[cameraIndex].cameraNameLength + 1);

		memcpy(cameraProjMatrix, &cameraList[cameraIndex].projMatrix, sizeof(float) * 16);
	}
}

void HMessageReader::fCameraChanged(char * cameraName)
{
}


