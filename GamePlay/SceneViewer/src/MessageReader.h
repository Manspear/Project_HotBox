#ifndef MESSAGEREADER_H
#define MESSAGEREADER_H

#include "CircularBuffer.h"
#include "../../../MayaCallBackReader/MayaCallbackReader/MayaHeader.h"

#include <string>

class HMessageReader
{
public:

	enum MessageType
	{
		eNewMesh,
		eVertexChanged,
		eNewMaterial,
		eMaterialChanged,
		eNewTransform,
		eNewCamera,
		eCameraChanged,
		eNewLight,
		eNodeRemoved,
		eDefault
	};

	size_t bufferSize;

	int delayTime;

	int numMessages;

	int messageCount;

	int chunkSize;

	size_t maxSize;

	circularBuffer circBuff;

	HMessageReader();

	~HMessageReader();

	/*Different node types obtained from Maya plugin are retrieved here in these functions.*/
	void processMessage(char* messageData, MessageType& msgType);

	void read(circularBuffer& circBuff, std::vector<HMessageReader::MessageType>& enumList);

	/*Functions for processing mesh messages, getting the newly mesh data and also the updated vertices from the mesh.*/
	void processMesh(char* messageData, unsigned int meshCount);
	void getNewMesh(char * meshName, std::vector<hVertexHeader>& vertexList, unsigned int & numVertices, unsigned int * indexList, unsigned int & numIndices);
	void getVertexUpdate(char* meshName, void* updatedVertexList, unsigned int* indexlist, unsigned int& numVerticesModified);

	/*Functions for processing material messages, getting the newly material data abd update a existing mesh material.*/
	void processMaterial(char* messageData);
	void getNewMaterial(char* materialName, char* texturePath, float ambient[3], float diffuse[3], float specular[3]);
	void getChangedMaterial(char* meshName, char* materialName);

	/*Functions for processing light messages and getting the new light.*/
	void processLight(char* messageData);
	void getNewLight(float color[3], float range);

	/*Functions for processing transform messages and getting the new transform.*/
	void processTransform(char* messageData);
	void getNewTransform(char* childName, float translation[3], float scale[3], float rotation[4]);

	/*Functions for processing camera messages and getting the new camera.*/
	void processCamera(char* messageData);
	void getNewCamera(char* cameraName, float cameraMatrix[4][4]);

private:

};

#endif 
