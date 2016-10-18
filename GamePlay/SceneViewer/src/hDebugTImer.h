#pragma once
#include <time.h>
/*
This is a timer used to check the delta time between timer.start()
and timer.reset()
*/
class DebugTimer
{
private:
	double dt;
	double startTime;
public:
	void start();
	void stop(double& deltaTime);
};