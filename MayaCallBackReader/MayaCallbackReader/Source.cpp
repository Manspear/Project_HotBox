// UD1414_Plugin.cpp : Defines the exported functions for the DLL application.

#include "MayaIncludes.h"
#include "HelloWorldCmd.h"
#include <iostream>
#include <vector>
#include <map>
#include <queue>
using namespace std;

MCallbackIdArray ids;
std::queue<MObject> queueList;

/*e for enum*/
enum eNodeType
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

void fLoadMesh(MFnMesh& mesh)
{
    struct sPoint
    {
        float x, y, z;
    };
    struct sUV
    {
        float u, v;
    };
    struct sNormal
    {
        float x, y, z;
    };

    std::vector<sUV> uv;
    std::vector<sPoint> pnt;
    std::vector<sNormal> nor;

    MIntArray normalCnts;
    MIntArray normalIDs;
    mesh.getNormalIds(normalCnts, normalIDs);

    /*FUCK INDEXING! LET'S DO IT THE SHITTY WAY!!!*/
    MStatus res;
    MIntArray triCnt;
    MIntArray triVert; //vertex Ids for each tri vertex

    struct sAllVertex
    {
        sUV uv;
        sPoint pnt;
        sNormal nor;
    };

    const float * rwPnts = mesh.getRawPoints(&res);
    const float * rwNrmls = mesh.getRawNormals(&res);
    //Use this to get the normal of this vertex.

    MStringArray uvSetNames;
    mesh.getUVSetNames(uvSetNames);

    //"Vilken normal tillhör vilken vertis?" --> getTriangleOffsets()

    std::vector<sAllVertex> allVert;
    std::vector<MPoint> pntArr;
    mesh.getTriangles(triCnt, triVert);
    allVert.resize(triVert.length());
    for (int i = 0; i < allVert.size(); i++)
    {
        sAllVertex tempAll;
        MVector tempVec;
        MPoint tempPoint;
        mesh.getVertexNormal(triVert[i], tempVec, MSpace::kObject);  //<-- These used!
        mesh.getPoint(triVert[i], tempPoint, MSpace::kObject);

        tempAll.pnt.x = tempPoint.x;
        tempAll.pnt.y = tempPoint.y;
        tempAll.pnt.z = tempPoint.z;

        tempAll.nor.x = tempVec.x;
        tempAll.nor.y = tempVec.y;
        tempAll.nor.z = tempVec.z;

        allVert[i] = tempAll;
        
        pntArr.push_back(tempPoint);
    }
    //float u;
    //float v;
    MFloatArray u;
    MFloatArray v;
    int polyOffset = 0;
    for (int i = 0; i < mesh.numPolygons(); i++)
    {
        int vertCnt = mesh.polygonVertexCount(i, &res);
        if (res == MStatus::kSuccess)
        {
            for (int j = 0; j < vertCnt; j++)
            {
                float2 uv;
                MString* uvSetName = &uvSetNames[0];
                //With this function Maya loops "forever"
                int jiejie;
                mesh.getUVAtPoint(pntArr[polyOffset], uv, MSpace::kObject, uvSetName, &jiejie);
                mesh.getUVs(u, v, uvSetName);
                //For each vertex, for each polygon. Meaning (for a cube) 8 * 3 = 24 UVs

                //mesh.getPolygonUV(i, j, u, v, uvSetName);
                
                //Add UVs linearly..? Like 0, 1, 2, 3...
                //polyOffset reaches 36... Somehow
                
                int meimei = j;
                allVert[polyOffset].uv.u = uv[0];
                allVert[polyOffset].uv.v = uv[1];
                MGlobal::displayInfo(MString("Polyoffset: ") + polyOffset + MString(" closestPolygon: ") + jiejie);
                MGlobal::displayInfo(MString("PolyOffset: ") + polyOffset + MString(" Setname: ") + *uvSetName + MString(" u: ") + uv[0] + MString(" v ") + uv[1]);
                polyOffset += 1;
            }
        } 
    }

    /*
    Bestäm kortaste avståndet mellan planet X och linje L. De är parallella. 
    Ta en punkt på linjen. Ta en punkt på planet. Dra en vektor mellan dem. 
    Du vet att planet och linjen är parallella. 
    Ta två linjer, beräkna deras kryssprodukt.
    Någon multipel av resultatet av kryssprodukten ligger i planet.
    När du har hittat rätt multipel, vanlig trigonometri.

    Elternativt:
    När vektor mellan punkt på linje, och punkt på plan är beräknad (ej normaliserad)
    Projicera den vektorn på planet. 
    */

    MFloatArray uArr;
    MFloatArray vArr;
    mesh.getUVs(uArr, vArr, &uvSetNames[0]);
    for (int i = 0; i < allVert.size(); i++)
    {
        /*
        Alright. NumUvs never change even if the mesh is triangulated...
        Which means that there exists "UV control points"
        Which means, in turn, that you need a "UV index array" to get values from those UV control points.
        So how do you get a UV index array?
        */
        MGlobal::displayInfo(MString("NumUVs: ") + uArr.length() + MString(" ") + vArr.length());
        MGlobal::displayInfo(MString("Allvert.Size: ") + allVert.size() + MString(" Pos: ") + allVert[i].pnt.x + MString(" ") + allVert[i].pnt.y + MString(" ") + allVert[i].pnt.z + MString(" UV: ") + allVert[i].uv.u + MString(" ") + allVert[i].uv.v + MString(" Normal: ") + allVert[i].nor.x + MString(" ") + allVert[i].nor.y + MString(" ") + allVert[i].nor.z);
    }
}

/*
Only seems to trigger when new vertices and/or edge loops are added.
*/
void fOnMeshTopoChange(MObject &node, void *clientData)
{
	MGlobal::displayInfo("TOPOLOGY!");
    MStatus res;
    MFnMesh meshFn(node, &res);
    if (res == MStatus::kSuccess)
    {
        //loadMesh // reloadMesh
    }
}

void fOnMeshAttrChange(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
    if (msg & MNodeMessage::AttributeMessage::kAttributeSet)
    {
        MStatus res;
        MObject temp = plug.node();
        MFnMesh meshFn(temp, &res);
        if (res == MStatus::kSuccess)
        {
            /*When a mesh point gets changed*/
            if ((MFnMesh(plug.node()).findPlug("pnts") == plug) && plug.isElement())
            {
                MPoint aPoint;
                res = meshFn.getPoint(plug.logicalIndex(), aPoint, MSpace::kObject);
                if (res == MStatus::kSuccess)
                    MGlobal::displayInfo("Point moved: " + MString() + aPoint.x + " " + aPoint.y + " " + aPoint.z);
            }
            fLoadMesh(meshFn);
        }
    }
}

void fOnTransformAttrChange(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
    if (msg & MNodeMessage::AttributeMessage::kAttributeSet)
    {
        MObject obj = plug.node();
        MStatus res;
        if (!plug.isArray())
        {
            if (obj.hasFn(MFn::kTransform))
            {
                MFnTransform fnTra(obj, &res);
                if (res == MStatus::kSuccess)
                {
                    MTransformationMatrix transMat = fnTra.transformation();
                    MTransformationMatrix::RotationOrder rotOrder;
                    double rot[3];
                    double scale[3];
                    MVector trans = transMat.getTranslation(MSpace::kWorld);
                    transMat.getRotation(rot, rotOrder);
                    transMat.getScale(scale, MSpace::kObject);

                    MFnAttribute fnAtt(plug.attribute(), &res);
                    if (res == MStatus::kSuccess)
                    {
                        MGlobal::displayInfo("Transform node: " + fnTra.name() + " Trans: " + trans.x + " " + trans.y + " " + trans.z + " Rot: " + rot[0] + " " + rot[1] + " " + rot[2] + " Scale: " + scale[0] + " " + scale[1] + " " + scale[2]);
                    }
                }
            }
        }
    }
}

void fOnNodeAttrChange(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	if (msg & MNodeMessage::AttributeMessage::kAttributeSet && !plug.isArray() && plug.isElement())
	{
		MStatus res;
		MObject obj = plug.node(&res);
		if (res == MStatus::kSuccess)
		{
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
        } 
    }
}

void fOnNodeAttrAddedRemoved(MNodeMessage::AttributeMessage msg, MPlug &plug, void *clientData)
{
	MGlobal::displayInfo("Attribute Added or Removed! " + plug.info());
}

void fOnComponentChange(MUintArray componentIds[], unsigned int count, void *clientData)
{
    MGlobal::displayInfo("I AM CHANGED!");
}

void fMeshAddCbks(MObject& node, void* clientData)
{
    MStatus res;
    MCallbackId id;
    MFnMesh meshFn(node, &res);
    if (res == MStatus::kSuccess)
    {
        id = MNodeMessage::addAttributeChangedCallback(node, fOnMeshAttrChange, NULL, &res);
        if (res == MStatus::kSuccess)
            ids.append(id);
        id = MNodeMessage::addAttributeAddedOrRemovedCallback(node, fOnNodeAttrAddedRemoved, NULL, &res);
        if (res == MStatus::kSuccess)
            ids.append(id);

        id = MPolyMessage::addPolyTopologyChangedCallback(node, fOnMeshTopoChange, NULL, &res);
        if (res == MStatus::kSuccess)
        {
            ids.append(id);
        }
        bool arr[4]{ true, false, false, false };
        id = MPolyMessage::addPolyComponentIdChangedCallback(node, arr, 4, fOnComponentChange, NULL, &res);
        if (res == MStatus::kSuccess)
        {
            ids.append(id);
        }
    }
}

void fTransAddCbks(MObject& node, void* clientData)
{
    MStatus res;
    MCallbackId id;
    MFnTransform transFn(node, &res);
    if (res == MStatus::kSuccess)
        id = MNodeMessage::addAttributeChangedCallback(node, fOnTransformAttrChange, NULL, &res);
    if (res == MStatus::kSuccess)
    {
        ids.append(id);
    }
 
}

void fDagNodeAddCbks(MObject& node, void* clientData)
{
    MStatus res;
    MFnDagNode dagFn(node, &res);
    if(res == MStatus::kSuccess)
    {
        MCallbackId id = MNodeMessage::addAttributeChangedCallback(node, fOnNodeAttrChange, NULL, &res);
        if (res == MStatus::kSuccess)
            ids.append(id);
        id = MNodeMessage::addAttributeAddedOrRemovedCallback(node, fOnNodeAttrAddedRemoved, NULL, &res);
        if (res == MStatus::kSuccess)
            ids.append(id);
    }    
}

void fOnNodeCreate(MObject& node, void *clientData)
{
    eNodeType nt = eNodeType::notHandled;
    MStatus res = MStatus::kNotImplemented;

    MGlobal::displayInfo("GOT INTO NODECREATE!");
    if (node.hasFn(MFn::kMesh))
    {
        nt = eNodeType::mesh; MGlobal::displayInfo("MESH!");
    }
    else if (node.hasFn(MFn::kTransform))
    {
        nt = eNodeType::transform; MGlobal::displayInfo("TRANSFORM!");
    }
    else if (node.hasFn(MFn::kDagNode))
    {
        nt = eNodeType::dagNode; MGlobal::displayInfo("NODE!");
    }

    switch (nt)
    {
        case(eNodeType::mesh):
        {
            MFnMesh meshFn(node, &res);
            if (res == MStatus::kSuccess)
            {
                fMeshAddCbks(node, clientData);
                fLoadMesh(meshFn);
            }
            break;
        }
        case(eNodeType::transform):
        {
            MFnTransform transFn(node, &res);
            if (res == MStatus::kSuccess)
                fTransAddCbks(node, clientData);
            break;
        }
        case(eNodeType::dagNode):
        {
            MFnDagNode dagFn(node, &res);
            if (res == MStatus::kSuccess)
                fDagNodeAddCbks(node, clientData);
            break;
        }
        case(eNodeType::notHandled):
        {
            break;
        }
    }

        if (res != MStatus::kSuccess && nt == eNodeType::mesh || res != MStatus::kSuccess && nt == eNodeType::transform)
        {
            MGlobal::displayInfo("PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP");
            queueList.push(node);
        }
        if (clientData != NULL)
        {
            if (*(bool*)clientData == true && res == MStatus::kSuccess)
            {
                MGlobal::displayInfo("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
                queueList.pop();
                delete clientData;
            }
        }
}

void fOnNodeNameChange(MObject &node, const MString &str, void *clientData)
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

void fOnElapsedTime(float elapsedTime, float lastTime, void *clientData)
{
    if (queueList.size() > 0)
    {
        MGlobal::displayInfo("TIME");
        bool* isRepeat = new bool(true);
        fOnNodeCreate(queueList.front(), isRepeat);
    }
}

void fIterateScene()
{
    MStatus res;
    MItDag nodeIt(MItDag::TraversalType::kBreadthFirst, MFn::Type::kDagNode, &res);
    
    if (res == MStatus::kSuccess)
    {
        while (!nodeIt.isDone())
        {
            fOnNodeCreate(nodeIt.currentItem(), NULL);
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
	temp = MDGMessage::addNodeAddedCallback(fOnNodeCreate,
		kDefaultNodeType,
		NULL,
		&res
	);
    if (res == MS::kSuccess)
    {
        MGlobal::displayInfo("nodeAdded success!");
        ids.append(temp);
    }
    temp = MNodeMessage::addNameChangedCallback(MObject::kNullObj, fOnNodeNameChange, NULL, &res);
    if (res == MStatus::kSuccess)
    {
        MGlobal::displayInfo("nameChange success!");
        ids.append(temp);
    }
    temp = MTimerMessage::addTimerCallback(1, fOnElapsedTime, NULL, &res);
    if (res == MStatus::kSuccess)
    {
        MGlobal::displayInfo("timerFunc success!");
        ids.append(temp);
    }
    fIterateScene();

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