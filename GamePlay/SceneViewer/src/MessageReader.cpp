#include "MessageReader.h"

HMessageReader::HMessageReader()
{
	bufferSize = 200 << 20;
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
	/*If the CONSUMER have anything to read, process what have been read.*/
	if (circBuff.pop(msg, length))
	{
		fProcessMessage(msg, scene);
	}
}

void HMessageReader::fProcessDeletedObject(char * messageData, gameplay::Scene* scene)
{
	hRemovedObjectHeader* remoh = (hRemovedObjectHeader*)(messageData + sizeof(hMainHeader));

	remoh->name = (char*)messageData + sizeof(hMainHeader) + sizeof(hRemovedObjectHeader);
	
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
	/*Read the main header to check what type of message to process.
	NOTE: There always be one message sent for each specific sent from Maya.*/
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
			/*Process mesh data.*/
			fProcessMesh(messageData, scene);
		}
	}

	if (mainHeader.lightCount > 0)
	{
		for (int i = 0; i < mainHeader.lightCount; i++)
		{
			/*Process light data.*/
			fProcessLight(messageData, scene);
		}
	}

	if (mainHeader.cameraCount > 0)
	{
		for (int i = 0; i < mainHeader.cameraCount; i++)
		{
			/*Process camera data.*/
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
			/*Process transform data.*/
			fProcessTransform(messageData, scene);
		}
	}

	if (mainHeader.hierarchyCount > 0)
	{
		for (int i = 0; i < mainHeader.hierarchyCount; i++)
		{
			/*Process hierarchy data.*/
			fProcessHierarchy(messageData, scene);
		}
	}

	if (mainHeader.childRemovedCount > 0)
	{
		for (int i = 0; i < mainHeader.childRemovedCount; i++)
		{
			/*Process child data when adding.*/
			fProcessChildChange(messageData, scene, eChangeType::eRemoveChild);
		}
	}
	if (mainHeader.childAddedCount > 0)
	{
		for (int i = 0; i < mainHeader.childAddedCount; i++)
		{
			/*Process chil data when changing.*/
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

	meshHeader->meshName = messageData + sizeof(hMainHeader) + sizeof(hMeshHeader);

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
			//nd->setDrawable(NULL);
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

	/*Get the material of the old model*/
	gameplay::Model* model = static_cast<gameplay::Model*>(nd->getDrawable());
	gameplay::Material* oldMat = model->getMaterial();

	meshModel->setMaterial(oldMat);

	oldMat->addRef();

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
	/*Read the material hMaterialHeader from messageData.*/
	hMaterialHeader* materialHeader = (hMaterialHeader*)(messageData + sizeof(hMainHeader));

	materialHeader->connectMeshList.resize(materialHeader->numConnectedMeshes);

	/*Read the name of the texture map if one exists.*/
	if (materialHeader->isTexture == true)
		materialHeader->colorMap = (char*)((char*)messageData + sizeof(hMainHeader) + sizeof(hMaterialHeader));

	int prevSize = 0;

	for (int i = 0; i < materialHeader->connectMeshList.size(); i++)
	{
		hMeshConnectMaterialHeader* hConnectMaterial;

		/*Read the hMeshConnectMaterialHeader different depending on if textures exist or not.*/
		if (materialHeader->isTexture == true)
			hConnectMaterial = (hMeshConnectMaterialHeader*)(messageData + sizeof(hMainHeader) + sizeof(hMaterialHeader) + materialHeader->colorMapLength + prevSize);

		else
			hConnectMaterial = (hMeshConnectMaterialHeader*)(messageData + sizeof(hMainHeader) + sizeof(hMaterialHeader) + prevSize);

		/*Read one or several names of meshes connected with the material.*/
		hConnectMaterial->connectMeshName = (char*)((char*)hConnectMaterial + sizeof(hMeshConnectMaterialHeader));

		prevSize += sizeof(hMeshConnectMaterialHeader) + hConnectMaterial->connectMeshNameLength;

		/*If the connected mesh already exists in the scene, obtain and "create/update" it's material and texture.*/
		gameplay::Node* meshNode = scene->findNode(hConnectMaterial->connectMeshName);
		if (meshNode != NULL)
		{
			gameplay::Model* meshModel = static_cast<gameplay::Model*>(meshNode->getDrawable());
			gameplay::Material* material;
			gameplay::Texture::Sampler* colorTexture;

			/*If there are textures in the material, set the material for vert and frag shaders for textures.*/
			if (materialHeader->isTexture == true)
			{
				material = meshModel->setMaterial("res/shaders/textured.vert", "res/shaders/textured.frag", "POINT_LIGHT_COUNT 1");

				/*NOTE: Gameplay3D do not support jpg files.*/
				colorTexture = gameplay::Texture::Sampler::create(materialHeader->colorMap, false);

				/*These filter and wrap modes are set, for now.*/
				colorTexture->setFilterMode(Texture::LINEAR, Texture::LINEAR);
				colorTexture->setWrapMode(Texture::CLAMP, Texture::CLAMP);
			}
			/*If there are no textures in the material, set the material for colored vert and frag shaders.*/
			else
				material = meshModel->setMaterial("res/shaders/colored.vert", "res/shaders/colored.frag", "POINT_LIGHT_COUNT 1");
		
			/*First strange thing: render state of Gameplay3D have to be set with the materials of meshes.*/
			gameplay::RenderState::StateBlock* block = gameplay::RenderState::StateBlock::create();
			block->setCullFace(true);
			block->setDepthTest(true);
			block->setDepthWrite(true);
			block->setStencilTest(true);

			material->setStateBlock(block);

			/*Second strange thing: the world, view and projection matrices have to be set also with the material.*/
			material->setParameterAutoBinding("u_worldViewMatrix", gameplay::RenderState::AutoBinding::WORLD_VIEW_MATRIX);
			material->setParameterAutoBinding("u_worldViewProjectionMatrix", gameplay::RenderState::AutoBinding::WORLD_VIEW_PROJECTION_MATRIX);
			material->setParameterAutoBinding("u_inverseTransposeWorldViewMatrix", gameplay::RenderState::AutoBinding::INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX);

			/*If the lightNode have a Id, set the light with the material.*/
			if (scene->findNode(lightNode->getId()))
			{
				/*Third strange thing: This is not good that lights are attached to materials. At this
				moment I can only support one light, which I wanna find a workaround with to add more lights.*/
				material->getParameter("u_pointLightColor[0]")->bindValue(lightNode->getLight(), &gameplay::Light::getColor);
				material->getParameter("u_pointLightRangeInverse[0]")->bindValue(lightNode->getLight(), &gameplay::Light::getRangeInverse);
				material->getParameter("u_pointLightPosition[0]")->bindValue(lightNode, &gameplay::Node::getTranslationView);
			}

			/*Set the material's ambient color.*/
			material->getParameter("u_ambientColor")->setValue(gameplay::Vector3(materialHeader->ambient[0], materialHeader->ambient[1], materialHeader->ambient[2]));

			/*Set if there is a texture to the material.*/
			if (materialHeader->isTexture == true)
				material->getParameter("u_diffuseTexture")->setValue(colorTexture);

			/*Set the material's diffuse color, if there is no texture.*/
			else
				material->getParameter("u_diffuseColor")->setValue(gameplay::Vector4(materialHeader->diffuseColor[0], materialHeader->diffuseColor[1], materialHeader->diffuseColor[2], materialHeader->diffuseColor[3]));

			/*Set the material to the mesh model.*/
			meshModel->setMaterial(material);
		}
	}
}

void HMessageReader::fProcessLight(char* messageData, gameplay::Scene* scene)
{
	/*Read the hLightHeader from messageData.*/
	hLightHeader* lightHeader = (hLightHeader*)(messageData + sizeof(hMainHeader));

	lightHeader->lightName = messageData + sizeof(hMainHeader) + sizeof(hLightHeader);
	/*If the light node with the name ID  already exists in the scene, obtain it and update it's value.*/
	lightNode = scene->findNode(lightHeader->lightName);
	if (lightNode != NULL)
	{
		gameplay::Light* light = static_cast<gameplay::Light*>(lightNode->getLight());
		light->setColor(gameplay::Vector3(lightHeader->color[0], lightHeader->color[1], lightHeader->color[2]));
		lightNode->setLight(light);
	}
	/*If the light node is new, create a new one and add it to the scene.*/
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

	if (transH->childNameLength > 0)
	{
		gameplay::Node* nd = scene->findNode(transH->childName);

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

		/*The proj Matrix in openGL is slightly different, reason for negating 10 and 14
		in the matrix, is because maya uses a left hand coordinate system.*/
		cameraHeader->projMatrix[10] *= -1;
		cameraHeader->projMatrix[14] *= -1;
		cam->setProjectionMatrix(cameraHeader->projMatrix);

		/*Set the current existing camera as active.*/
		scene->setActiveCamera(cam);

		/*Update the transformation values.*/
		camNode->setScale(cameraHeader->scale);
		camNode->setTranslation(cameraHeader->trans);
		camNode->setRotation(camQuat);
	}
	/*If the camera is new, create a new camera node and it to the scene.*/
	else 
	{
		camNode = gameplay::Node::create(cameraHeader->cameraName);

		/*If the new camera have ortographic properties, it's proj matrix can be set 
		to a perspective camera created in Gameplay3D. */
		gameplay::Camera* cam = gameplay::Camera::createPerspective(0, 0, 0, 0);

		/*The proj Matrix in openGL is slightly different, reason for negating 10 and 14
		in the matrix, is because maya uses a left hand coordinate system.*/
		cameraHeader->projMatrix[10] *= -1;
		cameraHeader->projMatrix[14] *= -1;
		cam->setProjectionMatrix(cameraHeader->projMatrix);

		/*Set the new camera as active.*/
		camNode->setCamera(cam);
		scene->setActiveCamera(cam);

		/*Set the transformation values for the new camera.*/
		camNode->setScale(cameraHeader->scale);
		camNode->setRotation(camQuat);
		camNode->setTranslation(cameraHeader->trans);

		/*Add the new camera node to the scene.*/
		scene->addNode(camNode);
	}
}
