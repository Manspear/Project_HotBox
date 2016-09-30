#pragma once
#include <maya/MArgList.h>
#include <maya/MObject.h>
#include <maya/MGlobal.h>
#include <maya/MPxCommand.h>

class HelloWorld : public MPxCommand
{
public:
	HelloWorld(){};
	virtual MStatus doIt(const MArgList&);
	static void* creator();
};