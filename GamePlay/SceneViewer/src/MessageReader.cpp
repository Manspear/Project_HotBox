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

void HMessageReader::fRead(circularBuffer& circBuff, gameplay::Scene* scene)
{
	char* msg = new char[maxSize];
	size_t length;
	if (circBuff.pop(msg, length))
	{
		fProcessMessage(msg, scene);
	}
	delete[] msg;
}

void HMessageReader::fProcessDeletedObject(char * messageData, gameplay::Scene* scene)
{
	hRemovedObjectHeader* remoh = (hRemovedObjectHeader*)messageData + sizeof(hMainHeader);

	hRemovedObjectHeader temp;
	temp.nodeType = remoh->nodeType;
	temp.name = new char[remoh->nameLength + 1];
	memcpy((char*)temp.name, remoh + sizeof(hRemovedObjectHeader), temp.nameLength);
	(char)temp.name[remoh->nameLength] = '\0';
	
	//removedList.push_back(temp);
	gameplay::Node* nd = scene->findNode(temp.name);
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
			fProcessLight(messageData, scene);
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

void HMessageReader::fProcessMesh(char* messageData, gameplay::Scene* scene)
{
	/*Read the meshHeader from messageData.*/
	hMeshHeader meshHeader = *(hMeshHeader*)(messageData + sizeof(hMainHeader));
	char* meshName;
	meshName = new char[meshHeader.meshNameLen + 1];
	memcpy(meshName, messageData + sizeof(hMainHeader) + sizeof(hMeshHeader), meshHeader.meshNameLen);
	meshName[meshHeader.meshNameLen] = '\0';

	char* prntTransName;
	prntTransName = new char[meshHeader.prntTransNameLen + 1];
	memcpy(prntTransName, messageData + sizeof(hMainHeader) + sizeof(hMeshHeader) + meshHeader.meshNameLen, meshHeader.prntTransNameLen);
	prntTransName[meshHeader.prntTransNameLen] = '\0';

	hMeshVertex vertList;
	vertList.vertexList.resize(meshHeader.vertexCount);
	memcpy(&vertList.vertexList[0], messageData + sizeof(hMainHeader) +
		sizeof(hMeshHeader) + meshHeader.meshNameLen + meshHeader.prntTransNameLen,
		meshHeader.vertexCount * sizeof(sBuiltVertex));

	gameplay::Node* nd = scene->findNode(meshName);

	if (nd != NULL)
	{
		gameplay::Model* model = static_cast<gameplay::Model*>(nd->getDrawable());
		model->getMesh()->setVertexData(&vertList.vertexList[0], 0, vertList.vertexList.size());
	}
	else
	{
		/*The node is new*/

		nd = gameplay::Node::create(meshName);

		gameplay::VertexFormat::Element elements[] = {
			gameplay::VertexFormat::Element(gameplay::VertexFormat::POSITION, 3),
			gameplay::VertexFormat::Element(gameplay::VertexFormat::TEXCOORD0, 2),
			gameplay::VertexFormat::Element(gameplay::VertexFormat::NORMAL, 3)
		};
		const gameplay::VertexFormat vertFormat(elements, ARRAYSIZE(elements));

		gameplay::Mesh* mesh = gameplay::Mesh::createMesh(vertFormat, meshHeader.vertexCount, true);
		mesh->setVertexData(&vertList.vertexList[0], 0, meshHeader.vertexCount);

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
}

void HMessageReader::fProcessMaterial(char* messageData, gameplay::Scene* scene)
{
	/*Fill the materiallist vector for this process.*/
}

void HMessageReader::fProcessLight(char* messageData, gameplay::Scene* scene)
{
	/*Fill the lightlist vector for this process.*/
}

void HMessageReader::fProcessTransform(char* messageData, gameplay::Scene* scene)
{
	/*Fill the transformlist vector for this process.*/
}

void HMessageReader::fProcessCamera(char* messageData, gameplay::Scene* scene)
{
	/*Read the hCameraHeader from messageData.*/
	hCameraHeader cameraHeader = *(hCameraHeader*)(messageData + sizeof(hMainHeader));

	char* cameraName = new char[cameraHeader.cameraNameLength + 1];
	memcpy(cameraName, messageData + sizeof(hMainHeader) + sizeof(hCameraHeader), cameraHeader.cameraNameLength);
	cameraName[cameraHeader.cameraNameLength] = '\0';

	gameplay::Node* camNode = scene->findNode(cameraName);
	gameplay::Quaternion camQuat = cameraHeader.rot;

	if (camNode != NULL)
	{
		gameplay::Camera* cam = static_cast<gameplay::Camera*>(camNode->getCamera());
		cam->setProjectionMatrix(cameraHeader.projMatrix);

		camNode->setTranslation(cameraHeader.trans);
		camNode->setScale(cameraHeader.scale);
		camNode->setRotation(camQuat);
	}

	else 
	{
		camNode = gameplay::Node::create(cameraName);

		gameplay::Camera* cam = gameplay::Camera::createPerspective(0, 0, 0, 0);
		cam->setProjectionMatrix(cameraHeader.projMatrix);

		camNode->setCamera(cam);
		scene->setActiveCamera(cam);

		camNode->setTranslation(cameraHeader.trans);
		camNode->setScale(cameraHeader.scale);
		camNode->setRotation(camQuat);

		scene->addNode(camNode);
	}
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


