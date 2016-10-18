#include "hDebugTImer.h"

void DebugTimer::start()
{
	double clockTicks = clock();
	startTime = clockTicks / CLOCKS_PER_SEC;
}

void DebugTimer::stop(double& deltaTime)
{
	double clockTicks = clock();
	double endTime = clockTicks / CLOCKS_PER_SEC;

	deltaTime = startTime - endTime;
}
