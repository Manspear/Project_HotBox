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

	msgReader = new HMessageReader();

	/*Initialize a default camera to see the mesh, later a camera from Maya will be loaded.*/
	Node* cameraNode = Node::create();
	_scene->addNode(cameraNode);
	Camera* cam = cameraNode->getCamera();
	cam = Camera::createPerspective(0, 0, 0, 0);
	cameraNode->setCamera(cam);
	_scene->setActiveCamera(cam);
	cameraNode->setTranslation(Vector3(0, 0, -5));
	cameraNode->release();
	cam->release();

	/*Initialize a default point light to give the scene light, later a light from Maya will be loaded.*/
	Node* lightNode = Node::create("pointLight");
	Light* light = Light::createPoint(Vector3(0.5f, 0.5f, 0.5f), 20);
	lightNode->setLight(light);
	lightNode->translate(Vector3(2.f, 2.f, 2.f));
	_scene->addNode(lightNode);
	lightNode->release();
	light->release();
}

void HSceneViewer::finalize()
{
    SAFE_RELEASE(_scene);
}

void HSceneViewer::update(float elapsedTime)
{
	/*Get the information we send from the Maya plugin here.*/
	HMessageReader::MessageType messageType;

	msgReader->read(msgReader->circBuff, enumList);

	for (int enumIndex = 0; enumIndex < enumList.size(); enumIndex++)
	{
		switch (enumIndex)
		{
		case HMessageReader::eDefault:
			printf("Hello! I am a default node.\n");

		case HMessageReader::eNewMesh:
			/*Add new mesh to scene.*/
			addMesh();

			break;

		case HMessageReader::eVertexChanged:
			/*Vertices changed in a mesh. Update the information.*/
			modifyMesh();

			break;

		case HMessageReader::eNewMaterial:
			/*A new material to add for a mesh in scene.*/
			addMaterial();

			break;

		case HMessageReader::eMaterialChanged:
			/*A material is changed in a mesh. Update the information.*/
			modifyMaterial();

			break;

		case HMessageReader::eNewLight:
			/*A new light to add in the scene.*/
			addLight();

			break;

		case HMessageReader::eNewTransform:
			/*A transform was added for any nodetype. Update the information.*/
			addTransform();

			break;

		case HMessageReader::eNewCamera:
			/*A new camera to add in the scene.*/
			addCamera();

			break;

		case HMessageReader::eCameraChanged:
			/*The camera is changed. Update the information.*/
			modifyCamera();

			break;

		case HMessageReader::eNodeRemoved:
			/*A node is removed from the scene. Update the scene.*/
			removeNode();

			break;
		}
	}


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

void HSceneViewer::addMesh()
{
	bool meshAlreadyExists = false;

	std::vector<hVertexHeader> vertexList;
	unsigned int numVertices = 0;
	unsigned int* indexList = nullptr;
	unsigned int numIndex = 36;
	char* meshName = nullptr;
	meshName = new char[128];
	msgReader->getNewMesh(meshName, vertexList, numVertices, indexList, numIndex);

	indexList = new unsigned int[36];
	for (int i = 0; i < 36; i++)
	{
		indexList[i] = i;
	}

	Node* meshNode = _scene->findNode(meshName);

	/*If the mesh already exists, handle it.*/
	if (meshNode)
	{
		meshAlreadyExists = true;
		_scene->removeNode(meshNode);
	}

	/*Create a new mesh node with the name of mesh.*/
	else
	{
		meshNode = Node::create(meshName);
	}

	delete[] meshName;

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

	/*Finally add this "MESHNODE" to the scene for rendering.*/
	_scene->addNode(meshNode);
}

void HSceneViewer::modifyMesh()
{
}

void HSceneViewer::addCamera()
{
}

void HSceneViewer::modifyCamera()
{
}

void HSceneViewer::addMaterial()
{
}

void HSceneViewer::modifyMaterial()
{
}

void HSceneViewer::addTransform()
{
}

void HSceneViewer::addLight()
{
}

void HSceneViewer::modifyLight()
{
}

void HSceneViewer::removeNode()
{
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
