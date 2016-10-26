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

	lightNode = gameplay::Node::create();

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
		gameplay::Node* parNd = nd->getParent();
		if (parNd != NULL)
			parNd->removeChild(nd);
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

	if (mainHeader.childRemovedCount > 0)
	{
		for (int i = 0; i < mainHeader.childRemovedCount; i++)
		{
			fProcessChildChange(messageData, scene, eChangeType::eRemoveChild);
		}
	}
	if (mainHeader.childAddedCount > 0)
	{
		for (int i = 0; i < mainHeader.childAddedCount; i++)
		{
			fProcessChildChange(messageData, scene, eChangeType::eAddChild);
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

/*
"Once a vertex buffer has been set, you cannot change the number of vertices sent into it"
- Franscisco
*/
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
		if (model->getMesh()->getVertexCount() == meshHeader->vertexCount)
			model->getMesh()->setVertexData(vtxPtr, 0, meshHeader->vertexCount);
		else 
		if (model->getMesh()->getVertexCount() != meshHeader->vertexCount)
		{
			nd->setDrawable(NULL);
			fRefreshMeshNode(vtxPtr, meshHeader, nd, scene);
		}
	}
	else
		fCreateNewMeshNode(const_cast<char*>(meshHeader->meshName), vtxPtr, meshHeader, nd, scene);
}

void HMessageReader::fRefreshMeshNode(hVertexHeader* vertList,
	hMeshHeader* meshHeader, gameplay::Node* nd, gameplay::Scene* scene)
{

	gameplay::VertexFormat::Element elements[] = {
		gameplay::VertexFormat::Element(gameplay::VertexFormat::POSITION, 3),
		gameplay::VertexFormat::Element(gameplay::VertexFormat::TEXCOORD0, 2),
		gameplay::VertexFormat::Element(gameplay::VertexFormat::NORMAL, 3)
	};
	const gameplay::VertexFormat vertFormat(elements, ARRAYSIZE(elements));

	gameplay::Mesh* mesh = gameplay::Mesh::createMesh(vertFormat, meshHeader->vertexCount, false);
	mesh->setVertexData(vertList, 0, meshHeader->vertexCount);

	gameplay::Model* meshModel = gameplay::Model::create(mesh);
	mesh->release();

	nd->setDrawable(meshModel);

	scene->addNode(nd);
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

	gameplay::Mesh* mesh = gameplay::Mesh::createMesh(vertFormat, meshHeader->vertexCount, false);
	mesh->setVertexData(vertList, 0, meshHeader->vertexCount);

	gameplay::Model* meshModel = gameplay::Model::create(mesh);
	mesh->release();

	nd->setDrawable(meshModel);

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
using namespace gameplay;
void HMessageReader::fProcessChildChange(char * messageData, gameplay::Scene* scene, eChangeType tp)
{
	hParChildHeader* pc = (hParChildHeader*)(messageData + sizeof(hMainHeader));
	pc->parentName = messageData + sizeof(hMainHeader) + sizeof(hParChildHeader);
	pc->childName = messageData + sizeof(hMainHeader) + sizeof(hParChildHeader) + pc->parentNameLength;

	gameplay::Node* parNode = scene->findNode(pc->parentName);
	gameplay::Node* childNode = scene->findNode(pc->childName);

	if (tp == eChangeType::eRemoveChild)
	{
		//gameplay::Vector3 trans = childNode->getTranslation();
		//gameplay::Quaternion rot = childNode->getRotation();
		//gameplay::Vector3 scale = childNode->getScale();

		//gameplay::Model* model = static_cast<gameplay::Model*>(childNode->getDrawable());
		//gameplay::Mesh* mesh = model->getMesh();
		//gameplay::Material* material = model->getMaterial();

		//gameplay::MaterialParameter* matParam = material->getParameterByIndex(0);

		printf("Childremoved! \n");

		Node* asdif = childNode->clone();
		
		parNode->removeChild(childNode);

		scene->addNode(asdif);
	}
	else if (tp == eChangeType::eAddChild)
		parNode->addChild(childNode);
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

void HMessageReader::fProcessQueues(gameplay::Scene* scene)
{
	/*Looping through queues.*/
	fProcessTransformQueue(scene);
	/*This needs to be laid after fRead. Otherwise odd hierarchy stuff happen, probably because of
	removeMessage*/
	fProcessHierarchyQueue(scene);
}

void HMessageReader::fProcessMaterial(char* messageData, gameplay::Scene* scene)
{
	hMaterialHeader* materialHeader = (hMaterialHeader*)(messageData + sizeof(hMainHeader));

	materialHeader->connectMeshList.resize(materialHeader->numConnectedMeshes);

	if (materialHeader->isTexture == true)
	{
		materialHeader->colorMap = (char*)((char*)messageData + sizeof(hMainHeader) + sizeof(hMaterialHeader));
	}

	int prevSize = 0;

	for (int i = 0; i < materialHeader->connectMeshList.size(); i++)
	{
		hMeshConnectMaterialHeader* hConnectMaterial;

		if (materialHeader->isTexture == true)
		{
			hConnectMaterial = (hMeshConnectMaterialHeader*)(messageData + sizeof(hMainHeader) + sizeof(hMaterialHeader) + materialHeader->colorMapLength + prevSize);
		}

		else
		{
			hConnectMaterial = (hMeshConnectMaterialHeader*)(messageData + sizeof(hMainHeader) + sizeof(hMaterialHeader) + prevSize);
		}

		hConnectMaterial->connectMeshName = (char*)((char*)hConnectMaterial + sizeof(hMeshConnectMaterialHeader));

		prevSize += sizeof(hMeshConnectMaterialHeader) + hConnectMaterial->connectMeshNameLength;

		gameplay::Node* meshNode = scene->findNode(hConnectMaterial->connectMeshName);

		if (meshNode != NULL)
		{
			gameplay::Model* meshModel = static_cast<gameplay::Model*>(meshNode->getDrawable());
			gameplay::Material* material;
			gameplay::Texture::Sampler* colorTexture;

			if (materialHeader->isTexture == true)
			{
				material = meshModel->setMaterial("res/shaders/textured.vert", "res/shaders/textured.frag", "POINT_LIGHT_COUNT 1");

				/*Gameplay3D being retarded because you can't send jpg texture files when creating sampler.*/
				colorTexture = gameplay::Texture::Sampler::create(materialHeader->colorMap, false);

				colorTexture->setFilterMode(Texture::LINEAR, Texture::LINEAR);
				colorTexture->setWrapMode(Texture::CLAMP, Texture::CLAMP);
			}

			else
			{
				material = meshModel->setMaterial("res/shaders/colored.vert", "res/shaders/colored.frag", "POINT_LIGHT_COUNT 1");
			}

			gameplay::RenderState::StateBlock* block = gameplay::RenderState::StateBlock::create();
			block->setCullFace(true);
			block->setDepthTest(true);
			block->setDepthWrite(true);
			block->setStencilTest(true);

			material->setStateBlock(block);

			material->setParameterAutoBinding("u_worldViewMatrix", gameplay::RenderState::AutoBinding::WORLD_VIEW_MATRIX);
			material->setParameterAutoBinding("u_worldViewProjectionMatrix", gameplay::RenderState::AutoBinding::WORLD_VIEW_PROJECTION_MATRIX);
			material->setParameterAutoBinding("u_inverseTransposeWorldViewMatrix", gameplay::RenderState::AutoBinding::INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX);

			if (scene->findNode(lightNode->getId()))
			{
				material->getParameter("u_pointLightColor[0]")->bindValue(lightNode->getLight(), &gameplay::Light::getColor);
				material->getParameter("u_pointLightRangeInverse[0]")->bindValue(lightNode->getLight(), &gameplay::Light::getRangeInverse);
				material->getParameter("u_pointLightPosition[0]")->bindValue(lightNode, &gameplay::Node::getTranslationView);
			}
			
			material->getParameter("u_ambientColor")->setValue(gameplay::Vector3(materialHeader->ambient[0], materialHeader->ambient[1], materialHeader->ambient[2]));

			if (materialHeader->isTexture == true)
			{
				material->getParameter("u_diffuseTexture")->setValue(colorTexture);
			}

			else
			{
				material->getParameter("u_diffuseColor")->setValue(gameplay::Vector4(materialHeader->diffuseColor[0], materialHeader->diffuseColor[1], materialHeader->diffuseColor[2], materialHeader->diffuseColor[3]));
			}

			meshModel->setMaterial(material);
		}
	}
}

void HMessageReader::fProcessLight(char* messageData, gameplay::Scene* scene)
{
	/*Read the hLightHeader from messageData.*/
	hLightHeader* lightHeader = (hLightHeader*)(messageData + sizeof(hMainHeader));

	lightHeader->lightName = messageData + sizeof(hMainHeader) + sizeof(hLightHeader);

	lightNode = scene->findNode(lightHeader->lightName);

	if (lightNode != NULL)
	{
		gameplay::Light* light = static_cast<gameplay::Light*>(lightNode->getLight());

		light->setColor(gameplay::Vector3(lightHeader->color[0], lightHeader->color[1], lightHeader->color[2]));

		lightNode->setLight(light);
	}

	else
	{
		lightNode = gameplay::Node::create(lightHeader->lightName);

		gameplay::Light* light = gameplay::Light::createPoint(gameplay::Vector3(lightHeader->color[0], lightHeader->color[1], lightHeader->color[2]), 100);

		lightNode->setLight(light);

		light->release();

		scene->addNode(lightNode);
	}
}

void HMessageReader::fProcessTransform(char* messageData, gameplay::Scene* scene)
{
	/*Fill the transformlist vector for this process.*/
	hTransformHeader* transH = (hTransformHeader*)(messageData + sizeof(hMainHeader));
	transH->childName = (const char*)(messageData + sizeof(hMainHeader) + sizeof(hTransformHeader));
	//printf("Trans: %s\n", transH->childName);
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

		cameraHeader->projMatrix[10] *= -1;
		cameraHeader->projMatrix[14] *= -1;
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

		cameraHeader->projMatrix[10] *= -1;
		cameraHeader->projMatrix[14] *= -1;
		cam->setProjectionMatrix(cameraHeader->projMatrix);

		camNode->setCamera(cam);
		scene->setActiveCamera(cam);

		camNode->setTranslation(cameraHeader->trans);
		camNode->setRotation(camQuat);
		camNode->setScale(cameraHeader->scale);

		scene->addNode(camNode);
	}
}
