#include "gameplay.h"
#ifndef HSceneViewer_H_
#define HSceneViewer_H_

#include "MessageReader.h"

using namespace gameplay;

class HSceneViewer: public Game
{
public:
	HSceneViewer();

	~HSceneViewer();

	Scene* _scene;

	void keyEvent(Keyboard::KeyEvent evt, int key);
	
    void touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex);

	HMessageReader* msgReader;

	size_t bufferSize, maxSize;

	int delayTime, numMessages, chunkSize;

protected:

    void initialize();

    void finalize();

    void update(float elapsedTime);

    void render(float elapsedTime);

private:

	/*Draws the scene each frame.*/
    bool drawScene(Node* node);

    bool _wireframe;
};

#endif
