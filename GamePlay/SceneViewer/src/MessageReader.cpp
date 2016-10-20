#include "MessageReader.h"

HMessageReader::HMessageReader()
{
	bufferSize = 8 << 20;
	chunkSize = 256;
	maxSize = bufferSize / 4;
	msg = new char[maxSize];
	delayTime = 0;

	numMessages = 1;

	messageCount = 0;

	LPCWSTR msgBuffName = TEXT("MessageBuffer");
	LPCWSTR varBuffName = TEXT("VarBuffer");

	circBuff.initCircBuffer(msgBuffName, bufferSize, 1, chunkSize, varBuffName);
}

HMessageReader::~HMessageReader()
{
	delete[] msg;
}

void HMessageReader::fRead(circularBuffer& circBuff, gameplay::Scene* scene)
{
	size_t length;
	if (circBuff.pop(msg, length))
	{
		fProcessMessage(msg, scene);
	}
}

void HMessageReader::fProcessDeletedObject(char * messageData, gameplay::Scene* scene)
{
	hRemovedObjectHeader* remoh = (hRemovedObjectHeader*)(messageData + sizeof(hMainHeader));
	//remoh.name = new char[remoh.nameLength + 1];
	//remoh.name = new char[remoh.nameLength + 1];
	//memcpy(remoh.name, messageData + sizeof(hMainHeader) + sizeof(hRemovedObjectHeader), remoh.nameLength);
	//(char)remoh.name[remoh.nameLength] = '\0';
	remoh->name = (char*)messageData + sizeof(hMainHeader) + sizeof(hRemovedObjectHeader);
	//memcpy((char*)remoh.name, &remoh + sizeof(hRemovedObjectHeader), remoh.nameLength);
	//(char)remoh.name[remoh.nameLength] = '\0';
	
	//removedList.push_back(temp);
	gameplay::Node* nd = scene->findNode(remoh->name);
	if (nd != NULL)
	{
		scene->removeNode(nd);
	}
}

void HMessageReader::fProcessMessage(char* messageData, gameplay::Scene* scene)
{
	/*Here the engine will act as a CONSUMER, to read the messages,
	Should return the message type we want to use in the update() function.*/

	/*Read the main header to check what type of message to process.*/
	hMainHeader mainHeader = *(hMainHeader*)messageData;

	if (mainHeader.removedObjectCount > 0)
	{
		for (int i = 0; i < mainHeader.removedObjectCount; i++)
		{
			/*Process deleted object*/
			fProcessDeletedObject(messageData, scene);
		}
	}

	if (mainHeader.meshCount > 0)
	{
		for (int i = 0; i < mainHeader.meshCount; i++)
		{
			/*Process meshdata.*/
			fProcessMesh(messageData, scene);
		}
	}

	if (mainHeader.lightCount > 0)
	{
		for (int i = 0; i < mainHeader.lightCount; i++)
		{
			/*Process lightdata.*/
			fProcessLight(messageData, scene);
		}

	}

	if (mainHeader.cameraCount > 0)
	{
		for (int i = 0; i < mainHeader.cameraCount; i++)
		{
			/*Process cameradata.*/
			fProcessCamera(messageData, scene);
		}
	}

	if (mainHeader.materialCount > 0)
	{
		for (int i = 0; i < mainHeader.materialCount; i++)
		{
			/*Process materialdata.*/
			fProcessMaterial(messageData, scene);
		}
	}

	if (mainHeader.transformCount > 0)
	{
		for (int i = 0; i < mainHeader.transformCount; i++)
		{
			/*Process transformdata*/
			fProcessTransform(messageData, scene);
		}
	}

	if (mainHeader.hierarchyCount > 0)
	{
		for (int i = 0; i < mainHeader.hierarchyCount; i++)
		{
			fProcessHierarchy(messageData, scene);
		}
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

/*Depending on what's changed, change different values*/
void HMessageReader::fProcessMesh(char* messageData, gameplay::Scene* scene)
{
	/*Read the meshHeader from messageData.*/
	hMeshHeader* meshHeader = (hMeshHeader*)(messageData + sizeof(hMainHeader));
//char* meshName	;
	//meshName = new char[meshHeader.meshNameLen + 1];
	//memcpy(meshName, messageData + sizeof(hMainHeader) + sizeof(hMeshHeader), meshHeader.meshNameLen);
	//meshName[meshHeader.meshNameLen] = '\0';

	meshHeader->meshName = messageData + sizeof(hMainHeader) + sizeof(hMeshHeader);

	//hMeshVertex vertList;
	//vertList.vertexList.resize(meshHeader.vertexCount);
	//memcpy(&vertList.vertexList[0], messageData + sizeof(hMainHeader) +
	//	sizeof(hMeshHeader) + meshHeader.meshNameLen + meshHeader.prntTransNameLen,
	//	meshHeader.vertexCount * sizeof(sBuiltVertex));
	hVertexHeader* vtxPtr = (hVertexHeader*)(messageData + sizeof(hMainHeader) +
							sizeof(hMeshHeader) + meshHeader->meshNameLen);

	gameplay::Node* nd = scene->findNode(meshHeader->meshName);

	if (nd != NULL)
	{
		gameplay::Model* model = static_cast<gameplay::Model*>(nd->getDrawable());
		model->getMesh()->setVertexData(vtxPtr, 0, meshHeader->vertexCount);
	}
	else
	{
		fCreateNewMeshNode(const_cast<char*>(meshHeader->meshName), vtxPtr, meshHeader, nd, scene);
	}
	//delete[] meshName;
	//delete[] prntTransName;
}

void HMessageReader::fCreateNewMeshNode(char* meshName, hVertexHeader* vertList, 
										hMeshHeader* meshHeader, gameplay::Node* nd, 
										gameplay::Scene* scene)
{
	/*The node is new*/
	nd = gameplay::Node::create(meshName);

	gameplay::VertexFormat::Element elements[] = {
		gameplay::VertexFormat::Element(gameplay::VertexFormat::POSITION, 3),
		gameplay::VertexFormat::Element(gameplay::VertexFormat::TEXCOORD0, 2),
		gameplay::VertexFormat::Element(gameplay::VertexFormat::NORMAL, 3)
	};
	const gameplay::VertexFormat vertFormat(elements, ARRAYSIZE(elements));

	gameplay::Mesh* mesh = gameplay::Mesh::createMesh(vertFormat, meshHeader->vertexCount, true);
	mesh->setVertexData(vertList, 0, meshHeader->vertexCount);

	gameplay::Model* meshModel = gameplay::Model::create(mesh);
		mesh->release();

	gameplay::Material* material = meshModel->setMaterial("res/shaders/colored.vert", "res/shaders/colored.frag", "POINT_LIGHT_COUNT 1");

	gameplay::RenderState::StateBlock* block = gameplay::RenderState::StateBlock::create();
	block->setCullFace(true);
	block->setDepthTest(true);
	material->setStateBlock(block);

	material->setParameterAutoBinding("u_worldViewMatrix", gameplay::RenderState::AutoBinding::WORLD_VIEW_MATRIX);
	material->setParameterAutoBinding("u_worldViewProjectionMatrix", gameplay::RenderState::AutoBinding::WORLD_VIEW_PROJECTION_MATRIX);
	material->setParameterAutoBinding("u_inverseTransposeWorldViewMatrix", gameplay::RenderState::AutoBinding::INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX);

	gameplay::Node* lightNode = scene->findNode("pointLightShape1");

	material->getParameter("u_pointLightColor[0]")->bindValue(lightNode->getLight(), &gameplay::Light::getColor);
	material->getParameter("u_pointLightRangeInverse[0]")->bindValue(lightNode->getLight(), &gameplay::Light::getRangeInverse);
	material->getParameter("u_pointLightPosition[0]")->bindValue(lightNode, &gameplay::Node::getTranslationView);

	material->getParameter("u_ambientColor")->setValue(gameplay::Vector3(0.5, 0.5, 0.5));
	material->getParameter("u_diffuseColor")->setValue(gameplay::Vector4(0.5, 0.5, 0.5, 0.5));

	nd->setDrawable(meshModel);

	nd->setTranslation(gameplay::Vector3(0, 0, -3));

	scene->addNode(nd);
}

void HMessageReader::fModifyNodeTransform(hTransformHeader* transH, gameplay::Node* nd, gameplay::Scene* scene)
{
	nd->setTranslation(transH->trans);
	nd->setRotation((gameplay::Quaternion)transH->rot);
	nd->setScale(transH->scale);

	//nd->translate(transH->trans[0], transH->trans[1], transH->trans[2]);
	//nd->rotate(transH->rot[0], transH->rot[1], transH->rot[2], transH->rot[3]);
	//nd->scale(transH->scale[0], transH->scale[1], transH->scale[2]);
	/*This should only be used if you have more than one child*/
	//if (transH.childNameLength > 0)
	//{
	//	gameplay::Node* child = scene->findNode(transH.childName);
	//	if (child != NULL)
	//		nd->addChild(child);
	//}
}

void HMessageReader::fProcessTransformQueue(gameplay::Scene* scene)
{
	while(transNameQueue.size() > 0)
	{
		gameplay::Node* nd = scene->findNode(transNameQueue.front().childName);
		if (nd)
		{
			fModifyNodeTransform(&transNameQueue.front(), nd, scene);
			delete[] transNameQueue.front().childName;
			transNameQueue.pop();
		}
		else
			break;
	}
}


void HMessageReader::fProcessHierarchy(char * messageData, gameplay::Scene * scene)
{
	hHierarchyHeader* hiH = (hHierarchyHeader*)(messageData + sizeof(hMainHeader));
	hiH->parentNodeName = (char*)(messageData + sizeof(hMainHeader) + sizeof(hHierarchyHeader));
	gameplay::Node* parnd = scene->findNode(hiH->parentNodeName);

	if(parnd != NULL)
	{
		/*Now find the nodes of the children presented in the message*/
		hChildNodeNameHeader* cnH;
		int pastSize = 0;
		for (int i = 0; i < hiH->childNodeCount; i++)
		{
			cnH = (hChildNodeNameHeader*)(messageData + sizeof(hMainHeader) + sizeof(hHierarchyHeader) + hiH->parentNodeNameLength + pastSize);
			cnH->objName = messageData + sizeof(hMainHeader) + sizeof(hHierarchyHeader) + hiH->parentNodeNameLength + sizeof(hChildNodeNameHeader) + pastSize;
			pastSize += sizeof(hChildNodeNameHeader) + cnH->objNameLength;
			gameplay::Node* cnd = scene->findNode(cnH->objName);
			if (cnd != NULL)
			{
				parnd->addChild(cnd);
			}
			else
			{
				/*Do some dynamic memory allocation to temporarily save the names in this application*/
				fSaveHierarchy(messageData, hiH);
			}
		}
	}
	else
	{
		/*Do some dynamic memory allocation to temporarily save the names in this application*/
		fSaveHierarchy(messageData, hiH);
	}
}

void HMessageReader::fProcessHierarchyQueue(gameplay::Scene* scene)
{
	/*
	Do this once after every fRead
	gameplay3D must be able to handle
	the same node being "childed", right?
	*/
	if (hierarchyQueue.size() > 0)
	{
		bool isSuccess = true;
		gameplay::Node* parnd = scene->findNode(hierarchyQueue.front().parName);
		/* Check if the nodes are found in the scene */
		if (parnd != NULL)
		{
			gameplay::Node* cnd;
			for (int i = 0; i < hierarchyQueue.front().childNames.size(); i++)
			{
				char* nono = hierarchyQueue.front().childNames[i];
				cnd = scene->findNode(hierarchyQueue.front().childNames[i]);

				if (cnd == NULL)
				{
					isSuccess = false;
					break;
				}
			}
		}
		else
			isSuccess = false;
		/* If all of the nodes are found, do the parenting and deletes. */
		if (isSuccess)
		{
			gameplay::Node* cnd;
			for (int i = 0; i < hierarchyQueue.front().childNames.size(); i++)
			{
				cnd = scene->findNode(hierarchyQueue.front().childNames[i]);
				parnd->addChild(cnd);
				delete[] hierarchyQueue.front().childNames[i];
			}
			delete[] hierarchyQueue.front().parName;
			hierarchyQueue.pop();
		}
	}
}

void HMessageReader::fSaveHierarchy(char * messageData, hHierarchyHeader* hiH)
{
	hChildNodeNameHeader* cnH;
	sHierarchy lhie;

	lhie.parName = new char[hiH->parentNodeNameLength];
	memcpy(lhie.parName, hiH->parentNodeName, hiH->parentNodeNameLength);
	int pastSize = 0;
	for (int j = 0; j < hiH->childNodeCount; j++)
	{
		cnH = (hChildNodeNameHeader*)(messageData + sizeof(hMainHeader) + sizeof(hHierarchyHeader) + hiH->parentNodeNameLength + pastSize);
		cnH->objName = messageData + sizeof(hMainHeader) + sizeof(hHierarchyHeader) + hiH->parentNodeNameLength + sizeof(hChildNodeNameHeader) + pastSize;

		pastSize += sizeof(hChildNodeNameHeader) + cnH->objNameLength;

		char* cName = new char[cnH->objNameLength];
		memcpy(cName, cnH->objName, cnH->objNameLength);
		lhie.childNames.push_back(cName);
	}
	hierarchyQueue.push(lhie);
}

void HMessageReader::fProcessQueues(circularBuffer& circBuff, gameplay::Scene* scene)
{
	//if (tranQ.size() > 0)
	//{
	//	gameplay::Node* nd = scene->findNode(tranQ.front().childName + '\0');
	//	if (nd != NULL)
	//	{
	//		fModifyNodeTransform(tranQ.front(), nd, scene);
	//		//delete[] tranQ.front().childName;
	//		tranQ.pop();
	//	}
	//}
}

void HMessageReader::fProcessMaterial(char* messageData, gameplay::Scene* scene)
{
	hMaterialHeader* materialHeader = (hMaterialHeader*)(messageData + sizeof(hMainHeader));

	materialHeader->materialName = messageData + sizeof(hMainHeader) + sizeof(hMaterialHeader);
	materialHeader->connectedMeshName = messageData + sizeof(hMainHeader) + sizeof(hMaterialHeader) + materialHeader->materialNameLength;
}

void HMessageReader::fProcessLight(char* messageData, gameplay::Scene* scene)
{
	/*Fill the lightlist vector for this process.*/
}

void HMessageReader::fProcessTransform(char* messageData, gameplay::Scene* scene)
{
	/*Fill the transformlist vector for this process.*/
	hTransformHeader* transH = (hTransformHeader*)(messageData + sizeof(hMainHeader));
	transH->childName = (const char*)(messageData + sizeof(hMainHeader) + sizeof(hTransformHeader));
	printf("Trans: %s\n", transH->childName);
	if (transH->childNameLength > 0)
	{
		//transH.childName = new char[transH.childNameLength + 1];
		//memcpy((char*)transH.childName, messageData + sizeof(hMainHeader) + sizeof(hTransformHeader), transH.childNameLength);
		gameplay::Node* nd = scene->findNode(transH->childName);

		//if (nd == NULL)
		//{
		//	/*Add the translate header to a queue!*/
		//	tranQ.push(*transH);
		//}
		//else
		//{
		if(nd)
			fModifyNodeTransform(transH, nd, scene);
		else
		{
			/*Find save the name to the "transformqueueList"*/
			hTransformHeader lh = *transH;
			lh.childName = new char[transH->childNameLength];
			memcpy((char*)lh.childName, transH->childName, transH->childNameLength);

			transNameQueue.push(lh);
		}
			//delete[] transH.childName;
		//}
		/*
		If you know there is a child, but you can't find it, 
		try to find it again at a later time.
		So... Create a queue?
		How to create a queue?
		Just add the childName to a vector containing other names.
		This vecto we call the childFindQueue.
		It is called once per update, and it operates by FIFO.
		*/
	}
}

void HMessageReader::fProcessCamera(char* messageData, gameplay::Scene* scene)
{
	/*Read the hCameraHeader from messageData.*/
	hCameraHeader* cameraHeader = (hCameraHeader*)(messageData + sizeof(hMainHeader));

	cameraHeader->cameraName = messageData + sizeof(hMainHeader) + sizeof(hCameraHeader);

	gameplay::Node* camNode = scene->findNode(cameraHeader->cameraName);
	gameplay::Quaternion camQuat = cameraHeader->rot;

	/*If the camera already exists, update it's projection and transformation values.*/
	if (camNode != NULL)
	{
		gameplay::Camera* cam = static_cast<gameplay::Camera*>(camNode->getCamera());
		cam->setProjectionMatrix(cameraHeader->projMatrix);

		/*Set the current existing camera as active.*/
		scene->setActiveCamera(cam);

		camNode->setTranslation(cameraHeader->trans);
		camNode->setRotation(camQuat);
		camNode->setScale(cameraHeader->scale);
	}

	else 
	{
		camNode = gameplay::Node::create(cameraHeader->cameraName);

		gameplay::Camera* cam = gameplay::Camera::createPerspective(0, 0, 0, 0);
		cam->setProjectionMatrix(cameraHeader->projMatrix);

		camNode->setCamera(cam);
		scene->setActiveCamera(cam);

		camNode->setTranslation(cameraHeader->trans);
		camNode->setRotation(camQuat);
		camNode->setScale(cameraHeader->scale);

		scene->addNode(camNode);
	}
}
