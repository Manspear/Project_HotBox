// UD1414_Plugin.cpp : Defines the exported functions for the DLL application.

#include "MayaIncludes.h"
#include "HelloWorldCmd.h"
#include <iostream>
using namespace std;

MCallbackIdArray ids;
/*e for enum*/
enum eMyNodeType
{
    mesh,
    transform,
    dagNode,
    notHandled
};

void* HelloWorld::creator() { 
	return new HelloWorld;
};
MStatus HelloWorld::doIt(const MArgList& argList)
{
	MGlobal::displayInfo("Hello World!");
	return MS::kSuccess;
};
/*
Only seems to trigger when new vertices and/or edge loops are added.
*/
void onMeshTopoChange(MObject &node, void *clientData)
{
	MGlobal::displayInfo("TOPOLOGY!");
}

void onNodeAttrChange(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
    /*
    This will be a vertex.
    */
	if (msg & MNodeMessage::AttributeMessage::kAttributeSet && !plug.isArray() && plug.isElement())
	{
		MStatus res;
		MObject obj = plug.node(&res);; //this by itself returns kDoubleLinearAttribute
		if (res == MStatus::kSuccess)
		{
			//MGlobal::displayInfo("Success!");
			MFnTransform transFn(obj, &res);
			if (res == MStatus::kSuccess)
			{
                plug.name();
				MGlobal::displayInfo("Transform node: " + transFn.name() + " Attribute changed: " + plug.name());
			}
			else
			{
				MFnMesh meshFn(obj, &res);
				if (res == MStatus::kSuccess)
				{
					MPoint aPoint;
                    MFnAttribute fnAttr(plug.attribute());
                    
                    fnAttr.type();
					res = meshFn.getPoint(plug.logicalIndex(), aPoint, MSpace::kObject);
					if (res == MStatus::kSuccess)
					{
						MGlobal::displayInfo(MString("Mesh node: ") + meshFn.name() + MString(" new vertex set: ") + plug.name() + " " + aPoint.x + " " +  aPoint.y + " " + aPoint.z);
					}
				}
				else
				{
					MFnDagNode dagFn(obj, &res);
					if (res == MStatus::kSuccess)
					{
						MGlobal::displayInfo("Node type: " + dagFn.type() + MString(" node name: ") + dagFn.name() + " ATTR SET!");
					}
				}
			}
		}
	}
    else if (msg & MNodeMessage::AttributeMessage::kAttributeSet)
    {
        //MStatus result;
        //MObject objecto = plug.attribute(&result);
        //MGlobal::displayInfo(MString("MOOOOOOOOOOOOO ") + plug.name() + MString(" ") + objecto.apiTypeStr());

        MObject obj = plug.node();
        MStatus res;
        if (!plug.isArray())
        {
            if (obj.hasFn(MFn::kTransform))
            {
                MFnTransform fnTra(obj, &res);
                if (res == MStatus::kSuccess)
                {
                    MFnAttribute fnAtt(plug.attribute(), &res);
                    if (res == MStatus::kSuccess)
                    {
                        MGlobal::displayInfo("Transform node: " + fnTra.name() + " Attribute: " + fnAtt.name());
                    }                
                }
            }
            
            //MGlobal::displayInfo("Non array: " + plug.name() + " Belonging to: " + obj.apiTypeStr());
        } 
    }
}

void onNodeAttrAddedRemoved(MNodeMessage::AttributeMessage msg, MPlug &plug, void *clientData)
{
	MGlobal::displayInfo("Attribute Added or Removed! " + plug.info());
}

void onComponentChange(MUintArray componentIds[], unsigned int count, void *clientData)
{
    MGlobal::displayInfo("I AM CHANGED!");
}

void meshAddCbks(MObject& node, void* clientData)
{
    MStatus res;
    MCallbackId id;
    MFnMesh meshFn(node, &res);
    if (res == MStatus::kSuccess)
    {
        id = MNodeMessage::addAttributeChangedCallback(node, onNodeAttrChange, NULL, &res);
        if (res == MStatus::kSuccess)
            ids.append(id);
        id = MNodeMessage::addAttributeAddedOrRemovedCallback(node, onNodeAttrAddedRemoved, NULL, &res);
        if (res == MStatus::kSuccess)
            ids.append(id);

        id = MPolyMessage::addPolyTopologyChangedCallback(node, onMeshTopoChange, NULL, &res);
        if (res == MStatus::kSuccess)
        {
            ids.append(id);
        }
        bool arr[4]{ true, false, false, false };
        id = MPolyMessage::addPolyComponentIdChangedCallback(node, arr, 4, onComponentChange, NULL, &res);
        if (res == MStatus::kSuccess)
        {
            ids.append(id);
        }
    }
}

void transAddCbks(MObject& node, void* clientData)
{
    MStatus res;
    MCallbackId id;
    MFnTransform transFn(node, &res);
    if (res == MStatus::kSuccess)
        id = MNodeMessage::addAttributeChangedCallback(node, onNodeAttrChange, NULL, &res);
    if (res == MStatus::kSuccess)
    {
        ids.append(id);
    }
 
}

void dagNodeAddCbks(MObject& node, void* clientData)
{
    MStatus res;
    MFnDagNode dagFn(node, &res);
    if(res == MStatus::kSuccess)
    {
        MCallbackId id = MNodeMessage::addAttributeChangedCallback(node, onNodeAttrChange, NULL, &res);
        if (res == MStatus::kSuccess)
            ids.append(id);
        id = MNodeMessage::addAttributeAddedOrRemovedCallback(node, onNodeAttrAddedRemoved, NULL, &res);
        if (res == MStatus::kSuccess)
            ids.append(id);
    }    
}

void onNodeCreate(MObject& node, void *clientData)
{
    eMyNodeType nt = eMyNodeType::notHandled;

    MGlobal::displayInfo("GOT INTO NODECREATE!");
    if (node.hasFn(MFn::kMesh))
    {
        nt = eMyNodeType::mesh; MGlobal::displayInfo("MESH!");
    }
    else if (node.hasFn(MFn::kTransform))
    {
        nt = eMyNodeType::transform; MGlobal::displayInfo("TRANSFORM!");
    }
    else if (node.hasFn(MFn::kDagNode))
    {
        nt = eMyNodeType::dagNode; MGlobal::displayInfo("NODE!");
    }
    /*Test code for finding UVs...*/
    //MStatus res;
    //MCallbackId id = MNodeMessage::addAttributeChangedCallback(node, onNodeAttrChange, NULL, &res);
    //if (res == MStatus::kSuccess)
    //    ids.append(id);
    switch (nt)
    {
        case(eMyNodeType::mesh):
        {
            meshAddCbks(node, clientData);
            break;
        }
        case(eMyNodeType::transform):
        {
            transAddCbks(node, clientData);
            break;
        }
        case(eMyNodeType::dagNode):
        {
            dagNodeAddCbks(node, clientData);
            break;
        }
        case(eMyNodeType::notHandled):
        {
            break;
        }
    }

    //MStatus res;
    ////MFnMesh mesh(node, &res);
    //MFnMesh meshFn(node, &res);
    //if (res == MStatus::kSuccess)
    //{
    //    MGlobal::displayInfo("CREATE: Mesh name: " + meshFn.name());

    //    MCallbackId id = MNodeMessage::addAttributeChangedCallback(node, onNodeAttrChange, NULL, &res);
    //    if (res == MStatus::kSuccess)
    //        ids.append(id);
    //    id = MNodeMessage::addAttributeAddedOrRemovedCallback(node, onNodeAttrAddedRemoved, NULL, &res);
    //    if (res == MStatus::kSuccess)
    //        ids.append(id);
    //}
    //else
    //{
    //    MFnDagNode dagFn(node, &res);
    //    if (res = MStatus::kSuccess)
    //    {
    //        MGlobal::displayInfo("CREATE: Node name: " + dagFn.name() + " Node type: " + node.apiTypeStr());

    //        MCallbackId id = MNodeMessage::addAttributeChangedCallback(node, onNodeAttrChange, NULL, &res);
    //        if (res == MStatus::kSuccess)
    //            ids.append(id);
    //        id = MNodeMessage::addAttributeAddedOrRemovedCallback(node, onNodeAttrAddedRemoved, NULL, &res);
    //        if (res == MStatus::kSuccess)
    //            ids.append(id);
    //    }
    //}

    //if (node.hasFn(MFn::kMesh))
    //{
    //    MCallbackId id = MPolyMessage::addPolyTopologyChangedCallback(node, onMeshTopoChange, NULL, &res);
    //    if (res == MStatus::kSuccess)
    //    {
    //        ids.append(id);
    //        MGlobal::displayInfo("HEYEYE");
    //    }
    //}

    //if (node.hasFn(MFn::kTransform))
    //{
    //    MGlobal::displayInfo("Node transform: " + MFnTransform(node).name());

    //    MCallbackId id = MNodeMessage::addAttributeChangedCallback(node, onNodeAttrChange, NULL, &res);
    //    if (res == MStatus::kSuccess)
    //    {
    //        ids.append(id);
    //    }
    //}
}

void onNodeNameChange(MObject &node, const MString &str, void *clientData)
{
    MStatus res = MStatus::kSuccess;

    MFnTransform transFn(node, &res);
    
    node.hasFn(MFn::kTranslateManip);
    if (node.hasFn(MFn::kTranslateManip) || node.hasFn(MFn::kScaleManip) || node.hasFn(MFn::kRotateManip) ||
        node.hasFn(MFn::kUVManip2D) || node.hasFn(MFn::kFreePointManip) || node.hasFn(MFn::kScaleUVManip2D) ||
        node.hasFn(MFn::kPolyCaddyManip))
    {
        /* Manipulators are dumb so we ignore them.*/
    }
    else
    {
        if (res == MStatus::kSuccess)
        {
            MGlobal::displayInfo("NEW NAME: New transform node name: " + transFn.name() + " Node Type: " + node.apiTypeStr());
        }
        else
        {
            MFnMesh meshFn(node, &res);

            if (res == MStatus::kSuccess)
            {
                MGlobal::displayInfo("NEW NAME: New mesh node name: " + meshFn.name() + " Node Type: " + node.apiTypeStr());
            }
            else
            {
                MFnDagNode dagFn(node, &res);
                if (res == MStatus::kSuccess)
                {
                    //MFnDagNode dagFn(node);
                    MGlobal::displayInfo("NEW NAME: New node name: " + dagFn.name() + " Node Type: " + node.apiTypeStr());
                }
            }
        }
    }
    
}

void onElapsedTime(float elapsedTime, float lastTime, void *clientData)
{
    static float totTime = 0;
    totTime += elapsedTime;
    MGlobal::displayInfo("Time since last time: " + MString() + elapsedTime + " Elapsed Time: " + MString() + totTime );
}

void iterateScene()
{
    MStatus res;
    MItDag nodeIt(MItDag::TraversalType::kBreadthFirst, MFn::Type::kDagNode, &res);
    
    if (res == MStatus::kSuccess)
    {
        while (!nodeIt.isDone())
        {
            onNodeCreate(nodeIt.currentItem(), NULL);
            nodeIt.next();
        }
    }
    //for (;!meshIt.isDone(); meshIt.next())
    //{
    //    MFnMesh myMeshFn(meshIt.currentItem());

    //}
}

// called when the plugin is loaded
EXPORT MStatus initializePlugin(MObject obj)
{
	// most functions will use this variable to indicate for errors
	MStatus res = MS::kSuccess;

	MFnPlugin myPlugin(obj, "Maya plugin", "1.0", "Any", &res);
	if (MFAIL(res)) {
		CHECK_MSTATUS(res);
	}
	MStatus status = myPlugin.registerCommand("helloWorld", HelloWorld::creator);
	CHECK_MSTATUS_AND_RETURN_IT(status);
    
	MGlobal::displayInfo("Maya plugin loaded!");
	// if res == kSuccess then the plugin has been loaded,
	// otherwise it has not.
   
	MCallbackId temp;
	temp = MDGMessage::addNodeAddedCallback(onNodeCreate,
		kDefaultNodeType,
		NULL,
		&res
	);
    if (res == MS::kSuccess)
    {
        MGlobal::displayInfo("nodeAdded success!");
        ids.append(temp);
    }
    temp = MNodeMessage::addNameChangedCallback(MObject::kNullObj, onNodeNameChange, NULL, &res);
    if (res == MStatus::kSuccess)
    {
        MGlobal::displayInfo("nameChange success!");
        ids.append(temp);
    }

    temp = MTimerMessage::addTimerCallback(5, onElapsedTime, NULL, &res);
    if (res == MStatus::kSuccess)
    {
        MGlobal::displayInfo("timerFunc success!");
        ids.append(temp);
    }
    iterateScene();

	return res;
}


EXPORT MStatus uninitializePlugin(MObject obj)
{
	// simply initialize the Function set with the MObject that represents
	// our plugin
	MFnPlugin plugin(obj);

	// if any resources have been allocated, release and free here before
	// returning...

	MMessage::removeCallbacks(ids);

	MStatus status = plugin.deregisterCommand("helloWorld");
	CHECK_MSTATUS_AND_RETURN_IT(status);
	MGlobal::displayInfo("Maya plugin unloaded!");

	return MS::kSuccess;
}

//int awesomeFunction()
//{
//	return 69;
//}