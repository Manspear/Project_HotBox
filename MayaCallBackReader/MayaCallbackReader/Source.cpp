// UD1414_Plugin.cpp : Defines the exported functions for the DLL application.

#include "MayaIncludes.h"
#include "HelloWorldCmd.h"
#include "MayaHeader.h"
#include "CircularBuffer.h"
#include "Producer.h"
#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include <time.h>
using namespace std;

void fMakeMeshMessage(MObject obj, bool isFromQueue);
void fMakeCameraMessage(hCameraHeader& gCam);
void fTransAddCbks(MObject& node, void* clientData);
circularBuffer gCb;

float gClockTime;
float gClockticks;
float gMeshUpdateTimer;
float gDt30Fps;

#define BUFFERSIZE 8<<20
#define MAXMSGSIZE 2<<2
#define CHUNKSIZE 256

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
struct sBuiltVertex
{
	sPoint pnt;
    sUV uv;
    sNormal nor;
};
struct sMeshVertices
{
    std::vector<sBuiltVertex> vertices;
};
struct sMesh
{

    std::vector<sMeshVertices> vertexList;
};
//sAllVertex[36] trueList;
//sIndex[36] indexList;
//
///*Can you perhaps extend the idnex list?*/
//vertex;
//normal;
//UV;

/*
We can make it so that we only have to rebuild the vertex list once per mesh, no matter the number of vertives moved at once.
Do this by "unifying" the vertex list update.
A way to find selected vertices can be found under MGlobal::something
*/

MCallbackIdArray ids;
std::queue<MObject> queueList;
std::queue<MObject> updateMeshQueue;
//std::vector<sMesh> meshList;

/*e for enum*/
enum eNodeType
{
    mesh,
    transform,
    dagNode,
    notHandled,
	camera
};

void* HelloWorld::creator() { 
	return new HelloWorld;
};
MStatus HelloWorld::doIt(const MArgList& argList)
{
	MGlobal::displayInfo("Hello World!");
	return MS::kSuccess;
};

void fLoadMesh(MFnMesh& mesh, bool isFromQueue, std::vector<sBuiltVertex> &allVert)
{
    MIntArray normalCnts;
    MIntArray normalIDs;
    mesh.getNormalIds(normalCnts, normalIDs);

    /*FUCK INDEXING! LET'S DO IT THE SHITTY WAY!!!*/
    MStatus res;
    MIntArray triCnt;
    MIntArray triVert; //vertex Ids for each tri vertex

    const float * rwPnts = mesh.getRawPoints(&res);
    const float * rwNrmls = mesh.getRawNormals(&res);

    MStringArray uvSetNames;
    mesh.getUVSetNames(uvSetNames);

    //std::vector<sBuiltVertex> allVert;
    MPointArray pntArr;
    MVector tempVec;
    MPoint tempPoint;
    mesh.getTriangles(triCnt, triVert);
    allVert.resize(triVert.length());
    for (int i = 0; i < allVert.size(); i++)
    {
        mesh.getVertexNormal(triVert[i], tempVec, MSpace::kObject);  //<-- These used!
        mesh.getPoint(triVert[i], tempPoint, MSpace::kObject);
        allVert[i].pnt.x = tempPoint.x;
        allVert[i].pnt.y = tempPoint.y;
        allVert[i].pnt.z = tempPoint.z;
        allVert[i].nor.x = tempVec.x;
        allVert[i].nor.y = tempVec.y;
        allVert[i].nor.z = tempVec.z;

        pntArr.append(tempPoint);
    }
    MFloatArray u;
    MFloatArray v;
    mesh.getUVs(u, v, &uvSetNames[0]);
    MIntArray uvIndexArray;
    int uvId;
    int vertCnt;
    int triangleVerts[3];
    for (int i = 0; i < mesh.numPolygons(); i++)
    {
        vertCnt = mesh.polygonVertexCount(i, &res);
        if (res == MStatus::kSuccess)
        {
            for (int j = 0; j < triCnt[i]; j++)
            {
                mesh.getPolygonTriangleVertices(i, j, triangleVerts);

                mesh.getPolygonUVid(i, triangleVerts[0], uvId);      
                uvIndexArray.append(uvId);
                mesh.getPolygonUVid(i, triangleVerts[1], uvId);
                uvIndexArray.append(uvId);
                mesh.getPolygonUVid(i, triangleVerts[2], uvId);
                uvIndexArray.append(uvId);
            }
        } 
    }
    for (int i = 0; i < allVert.size(); i++)
    {
        mesh.getUV(uvIndexArray[i], allVert[i].uv.u, allVert[i].uv.v, &uvSetNames[0]);
    }
    MFloatArray uArr;
    MFloatArray vArr;
    mesh.getUVs(uArr, vArr, &uvSetNames[0]);
    for (int i = 0; i < allVert.size(); i++)
    {
        MGlobal::displayInfo(MString("Finished Vertex: Pos: ") + allVert[i].pnt.x + MString(" ") + allVert[i].pnt.y + MString(" ") + allVert[i].pnt.z + MString(" UV: ") + allVert[i].uv.u + MString(" ") + allVert[i].uv.v + MString(" Normal: ") + allVert[i].nor.x + MString(" ") + allVert[i].nor.y + MString(" ") + allVert[i].nor.z);
    }

    if (isFromQueue)
    {
        updateMeshQueue.pop();
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

void fFindShader(MObject& obj)
{

}

void fOnMeshAttrChange(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
    /*Limit the number of "updates per second" of this function*/
    if (gMeshUpdateTimer > gDt30Fps)
    {
        if (msg & MNodeMessage::AttributeMessage::kAttributeSet)
        {
            MStatus res;
            MObject temp = plug.node();

			/*MDagPath path = MDagPath::getAPathTo(temp, &res);*/

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
                updateMeshQueue.push(temp);

				/*MObjectArray sets;
				MObjectArray comps;
				unsigned int instanceNumber = path.instanceNumber();*/

				/*if (!meshFn.getConnectedSetsAndMembers(instanceNumber, sets, comps, true))
				{
					if (sets.length())
					{
						MObject set = sets[0];
						MObject comp = comps[0];
					}
				}*/

            }
        }
        gMeshUpdateTimer = 0;
    }

   // MGlobal::displayInfo(MString("Clock timer: ") + difftime(meshUpdateTimerCompare, meshUpdateTimer));
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
                    MTransformationMatrix transMat = fnTra.transformationMatrix();

                    MTransformationMatrix::RotationOrder rotOrder;
                    double rot[3];
                    double scale[3];
					MVector tempTrans = transMat.getTranslation(MSpace::kObject);
					double trans[3]; 
					tempTrans.get(trans);

					transMat.getRotation(rot, rotOrder);
                    transMat.getScale(scale, MSpace::kObject);

                    MFnAttribute fnAtt(plug.attribute(), &res);
                    if (res == MStatus::kSuccess)
                    {
                        MGlobal::displayInfo("Transform node: " + fnTra.name() 
							+ " Trans: " + trans[0] + " " + trans[1] + " " + trans[2] 
							+ " Rot: " + rot[0] + " " + rot[1] + " " + rot[2] 
							+ " Scale: " + scale[0] + " " + scale[1] + " " + scale[2]);
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

void fCameraChanged(const MString &str, void* clientData)
{
	MStatus res;

	MStatus status = MStatus::kFailure;

	M3dView cameraView = M3dView::active3dView();

	MMatrix camProjMatrix;

	cameraView.projectionMatrix(camProjMatrix);
	
	MDagPath cameraPath;

	cameraView.getCamera(cameraPath);

	MFnCamera camera(cameraPath.node());

	MFloatMatrix mat = camera.projectionMatrix();

	mat.matrix[2][2] -= mat.matrix[2][2];
	mat.matrix[3][2] -= mat.matrix[3][2];

	for (int row = 0; row < 4; row++)
	{
		for (int column = 0; column < 4; column++)
		{
			MGlobal::displayInfo(MString() + mat.matrix[row][column]);
		}
	}

	MGlobal::displayInfo(camera.name());

	hCameraHeader gCam;

	gCam.cameraName = camera.name().asChar();
	gCam.cameraNameLength = camera.name().length();

	memcpy(&gCam.projMatrix, &mat, sizeof(float) * 16);

	MGlobal::displayInfo("Camera changed...");

	fMakeCameraMessage(gCam);

	/*Make a message to send the camera in to the Circle Buffer.*/
	//Call function here to send this message. 
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
        id = MNodeMessage::addAttributeChangedCallback(node, fOnTransformAttrChange, clientData, &res);
    if (res == MStatus::kSuccess)
    {
        ids.append(id);
    }
}

void fCameraAddCbks(MObject& node, void* clientData)
{
	MStatus res;
	MCallbackId id;
	MFnCamera camFn(node, &res);
	if (res == MStatus::kSuccess)
	{
		id = MUiMessage::add3dViewPreRenderMsgCallback(MString("modelPanel4"), fCameraChanged, NULL, &res);
		if (res == MStatus::kSuccess)
		{
			/*Seems like entering any Panel from 1 to 4 won't matter. Still get the viewport were currently in.*/
			MGlobal::displayInfo("cameraChanged success!");
			ids.append(id);
		}
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

	else if (node.hasFn(MFn::kCamera))
	{
		nt = eNodeType::camera; MGlobal::displayInfo("CAMERA!");
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
                //fLoadMesh(meshFn, false);
				fMakeMeshMessage(node, false);
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
		case(eNodeType::camera):
		{
			MFnCamera camFn(node, &res);
			if (res == MStatus::kSuccess)
				fCameraAddCbks(node, clientData);
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
            queueList.push(node);
        }

        if (clientData != NULL)
        {
            if (*(bool*)clientData == true && res == MStatus::kSuccess)
            {
                queueList.pop();
                delete clientData;
            }
        }
}

void fOnNodeNameChange(MObject &node, const MString &str, void *clientData)
{
    MStatus res = MStatus::kSuccess;

    MFnTransform transFn(node, &res);
    
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
    float oldTime = gClockTime;
    gClockticks = clock();
    gClockTime = gClockticks / CLOCKS_PER_SEC;
    gDt30Fps = gClockTime - oldTime;

    gMeshUpdateTimer += gDt30Fps;

    if (queueList.size() > 0)
    {
        MGlobal::displayInfo("TIME");
        bool* isRepeat = new bool(true);
        fOnNodeCreate(queueList.front(), isRepeat);
    }
    if (updateMeshQueue.size() > 0)
    {
        //fLoadMesh(MFnMesh(updateMeshQueue.front()), true);
		fMakeMeshMessage(updateMeshQueue.front(), true);
    }
    //MGlobal::displayInfo(MString("MeshQueue LENGTH: ") + updateMeshQueue.size());
}

void fMakeMeshMessage(MObject obj, bool isFromQueue)
{
    MStatus res;

	std::vector<sBuiltVertex> meshVertices;

    MFnMesh mesh(obj);

	fLoadMesh(mesh, isFromQueue, meshVertices);

    /*Have this function become awesome*/
    hMainHeader mainH;

    mainH.meshCount = 1;

    mainH.transformCount = 0;
	mainH.cameraCount = 0;
    mainH.lightCount = 0;
    mainH.materialCount = 0;

    hMeshHeader meshH;
    meshH.materialId = 0;
    meshH.meshName = mesh.name().asChar();
    meshH.meshNameLen = mesh.name().length();
    meshH.vertexCount = meshVertices.size();
    
    //Getting the first transform parent found. Will most likely be the direct parent.
    MObject parObj;
    for (unsigned int i = 0; i < mesh.parentCount(); i++)
    {
        parObj = mesh.parent(i, &res);
        if (parObj.hasFn(MFn::kTransform))
        {
            MFnTransform transFn(parObj);
            meshH.prntTransName = transFn.name().asChar();
            meshH.prntTransNameLen = transFn.name().length();
            break;
        }
    }
    
    /*Now put the mainHeader first, then the meshHeader, then the vertices*/
    size_t mainHMem = sizeof(mainH);
    size_t meshHMem = sizeof(meshH);
    size_t meshVertexMem = sizeof(sBuiltVertex);

    int totPackageSize = mainHMem + meshHMem + meshH.meshNameLen + meshH.prntTransNameLen + meshH.vertexCount * meshVertexMem;

    /*
    In the initialize-function, maybe have an array of msg. Pre-sized so that there's a total of 4 messages
    check if a message is "active", if it's unactive, i.e already read, the queued thing may memcpy into it.
    */
    char* msg = new char[totPackageSize];
    
    memcpy(msg, (void*)&mainH, (size_t)mainHMem);
    memcpy(msg + mainHMem, (void*)&meshH, (size_t)meshHMem);
    memcpy(msg + mainHMem + meshHMem, (void*)meshH.meshName, meshH.meshNameLen);
    memcpy(msg + mainHMem + meshHMem + meshH.meshNameLen, (void*)meshH.prntTransName, meshH.prntTransNameLen);
    memcpy(msg + mainHMem + meshHMem + meshH.meshNameLen + meshH.prntTransNameLen, meshVertices.data(), meshVertices.size() * meshVertexMem);

    size_t bufferSize = BUFFERSIZE;
    size_t maxMsgSize = MAXMSGSIZE;

    int delay = 0;
    int numMessages;
    int chunkSize = CHUNKSIZE;
    LPCWSTR varBuffName = TEXT("VarBuffer");

    Producer producer = Producer(1, chunkSize, varBuffName);
    producer.runProducer(gCb, (char*)msg, totPackageSize);
}

void fMakeCameraMessage(hCameraHeader& gCam)
{
	hMainHeader hMainHead;
	size_t mainMem = sizeof(hMainHead);
	size_t camMem = sizeof(hCameraHeader);

	hMainHead.cameraCount = 1;

	hMainHead.meshCount = 0;
	hMainHead.lightCount = 0;
	hMainHead.materialCount = 0;
	hMainHead.transformCount = 0;

	int totalSize = mainMem + camMem + gCam.cameraNameLength;

	char* msg = new char[totalSize];

	memcpy(msg, (void*)&hMainHead, mainMem);
	memcpy(msg + mainMem, (void*)&gCam, camMem);
	memcpy(msg + mainMem + camMem, (void*)gCam.cameraName, gCam.cameraNameLength);

	size_t bufferSize = BUFFERSIZE;
	size_t maxMsgSize = MAXMSGSIZE;

	int delay = 0;
	int numMessages;
	int chunkSize = CHUNKSIZE;
	LPCWSTR varBuffName = TEXT("VarBuffer");

	Producer producer = Producer(1, chunkSize, varBuffName);
	producer.runProducer(gCb, (char*)msg, totalSize);
}

void fMakeTransformMessage(MObject obj)
{
    MStatus res;
    MFnTransform trans(obj);
    hTransformHeader transH;
    MObject childObj;

    /*Getting the first mesh child*/
    for (unsigned int i = 0; i < trans.childCount(); i++)
    {
        childObj = trans.parent(i, &res);
        if (childObj.hasFn(MFn::kTransform))
        {
            MFnMesh meshFn(childObj);
            transH.childName = meshFn.name().asChar();
            transH.childNameLength = meshFn.name().length();
            break;
        }
    }
}
void fMakeLightMessage()
{

}
void fMakeGenericMessage()
{
    
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
}

void fOnNodeRemoved(MObject& node, void* clientData)
{
	MGlobal::displayInfo(MString("I got removed!"));
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

	temp = MDGMessage::addNodeRemovedCallback(fOnNodeRemoved, MString("kDefaultNodeType"), NULL, &res);
	if (res == MStatus::kSuccess)
	{
		MGlobal::displayInfo("nodeRemoved success!");
		ids.append(temp);
	}

    temp = MNodeMessage::addNameChangedCallback(MObject::kNullObj, fOnNodeNameChange, NULL, &res);
    if (res == MStatus::kSuccess)
    {
        MGlobal::displayInfo("nameChange success!");
        ids.append(temp);
    }
    temp = MTimerMessage::addTimerCallback(0.03, fOnElapsedTime, NULL, &res);
    if (res == MStatus::kSuccess)
    {
        MGlobal::displayInfo("timerFunc success!");
        ids.append(temp);
    }

    fIterateScene();

    float oldTime = gClockTime;
    gClockticks = clock();
    gClockTime = gClockticks / CLOCKS_PER_SEC;
    float dt = gClockTime - oldTime;

    gMeshUpdateTimer = 0;

    gCb.initCircBuffer(TEXT("MessageBuffer"), BUFFERSIZE, 0, CHUNKSIZE, TEXT("VarBuffer"));

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