#include "gameplay.h"

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
		unsigned int index = UINT_MAX;
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
	void fProcessMessage(char* messageData, gameplay::Scene* scene);

	void fRead(circularBuffer& circBuff, gameplay::Scene* scene);

	/*Functions for processing deleted nodes*/
	void fProcessDeletedObject(char* messageData, gameplay::Scene* scene);

	/*
	Functions for processing mesh messages, getting the newly mesh data and also the updated vertices from the mesh.
	*/
	void fProcessMesh(char* messageData, gameplay::Scene* scene);

	/*Functions for processing material messages, getting the newly material data abd update a existing mesh material.*/
	void fProcessMaterial(char* messageData, gameplay::Scene* scene);

	/*Functions for processing light messages and getting the new light.*/
	void fProcessLight(char* messageData, gameplay::Scene* scene);

	/*
	Functions for processing transform messages and getting the new transform.
	Creates a node with the same name as it's child node if it doesn't exist.
	Depending on it's child node type, it will create the node differently
	(if necessary)
	*/
	void fProcessTransform(char* messageData, gameplay::Scene* scene);

	/*Functions for processing camera messages and getting the new camera.*/
	void fProcessCamera(char* messageData, gameplay::Scene* scene);

	/*Functions for finding objects in the scene*/
	sFoundInfo fFindMesh(const char* mName);
	
private:
	/*Functions for creating and modifying meshes*/
	void fCreateNewMeshNode(char* meshName, hMeshVertex& vertList, 
						    hMeshHeader& meshHeader, gameplay::Node* nd,
							gameplay::Scene* scene);
	/*
	Applies its transform to the transform's node, and sets a child if there is one
	We need to be able to have more than one child in the future.
	*/
	void fModifyNodeTransform(hTransformHeader& transH, gameplay::Node* nd, gameplay::Scene* scene);
};


#endif 
