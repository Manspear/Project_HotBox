#ifndef HSceneViewer_H_
#define HSceneViewer_H_

#include "gameplay.h"
#include "MessageReader.h"

using namespace gameplay;

class HSceneViewer: public Game
{
public:
	HSceneViewer();

	~HSceneViewer();

	void keyEvent(Keyboard::KeyEvent evt, int key);
	
    void touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex);

	std::vector<HMessageReader::MessageType> enumList;

	HMessageReader* msgReader;

	size_t bufferSize;

	int delayTime;

	int numMessages;

	int chunkSize;

	size_t maxSize;

protected:

    void initialize();

    void finalize();

    void update(float elapsedTime);

    void render(float elapsedTime);

private:

	HMessageReader::MessageType msgType;

	/*Draws the scene each frame.*/
    bool drawScene(Node* node);

	void addMesh();
	void modifyMesh();

	void addCamera();
	void modifyCamera();

	void addMaterial();
	void modifyMaterial();

	void addTransform();

	void addLight();
	void modifyLight();

	void removeNode();

    Scene* _scene;
    bool _wireframe;
};

#endif
