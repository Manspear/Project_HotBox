#include "HSceneViewer.h"
// Declare our game instance
HSceneViewer game;

HSceneViewer::HSceneViewer()
    : _scene(NULL), _wireframe(false)
{

}

HSceneViewer::~HSceneViewer()
{
	delete msgReader;
}

void HSceneViewer::initialize()
{
	/*By default create an empty scene, fill with the nodes we send from Maya.*/
	_scene = Scene::create();

	//msgReader = new HMessageReader();

	/*Initialize a default camera to see the mesh, later a camera from Maya will be loaded.*/
	Camera* camera = Camera::createPerspective(45.f, getAspectRatio(), 1.f, 40.f);
	
	Node* cameraNode = _scene->addNode("camera");

	cameraNode->setCamera(camera);

	_scene->setActiveCamera(camera);
	camera->release();

	cameraNode->translate(0, 1, 5);
	cameraNode->rotateX(MATH_DEG_TO_RAD(-11.25f));

	/*Initialize a light to give the scene light, later a light from Maya will be loaded.*/
	Light* light = Light::createPoint(Vector3(30.5f, 0.5f, 0.5f), 20);

	Node* lightbla = Node::create("lightNode");

	lightbla->setLight(light);

	lightbla->translate(Vector3(0, 0, 0));

	_scene->addNode(lightbla);

	lightbla->release();

	light->release();

	float size = 1.0f;

	float a = size * 0.5f;

	float vertices[] =
	{
		-a, -a,  a,    0.0,  0.0,  1.0,   0.0, 0.0,
		a, -a,  a,    0.0,  0.0,  1.0,   1.0, 0.0,
		-a,  a,  a,    0.0,  0.0,  1.0,   0.0, 1.0,
		a,  a,  a,    0.0,  0.0,  1.0,   1.0, 1.0,
		-a,  a,  a,    0.0,  1.0,  0.0,   0.0, 0.0,
		a,  a,  a,    0.0,  1.0,  0.0,   1.0, 0.0,
		-a,  a, -a,    0.0,  1.0,  0.0,   0.0, 1.0,
		a,  a, -a,    0.0,  1.0,  0.0,   1.0, 1.0,
		-a,  a, -a,    0.0,  0.0, -1.0,   0.0, 0.0,
		a,  a, -a,    0.0,  0.0, -1.0,   1.0, 0.0,
		-a, -a, -a,    0.0,  0.0, -1.0,   0.0, 1.0,
		a, -a, -a,    0.0,  0.0, -1.0,   1.0, 1.0,
		-a, -a, -a,    0.0, -1.0,  0.0,   0.0, 0.0,
		a, -a, -a,    0.0, -1.0,  0.0,   1.0, 0.0,
		-a, -a,  a,    0.0, -1.0,  0.0,   0.0, 1.0,
		a, -a,  a,    0.0, -1.0,  0.0,   1.0, 1.0,
		a, -a,  a,    1.0,  0.0,  0.0,   0.0, 0.0,
		a, -a, -a,    1.0,  0.0,  0.0,   1.0, 0.0,
		a,  a,  a,    1.0,  0.0,  0.0,   0.0, 1.0,
		a,  a, -a,    1.0,  0.0,  0.0,   1.0, 1.0,
		-a, -a, -a,   -1.0,  0.0,  0.0,   0.0, 0.0,
		-a, -a,  a,   -1.0,  0.0,  0.0,   1.0, 0.0,
		-a,  a, -a,   -1.0,  0.0,  0.0,   0.0, 1.0,
		-a,  a,  a,   -1.0,  0.0,  0.0,   1.0, 1.0
	};

	short indices[] =
	{
		0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7, 8, 9, 10, 10, 9, 11, 12, 13, 14, 14, 13, 15, 16, 17, 18, 18, 17, 19, 20, 21, 22, 22, 21, 23
	};

	unsigned int vertexCount = 24;
	unsigned int indexCount = 36;

	VertexFormat::Element elements[] =
	{
		VertexFormat::Element(VertexFormat::POSITION, 3),
		VertexFormat::Element(VertexFormat::NORMAL, 3),
		VertexFormat::Element(VertexFormat::TEXCOORD0, 2)
	};

	Mesh* mesh = Mesh::createMesh(VertexFormat(elements, 3), vertexCount, false);

	mesh->setVertexData(vertices, 0, vertexCount);

	MeshPart* meshPart = mesh->addPart(Mesh::TRIANGLES, Mesh::INDEX16, indexCount, false);

	meshPart->setIndexData(indices, 0, indexCount);

	Model* cubeModel = Model::create(mesh);

	mesh->release();

	Material* material = Material::create("res/shaders/colored.vert", "res/shaders/colored.frag", "POINT_LIGHT_COUNT 1");

	RenderState::StateBlock* block = RenderState::StateBlock::create();
	block->setCullFace(true);
	block->setDepthTest(true);
	material->setStateBlock(block);

	material->setParameterAutoBinding("u_worldViewMatrix", RenderState::AutoBinding::WORLD_VIEW_MATRIX);
	material->setParameterAutoBinding("u_worldViewProjectionMatrix", RenderState::AutoBinding::WORLD_VIEW_PROJECTION_MATRIX);
	material->setParameterAutoBinding("u_inverseTransposeWorldViewMatrix", RenderState::AutoBinding::INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX);

	Node* lightNode = _scene->findNode("lightNode");

	material->getParameter("u_pointLightColor[0]")->bindValue(lightNode->getLight(), &Light::getColor);
	material->getParameter("u_pointLightRangeInverse[0]")->bindValue(lightNode->getLight(), &Light::getRangeInverse);
	material->getParameter("u_pointLightPosition[0]")->bindValue(lightNode, &Node::getTranslationView);

	material->getParameter("u_ambientColor")->setValue(Vector3(1.0f, 1.0f, 1.0f));

	/*The input to the fragment shader have a vec4, and that's why it crashed.. when passing a Vector4.*/
	material->getParameter("u_diffuseColor")->setValue(Vector4(0.5f, 0.5f, 0.5f, 0.0f));

	Node* meshNode = Node::create("meshNode");

	meshNode->translate(0.f, 0.f, -2.f);

	cubeModel->setMaterial(material);

	meshNode->setDrawable(cubeModel);

	_scene->addNode(meshNode);

	cubeModel->release();
}

void HSceneViewer::finalize()
{
    SAFE_RELEASE(_scene);
}
/*
Through fRead we use fProcessNode, that in turn uses fProcessMesh, processLight etc
In processMesh meshData is stored in the different vectors called meshVertices, mesh

We need to be able to access any mesh, any vertexlist no matter where in the
vector they are.
For that we need to know:
which node was changed --> get it's name
what happened to that node --> have some bools
*/
void HSceneViewer::update(float elapsedTime)
{
	/*Get the information we send from the Maya plugin here.*/
	HMessageReader::MessageType messageType;

	//msgReader->fRead(msgReader->circBuff, messageType);

	//switch (messageType)
	//{
	//case HMessageReader::eDefault:
	//	printf("Hello! I am a default node.\n");

	//	break;

	//case HMessageReader::eNewMesh:
	//	/*Add new mesh to scene.*/
		fAddMesh();

	//	break;

	//case HMessageReader::eVertexChanged:
	//	/*Vertices changed in a mesh. Update the information.*/
	//	fModifyMesh();

	//	break;

	//case HMessageReader::eNewMaterial:
	//	/*A new material to add for a mesh in scene.*/
	//	fAddMaterial();

	//	break;

	//case HMessageReader::eMaterialChanged:
	//	/*A material is changed in a mesh. Update the information.*/
	//	fModifyMaterial();

	//	break;

	//case HMessageReader::eNewLight:
	//	/*A new light to add in the scene.*/
	//	fAddLight();

	//	break;

	//case HMessageReader::eNewTransform:
	//	/*A transform was added for any nodetype. Update the information.*/
	//	fAddTransform();

	//	break;

	//case HMessageReader::eNewCamera:
	//	/*A new camera to add in the scene.*/
	//	fAddCamera();

	//	break;

	//case HMessageReader::eCameraChanged:
	//	/*The camera is changed. Update the information.*/
	//	fModifyCamera();

	//	break;

	//case HMessageReader::eNodeRemoved:
	//	/*A node is removed from the scene. Update the scene.*/
	//	fRemoveNode();

	//	break;
	//}
}

void HSceneViewer::render(float elapsedTime)
{
    // Clear the color and depth buffers
	clear(CLEAR_COLOR_DEPTH, Vector4(1, 1, 1, 0), 1.0f, 0);

    // Visit all the nodes in the scene for drawing
    _scene->visit(this, &HSceneViewer::drawScene);
}

bool HSceneViewer::drawScene(Node* node)
{
    // If the node visited contains a drawable object, draw it
    Drawable* drawable = node->getDrawable(); 
    if (drawable)
        drawable->draw(_wireframe);

    return true;
}



void HSceneViewer::fAddMesh()
{
	/*
	Debug functionality:

	enter this function for any mesh-related change
	check if the mesh already exists in the scene
	if it already exists, modify that node without removing it
	if it doesn't exist, add that node
	*/

	bool meshAlreadyExists = false;

	std::vector<hVertexHeader> vertexList;
	unsigned int numVertices = 0;
	unsigned int* indexList = nullptr;
	unsigned int numIndex = 0;
	char* meshName = nullptr;
	meshName = new char[128];
	msgReader->fGetNewMesh(meshName, vertexList, numVertices, indexList, numIndex);

	//TEMP DEBUG
	numIndex = numVertices;

	indexList = new unsigned int[numVertices];
	for (int i = 0; i < numVertices; i++)
	{
		indexList[i] = i;
	}

	Node* meshNode = _scene->findNode(meshName);

	/*If the mesh already exists, handle it.*/
	if (meshNode)
	{
		meshAlreadyExists = true;

		//_scene->removeNode(meshNode);
	}

	/*Create a new mesh node with the name of mesh.*/
	else
	{
		meshNode = Node::create(meshName);

		VertexFormat::Element elements[] = {
			VertexFormat::Element(VertexFormat::POSITION, 3),
			VertexFormat::Element(VertexFormat::TEXCOORD0, 2),
			VertexFormat::Element(VertexFormat::NORMAL, 3),
		};

		const VertexFormat verticesFormat(elements, ARRAYSIZE(elements));

		/*Assign the vertices data to the new mesh, using an index of some sort.*/

		/*Create the mesh with the vertex format and number of vertices.*/
		Mesh* mesh = Mesh::createMesh(verticesFormat, numVertices, false);

		/*Set the vertex data for this mesh. HERE the vertex data should be used as argument.*/
		mesh->setVertexData(&vertexList[0], 0);

		/*How the mesh is to be constructed in the shader.*/
		MeshPart* meshPart = mesh->addPart(Mesh::PrimitiveType::TRIANGLES, Mesh::IndexFormat::INDEX32, numIndex, false);

		/*The index data for the mesh construction.*/
		meshPart->setIndexData(indexList, 0, numIndex);

		/*Create a new model which we set as Drawable for the node.*/
		Model* meshModel = Model::create(mesh);

		SAFE_RELEASE(mesh);

		Node* lightNode = _scene->findNode("pointLight");

		Material* material = meshModel->setMaterial("res/shaders/colored.vert", "res/shaders/colored.frag", "DIRECTIONAL_LIGHT_COUNT 1");

		material->setParameterAutoBinding("u_worldViewProjectionMatrix", "WORLD_VIEW_PROJECTION_MATRIX");
		material->setParameterAutoBinding("u_inverseTransposeWorldViewMatrix", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX");

		material->getParameter("u_ambientColor")->setValue(Vector3(1.f, 1.f, 1.f));

		material->getParameter("u_directionalLightColor[0]")->setValue(lightNode->getLight()->getColor());
		material->getParameter("u_directionalLightDirection[0]")->bindValue(lightNode, &Node::getForwardVectorWorld);

		meshNode->setTranslation(Vector3(0.f, 0.f, -3.f));

		meshNode->setDrawable(meshModel);

		/*Finally add this "MESHNODE" to the scene for rendering.*/
		_scene->addNode(meshNode);
	}

	delete[] meshName;
}

void HSceneViewer::fModifyMesh()
{
	//std::vector<hVertexHeader> vertexList;
	//unsigned int numVertices = 0;
	//unsigned int* indexList = nullptr;
	//unsigned int numIndex = 0;
	//char* meshName = nullptr;
	//meshName = new char[128];
	//msgReader->fGetNewMesh(meshName, vertexList, numVertices, indexList, numIndex);

	////TEMP DEBUG
	//numIndex = numVertices;

	//indexList = new unsigned int[numVertices];
	//for (int i = 0; i < numVertices; i++)
	//{
	//	indexList[i] = i;
	//}

	//Node* meshNode = _scene->findNode(meshName);

	//VertexFormat::Element elements[] = {
	//	VertexFormat::Element(VertexFormat::POSITION, 3),
	//	VertexFormat::Element(VertexFormat::TEXCOORD0, 2),
	//	VertexFormat::Element(VertexFormat::NORMAL, 3),
	//};
	//const VertexFormat verticesFormat(elements, ARRAYSIZE(elements));

	//Mesh* mesh = Mesh::createMesh(verticesFormat, numVertices, false);

	//mesh->setVertexData(&vertexList[0], 0);

	//MeshPart* meshPart = mesh->addPart(Mesh::PrimitiveType::TRIANGLES, Mesh::IndexFormat::INDEX32, numIndex, false);

	//meshPart->setIndexData(indexList, 0, numIndex);

	//Model* meshModel = Model::create(mesh);

	//SAFE_RELEASE(mesh);

	//Material* material = meshModel->setMaterial("res/shaders/colored.vert", "res/shaders/colored.frag", "DIRECTIONAL_LIGHT_COUNT 1");

	//material->setParameterAutoBinding("u_worldViewProjectionMatrix", "WORLD_VIEW_PROJECTION_MATRIX");
	//material->setParameterAutoBinding("u_inverseTransposeWorldViewMatrix", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX");

	//meshNode->setTranslation(Vector3(0.f, 0.f, -5.f));

	//meshNode->setDrawable(meshModel);

	///*Finally add this "MESHNODE" to the scene for rendering.*/
	//_scene->addNode(meshNode);


	//delete[] meshName;
}

void HSceneViewer::fAddCamera()
{
	char* camName = nullptr;
	camName = new char[128];
	float camProjMatrix[16];

	msgReader->fGetNewCamera(camName, camProjMatrix);

	bool isCameraNew = false;

	Node* cameraNode = _scene->findNode(camName);

	/*If the camera node don't exist in the scene, create a new node for it.*/
	if (!cameraNode)
	{
		cameraNode = Node::create(camName);
		_scene->addNode(cameraNode);
		isCameraNew = true;
	}

	Camera* cam = cameraNode->getCamera();

	/*If the camera do not exist under this node, create it.*/
	if (!cam)
	{
		/*If the camera is ortographic, it will create one also with the createPerspective() func.*/
		cam = Camera::createPerspective(0, 0, 0, 0);

		cameraNode->setCamera(cam);

		_scene->setActiveCamera(cam);

		/*Set the projection matrix for the current active camera.*/
		cam->setProjectionMatrix(camProjMatrix);

		SAFE_RELEASE(cam);

		cameraNode->translate(0, 1, 5);
		cameraNode->rotateX(MATH_DEG_TO_RAD(-11.25f));
	}
	
	delete camName;
}

void HSceneViewer::fModifyCamera()
{
}

void HSceneViewer::fAddMaterial()
{
}

void HSceneViewer::fModifyMaterial()
{
}

void HSceneViewer::fAddTransform()
{
}

void HSceneViewer::fAddLight()
{
}

void HSceneViewer::fModifyLight()
{
}

void HSceneViewer::fRemoveNode()
{
	/*
	Find the node name of the removed node.
	Delete the node with the same name as the found name.
	*/
	
}

void HSceneViewer::keyEvent(Keyboard::KeyEvent evt, int key)
{
    if (evt == Keyboard::KEY_PRESS)
    {
        switch (key)
        {
        case Keyboard::KEY_ESCAPE:
            exit();
            break;
        }
    }
}

void HSceneViewer::touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex)
{
    switch (evt)
    {
    case Touch::TOUCH_PRESS:
        _wireframe = !_wireframe;
        break;
    case Touch::TOUCH_RELEASE:
        break;
    case Touch::TOUCH_MOVE:
        break;
    };
}
