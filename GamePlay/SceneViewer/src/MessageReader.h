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
		eMeshChanged,
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

	struct sFoundInfo
	{
		MessageType msgType;
		unsigned int index = INT_MAX;
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
	void fProcessMessage(char* messageData, MessageType& msgType);

	void fRead(circularBuffer& circBuff, MessageType& msgType);

	/*Functions for processing deleted nodes*/
	void fProcessDeletedObject(char* messageData);

	/*Functions for processing mesh messages, getting the newly mesh data and also the updated vertices from the mesh.*/
	void fProcessMesh(char* messageData);
	void fGetNewMesh(char * meshName, std::vector<hVertexHeader>& vertexList, unsigned int & numVertices, unsigned int * indexList, unsigned int & numIndices);
	void fGetVertexUpdate(char* meshName, void* updatedVertexList, unsigned int* indexlist, unsigned int& numVerticesModified);

	/*Functions for processing material messages, getting the newly material data abd update a existing mesh material.*/
	void fProcessMaterial(char* messageData);
	void fGetNewMaterial(char* materialName, char* texturePath, float ambient[3], float diffuse[3], float specular[3]);
	void fGetChangedMaterial(char* meshName, char* materialName);

	/*Functions for processing light messages and getting the new light.*/
	void fProcessLight(char* messageData);
	void fGetNewLight(float color[3], float range);

	/*Functions for processing transform messages and getting the new transform.*/
	void fProcessTransform(char* messageData);
	void fGetNewTransform(char* childName, float translation[3], float scale[3], float rotation[4]);

	/*Functions for processing camera messages and getting the new camera.*/
	void fProcessCamera(char* messageData);
	void fGetNewCamera(char* cameraName, float cameraProjMatrix[16]);
	void fCameraChanged(char* cameraName);

	/*Functions for finding objects in the scene*/
	sFoundInfo fFindMesh(const char* mName);

private:

};

#endif 
