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

	lightNode->setLight(light);

	lightNode->setLight(light);
	lightNode->translate(Vector3(0, 0, 0));
	_scene->addNode(lightNode);
	lightNode->release();

	/*float size = 1.0f;

	_scene->addNode(lightbla);

	lightbla->release();

	light->release();

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
	

	/*The input to the fragment shader have a vec4, and that's why it crashed.. when passing a Vector4.*/
	material->getParameter("u_diffuseColor")->setValue(Vector4(0.5f, 0.5f, 0.5f, 0.0f));

	Node* meshNode = Node::create("meshNode");

	meshNode->translate(0.f, 0.f, -2.f);

	cubeModel->setMaterial(material);

	meshNode->setDrawable(cubeModel);

	_scene->addNode(meshNode);

	cubeModel->release();*/
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
	//HMessageReader::MessageType messageType;
	HMessageReader::sFoundInfo nfo;
	msgReader->fRead(msgReader->circBuff, messageType);

	/*
	fRead calls functions that...
	Why don't we just create the nodes directly
	from the data read from the message?
	like:
	"read main header"
	"see what kind of item is in the message"
	"make that node in the 
	*/
	msgReader->fRead(msgReader->circBuff, _scene);
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


		Node* lightNode = _scene->findNode("pointLight");
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
