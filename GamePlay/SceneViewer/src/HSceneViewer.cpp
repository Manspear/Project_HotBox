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

void InitDosConsole() {
	AllocConsole();
	freopen("CONIN$", "rb", stdin);
	freopen("CONOUT$", "wb", stdout);
	freopen("CONOUT$", "wb", stderr);
}

void HSceneViewer::initialize()
{
	InitDosConsole();

	/*By default create an empty scene, fill with the nodes we send from Maya.*/
	_scene = Scene::create();

	msgReader = new HMessageReader();

	setVsync(false);
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
	static float timer = 0;
	
	/*Get the message information we send from the Maya plugin here.*/
	msgReader->fRead(msgReader->circBuff, _scene);

	/*
	Processing the queues of objects not found in the scene to check if they're in the scene
	This function is not called that often. This function MUST be placed after the fRead function.
	*/
	msgReader->fProcessQueues(_scene);
}

void HSceneViewer::render(float elapsedTime)
{
    // Clear the color and depth buffers
	clear(CLEAR_COLOR_DEPTH, Vector4(0.01f, 0.01f, 0.01f, 0), 1.0f, 0);

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
