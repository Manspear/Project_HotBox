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
		//Sleep(delayTime);

		//memcpy from the buffer into msg
		while (!circBuff.pop(msg, length))
		{
			Sleep(1);
		}
		/*
		memcpy from the msg memcpied from the buffer into another message... Sounds redundant.
		look into it in the future
		*/
		fProcessMessage(msg, msgType);

		messageCount++;
	}

	delete[] msg;
}

void HMessageReader::fProcessDeletedObject(char * messageData)
{
	hRemovedObjectHeader* remoh = (hRemovedObjectHeader*)messageData + sizeof(hMainHeader);

	hRemovedObjectHeader temp;
	temp.nodeType = remoh->nodeType;
	temp.name = new char[remoh->nameLength + 1];
	memcpy((char*)temp.name, remoh + sizeof(hRemovedObjectHeader), temp.nameLength);
	(char)temp.name[remoh->nameLength] = '\0';

	removedList.push_back(temp);
}

void HMessageReader::fProcessMessage(char* messageData, HMessageReader::MessageType &msgType)
{
	/*Here the engine will act as a CONSUMER, to read the messages,
	Should return the message type we want to use in the update() function.*/

	/*Get the message type.*/
	msgType = eDefault;

	/*Read the main header to check what type of message to process.*/
	hMainHeader mainHeader = *(hMainHeader*)messageData;

	if (mainHeader.removedObjectCount > 0)
	{
		for (int i = 0; i < mainHeader.removedObjectCount; i++)
		{
			/*Process deleted object*/
			fProcessDeletedObject(messageData);
		}
	}

	if (mainHeader.meshCount > 0)
	{
		for (int i = 0; i < mainHeader.meshCount; i++)
		{
			/*Process meshdata.*/
			fProcessMesh(messageData);
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
			fProcessCamera(messageData);
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

void HMessageReader::fProcessMesh(char* messageData)
{
	/*Read the meshHeader from messageData.*/
	hMeshHeader meshHeader = *(hMeshHeader*)(messageData + sizeof(hMainHeader));
	


	meshList.push_back(meshHeader);
	
	/*Resize the vertex list for each mesh with vertex count of each mesh.*/
	//meshVertexList[meshIndex].vertexList.resize(meshHeader.vertexCount);
	hMeshVertex tempList;
	meshVertexList.push_back(tempList);

	/*These work*/
	meshList.back().meshName = new char[meshHeader.meshNameLen + 1];
	memcpy((void*)meshList.back().meshName, messageData + sizeof(hMainHeader) + sizeof(hMeshHeader), meshHeader.meshNameLen);
	(char)meshList.back().meshName[meshHeader.meshNameLen] = '\0';
	meshList.back().prntTransName = new char[meshHeader.prntTransNameLen + 1];
	memcpy((void*)meshList.back().prntTransName, messageData + sizeof(hMainHeader) + sizeof(hMeshHeader) + meshHeader.meshNameLen, meshHeader.prntTransNameLen);
	(char)meshList.back().prntTransName[meshHeader.prntTransNameLen] = '\0';
	
	//meshList[meshIndex].meshName = meshName;
	//meshList[meshIndex].prntTransName = transName;
	
	std::vector<sBuiltVertex> meshVertices;
	memcpy(&meshVertexList.back().vertexList.front(), messageData + sizeof(hMainHeader) + 
		   sizeof(hMeshHeader) + meshHeader.meshNameLen + meshHeader.prntTransNameLen, 
		   meshHeader.vertexCount * sizeof(sBuiltVertex));
}

void HMessageReader::fGetNewMesh(char * meshName, std::vector<hVertexHeader>& vertexList, unsigned int & numVertices, unsigned int * indexList, unsigned int & numIndices)
{
	/*Use the meshlist vector to get the data and the vertex list data for each mesh.*/
	memcpy(meshName, meshList.back().meshName, meshList.back().meshNameLen+1);

	numVertices = meshList.back().vertexCount;

	vertexList = meshVertexList.back().vertexList;
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

void HMessageReader::fProcessCamera(char* messageData)
{

	/*Read the hCameraHeader from messageData.*/
	hCameraHeader cameraHeader = *(hCameraHeader*)(messageData + sizeof(hMainHeader));

	cameraList.push_back(cameraHeader);

	cameraList.back().cameraNameLength = cameraHeader.cameraNameLength;
	/*Maybe unimportant*/
	//memcpy(&cameraList.back().projMatrix, &cameraHeader.projMatrix, sizeof(float) * 16);

	cameraList.back().cameraName = new char[cameraHeader.cameraNameLength + 1];
	memcpy((void*)cameraList.back().cameraName, messageData + sizeof(hMainHeader) + sizeof(hCameraHeader), cameraHeader.cameraNameLength);
	(char)cameraList.back().cameraName[cameraHeader.cameraNameLength] = '\0';
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

HMessageReader::sFoundInfo HMessageReader::fFindMesh(const char* mName)
{
	sFoundInfo fi;
	for (int i = 0; i < meshList.size(); i++)
	{
		if (std::strcmp(meshList[i].meshName, mName) == 0)
		{
			fi.index = i;
			fi.msgType = MessageType::eMeshChanged;
			return fi;
		}
	}
	fi.msgType = MessageType::eNewMesh;
	return fi;
}


