// UD1414_Plugin.cpp : Defines the exported functions for the DLL application.

#include "MayaIncludes.h"
#include "HelloWorldCmd.h"
#include "MayaHeader.h"
#include "CircularBuffer.h"
#include "Producer.h"
#include "Mutex.h"
#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include <time.h>
using namespace std;

void fMakeTransformMessage(MObject obj, hTransformHeader transH);
void fMakeMeshMessage(MObject obj, bool isFromQueue);
void fMakeCameraMessage(hCameraHeader& gCam);

void fTransAddCbks(MObject& node, void* clientData);
circularBuffer gCb;
Producer producer;
Mutex mtx;

float gClockTime;
float gClockticks;
float gMeshUpdateTimer;
float gDt30Fps;

#define BUFFERSIZE 8<<20
#define MAXMSGSIZE 2<<20
#define CHUNKSIZE 256

bool firstActiveCam = true;

char* msg;

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

void fCamChange(const MString &str, MObject &node, void* clientData)
{
	MStatus res;

	MGlobal::displayInfo("BLA!");

	MFnCamera camFn(node, &res);

	if (res == MStatus::kSuccess)
	{
		MFloatMatrix transMat = camFn.projectionMatrix();

		float test[16];

		memcpy(test, &transMat, sizeof(MFloatMatrix));

		for (int i = 0; i < 16; i++)
		{
			MGlobal::displayInfo(MString() + test[i]);
		}
	}
}

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

void fOnMeshAttrChange(MNodeMessage::AttributeMessage attrMessage, MPlug &plug, MPlug &otherPlug, void *clientData)
{
    /*Limit the number of "updates per second" of this function*/
    if (gMeshUpdateTimer > gDt30Fps)
    {
        if (attrMessage & MNodeMessage::AttributeMessage::kAttributeSet)
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

void fOnTransformAttrChange(MNodeMessage::AttributeMessage attrMessage, MPlug &plug, MPlug &otherPlug, void *clientData)
{
    if (attrMessage & MNodeMessage::AttributeMessage::kAttributeSet)
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
					double rot[4];
					double scale[3];
					MVector tempTrans = transMat.getTranslation(MSpace::kWorld);
					double trans[3];
					tempTrans.get(trans); 
					//trans[0] = -trans[0];
					transMat.getRotation(rot, rotOrder);
					transMat.getRotationQuaternion(rot[0], rot[1], rot[2], rot[3], MSpace::kWorld);
					transMat.getScale(scale, MSpace::kObject);

                    MFnAttribute fnAtt(plug.attribute(), &res);
                    if (res == MStatus::kSuccess)
                    {
                       /* MGlobal::displayInfo("Transform node: " + fnTra.name() 
							+ " Trans: " + trans[0] + " " + trans[1] + " " + trans[2] 
							+ " Rot: " + rot[0] + " " + rot[1] + " " + rot[2] 
							+ " Scale: " + scale[0] + " " + scale[1] + " " + scale[2]);*/

						hTransformHeader hTrans;

						std::copy(trans, trans + 3, hTrans.trans);
						std::copy(rot, rot + 4, hTrans.rot);
						std::copy(scale, scale + 3, hTrans.scale);

						fMakeTransformMessage(obj, hTrans);
                    }
                }
            }
        }
    }
}

void fOnNodeAttrChange(MNodeMessage::AttributeMessage attrMessage, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	if (attrMessage & MNodeMessage::AttributeMessage::kAttributeSet && !plug.isArray() && plug.isElement())
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
    else if (attrMessage & MNodeMessage::AttributeMessage::kAttributeSet)
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

void fOnNodeAttrAddedRemoved(MNodeMessage::AttributeMessage attrMessage, MPlug &plug, void *clientData)
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
		id = MNodeMessage::addAttributeChangedCallback(node, fOnTransformAttrChange, clientData, &res);
	if (res == MStatus::kSuccess)
	{
		ids.append(id);
	}
}

void fLoadCamera(M3dView& activeView)
{
	hCameraHeader hCam;
	MStatus res;
	MFloatMatrix projMatrix;
	
	MDagPath cameraPath;

	if(activeView.getCamera(cameraPath))
	{
		MFnCamera camFn(cameraPath.node(), &res);
		if (res == MStatus::kSuccess)
		{
			MFloatMatrix projMatrix = camFn.projectionMatrix();

			projMatrix.matrix[2][2] = -projMatrix.matrix[2][2];
			projMatrix.matrix[3][2] = -projMatrix.matrix[3][2];

			memcpy(hCam.projMatrix, &camFn.projectionMatrix(), sizeof(MFloatMatrix));
			hCam.cameraName = camFn.name().asChar();
			hCam.cameraNameLength = camFn.name().length();

			MFnTransform fnTransform(camFn.parent(0), &res);
			if (res == MStatus::kSuccess)
			{
				MVector tempTrans = fnTransform.getTranslation(MSpace::kObject, &res);

				double camTrans[3];
				camTrans[0] = -camTrans[0];

				tempTrans.get(camTrans);
				double camScale[3];
				fnTransform.getScale(camScale);
				double camQuat[4];
				fnTransform.getRotationQuaternion(camQuat[0], camQuat[1], camQuat[2], camQuat[3], MSpace::kWorld);

				std::copy(camTrans, camTrans + 3, hCam.trans);
				std::copy(camScale, camScale + 3, hCam.scale);
				std::copy(camQuat, camQuat + 4, hCam.rot);
			}
		}
	}

	fMakeCameraMessage(hCam);
}

void fCameraChanged(const MString &str, void* clientData)
{
	hCameraHeader hCam;
	MStatus res;
	MMatrix projMatrix;

	M3dView activeView = M3dView::active3dView();

	MDagPath cameraPath;

	if (activeView.getCamera(cameraPath))
		{
		MFnCamera camFn(cameraPath.node(), &res);

		if (res == MStatus::kSuccess)
		{
			MFloatMatrix projMatrix = camFn.projectionMatrix();

			projMatrix.matrix[2][2] = -projMatrix.matrix[2][2];
			projMatrix.matrix[3][2] = -projMatrix.matrix[3][2];

			memcpy(hCam.projMatrix, &camFn.projectionMatrix(), sizeof(MFloatMatrix));
			hCam.cameraName = camFn.name().asChar();
			hCam.cameraNameLength = camFn.name().length();

			MFnTransform fnTransform(camFn.parent(0), &res);
			if (res == MStatus::kSuccess)
			{
				MVector tempTrans = fnTransform.getTranslation(MSpace::kTransform, &res);

				double camTrans[3];
				camTrans[0] = -camTrans[0];

				tempTrans.get(camTrans);
				double camScale[3];
				fnTransform.getScale(camScale);
				double camQuat[4];
				fnTransform.getRotationQuaternion(camQuat[0], camQuat[1], camQuat[2], camQuat[3], MSpace::kTransform);

				std::copy(camTrans, camTrans + 3, hCam.trans);
				std::copy(camScale, camScale + 3, hCam.scale);
				std::copy(camQuat, camQuat + 4, hCam.rot);
			}
		}
	}
		fMakeCameraMessage(hCam);
}

void fCameraAddCbks(MObject& node, void* clientData)
{
	MStatus res;
	MCallbackId id;
	MFnCamera camFn(node, &res);
	if (res == MStatus::kSuccess)
	{
		M3dView activeCamView = M3dView::active3dView(&res);

		if (res == MStatus::kSuccess)
		{
			if (firstActiveCam == true)
			{
				/*Load the active camera when plugin is initialized.*/
				fLoadCamera(activeCamView);
				firstActiveCam = false;
			}

			/*Collect changes when active camera changes.*/
			id = MUiMessage::add3dViewPreRenderMsgCallback(MString("modelPanel4"), fCameraChanged, NULL, &res);
			if (res == MStatus::kSuccess)
			{
				/*Seems like entering any Panel from 1 to 4 won't matter. Still get the viewport were currently in.*/
				MGlobal::displayInfo("cameraChanged success!");
				ids.append(id);
			}
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

void fLoadTransform(MObject& obj, void* clientData)
{
	MStatus res;

	/*if (obj.hasFn(MFn::kTransform))
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

			hTransformHeader hTrans;

			std::copy(trans, trans + 3, hTrans.trans);
			std::copy(rot, rot + 3, hTrans.rot);
			std::copy(scale, scale + 3, hTrans.scale);

			fMakeTransformMessage(obj, hTrans);

			}
		}*/
}

void fOnNodeCreate(MObject& node, void *clientData)
{
    eNodeType nt = eNodeType::notHandledNode;
    MStatus res = MStatus::kNotImplemented;

    MGlobal::displayInfo("GOT INTO NODECREATE!");
    if (node.hasFn(MFn::kMesh))
    {
        nt = eNodeType::meshNode; MGlobal::displayInfo("MESH!");
    }
    else if (node.hasFn(MFn::kTransform))
    {
        nt = eNodeType::transformNode; MGlobal::displayInfo("TRANSFORM!");
    }

	else if (node.hasFn(MFn::kCamera))
	{
		nt = eNodeType::cameraNode; MGlobal::displayInfo("CAMERA!");
	}

    else if (node.hasFn(MFn::kDagNode))
    {
        nt = eNodeType::dagNode; MGlobal::displayInfo("NODE!");
    }

    switch (nt)
    {
        case(eNodeType::meshNode):
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
        case(eNodeType::transformNode):
        {
            MFnTransform transFn(node, &res);
			if (res == MStatus::kSuccess)
			{
				fLoadTransform(node, clientData);
				fTransAddCbks(node, clientData);
			}
			break;
        }
		case(eNodeType::cameraNode):
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
        case(eNodeType::notHandledNode):
        {
            break;
        }
    }

	if (res != MStatus::kSuccess && nt == eNodeType::meshNode || res != MStatus::kSuccess && nt == eNodeType::transformNode
		|| res != MStatus::kSuccess && nt == eNodeType::cameraNode)
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
	mtx.lock();
    memcpy(msg, (void*)&mainH, (size_t)mainHMem);
    memcpy(msg + mainHMem, (void*)&meshH, (size_t)meshHMem);
    memcpy(msg + mainHMem + meshHMem, (void*)meshH.meshName, meshH.meshNameLen);
    memcpy(msg + mainHMem + meshHMem + meshH.meshNameLen, (void*)meshH.prntTransName, meshH.prntTransNameLen);
    memcpy(msg + mainHMem + meshHMem + meshH.meshNameLen + meshH.prntTransNameLen, meshVertices.data(), meshVertices.size() * meshVertexMem);

    producer.runProducer(gCb, (char*)msg, totPackageSize);
	mtx.unlock();
}

void fMakeCameraMessage(hCameraHeader& gCam)
{
	hMainHeader hMainHead;
	size_t mainMem = sizeof(hMainHead);
	size_t camMem = sizeof(hCameraHeader);

	hMainHead.cameraCount = 1;

	int totalSize = mainMem + camMem + gCam.cameraNameLength;

	mtx.lock();
	memcpy(msg, (void*)&hMainHead, mainMem);
	memcpy(msg + mainMem, (void*)&gCam, camMem);
	memcpy(msg + mainMem + camMem, (void*)gCam.cameraName, gCam.cameraNameLength);

	producer.runProducer(gCb, (char*)msg, totalSize);
	mtx.unlock();
}

void fMakeTransformMessage(MObject obj, hTransformHeader transH)
{ /*use mtx.lock() before the memcpys', and mtx.unlock() right after producer.runProducer()*/
    MStatus res;
    MFnTransform trans(obj);
    MObject childObj;
	//MObject parentObj;
	hMainHeader mainH;
	mainH.transformCount = 1;
	bool foundChild = false;
	/*If the transform has a parent, obtain it's name and length.*/
	//if (trans.hasParent(obj))
	//{
	//	/*Getting the first parent for mesh, camera and light.*/
	//	for (unsigned int i = 0; i < trans.parentCount(); i++)
	//	{
	//		parentObj = trans.parent(i, &res);
	//		if (parentObj.hasFn(MFn::kTransform))
	//		{
	//			MFnMesh meshFn(childObj, &res);
	//			if (res == MStatus::kSuccess)
	//			{
	//				transH.parentName = meshFn.name().asChar();
	//				transH.parentNameLength = meshFn.name().length();
	//				break;
	//			}
	//			MFnLight lightFn(childObj, &res);
	//			if (res == MStatus::kSuccess)
	//			{
	//				transH.parentName = meshFn.name().asChar();
	//				transH.parentNameLength = meshFn.name().length();
	//				break;
	//			}
	//		}
	//	}
	//}
	///*If there are no parents of this transform, set default values.*/
	//else
	//{
	//	transH.parentName = "1337";
	//	transH.parentNameLength = 0;
	//}

	/*Getting the first child for mesh, camera and light.*/
	for (unsigned int i = 0; i < trans.childCount(); i++)
	{
		childObj = trans.child(i, &res);

		//MFnTransform childTrans(childObj);
		//if (childObj.hasFn(MFn::kTransform))
		//{
		//	transH.childName = childTrans.name().asChar();
		//	transH.childNameLength = childTrans.name().length();
		//}
		//MFnLight lightFn(childObj, &res);
		//if (res == MStatus::kSuccess)
		//{
		//	transH.childName = lightFn.name().asChar();
		//	transH.childNameLength = lightFn.name().length();
		//	break;
		//}
		MFnMesh meshFn(childObj, &res);
		if (res == MStatus::kSuccess)
		{
			transH.childName = meshFn.name().asChar();
			transH.childNameLength = meshFn.name().length();
			foundChild = true;
			break;
		}
	}
	if (foundChild)
	{
		mtx.lock();
		memcpy(msg, &mainH, sizeof(hMainHeader));
		memcpy(msg + sizeof(hMainHeader), &transH, sizeof(hTransformHeader));
		memcpy(msg + sizeof(hMainHeader) + sizeof(hTransformHeader), transH.childName, transH.childNameLength);
		int totPacketSize = sizeof(hMainHeader) + sizeof(hTransformHeader) + transH.childNameLength;
		producer.runProducer(
			gCb,
			msg,
			sizeof(hMainHeader) +
			sizeof(hTransformHeader) +
			transH.childNameLength);
		mtx.unlock();
	}
}
void fMakeLightMessage()
{
	/*use mtx.lock() before the memcpys', and mtx.unlock() right after producer.runProducer()*/
}
void fMakeGenericMessage()
{
	/*use mtx.lock() before the memcpys', and mtx.unlock() right after producer.runProducer()*/
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

void fMakeRemovedMessage(MObject& node, eNodeType nodeType)
{
	MStatus res;
	hMainHeader mainH;
	mainH.removedObjectCount = 1;
	hRemovedObjectHeader roh;

	mtx.lock();
	if (nodeType == eNodeType::meshNode)
	{
		MFnMesh mesh(node, &res);
		if (res == MStatus::kSuccess)
		{
			roh.nodeType = eNodeType::meshNode;
			roh.nameLength = std::strlen(mesh.name().asChar());
			memcpy(msg, &mainH, sizeof(hMainHeader));
			/*First removed object header*/
			memcpy(msg + sizeof(hMainHeader), &roh, sizeof(hRemovedObjectHeader));
			/*Then the name*/
			memcpy(msg + sizeof(hMainHeader) + sizeof(hRemovedObjectHeader), mesh.name().asChar(), roh.nameLength);
			
			MGlobal::displayInfo(MString("Deleted mesh: ") + MString(mesh.name()));
		}
	}/*else
	if (nodeType == eNodeType::transformNode)
	{
		MFnTransform trans(node, &res);
		if (res == MStatus::kSuccess)
		{
			roh.nodeType = eNodeType::transformNode;
			roh.nameLength = std::strlen(trans.name().asChar());
			memcpy(msg, &mainH, sizeof(hMainHeader));
			memcpy(msg + sizeof(hMainHeader), trans.name().asChar(), roh.nameLength);

			MGlobal::displayInfo(MString("Deleted transform: ") + MString(trans.name()));
		}
	}else
	if (nodeType == eNodeType::cameraNode)
	{
		MFnCamera cam(node, &res);
		if (res == MStatus::kSuccess)
		{
			roh.nodeType = eNodeType::cameraNode;
			roh.nameLength = std::strlen(cam.name().asChar());
			memcpy(msg, &mainH, sizeof(hMainHeader));
			memcpy(msg + sizeof(hMainHeader), cam.name().asChar(), roh.nameLength);

			MGlobal::displayInfo(MString("Deleted camera: ") + MString(cam.name()));
		}
	}else
	if (nodeType == eNodeType::pointLightNode)
	{
		MFnPointLight pl(node, &res);
		if (res == MStatus::kSuccess)
		{
			roh.nodeType = eNodeType::pointLightNode;
			roh.nameLength = std::strlen(pl.name().asChar());
			memcpy(msg, &mainH, sizeof(hMainHeader));
			memcpy(msg + sizeof(hMainHeader), pl.name().asChar(), roh.nameLength);

			MGlobal::displayInfo(MString("Deleted pointLight: ") + MString(pl.name()));
		}
	}*/
	if (res == MStatus::kSuccess)
	{
		producer.runProducer(gCb, msg, sizeof(hMainHeader) + sizeof(hRemovedObjectHeader) + roh.nameLength);
	}
	mtx.unlock();
}

void fOnNodeRemoved(MObject& node, void* clientData)
{
	MGlobal::displayInfo(MString("I got removed! " + MString(node.apiTypeStr())));
}

void fOnMeshRemoved(MObject& node, void* clientData)
{
	MGlobal::displayInfo(MString("Mesh got removed! ") + MString(node.apiTypeStr()));

	/*Make a meshRemoved message*/
	fMakeRemovedMessage(node, eNodeType::meshNode);
}
void fOnTransformRemoved(MObject& node, void* clientData)
{
	MGlobal::displayInfo(MString("Transform got removed! ") + MString(node.apiTypeStr()));
}
void fOnCameraRemoved(MObject& node, void* clientData)
{
	MGlobal::displayInfo(MString("Camera got removed! ") + MString(node.apiTypeStr()));
}
void fOnPointLightRemoved(MObject& node, void* clientData)
{
	MGlobal::displayInfo(MString("pointLight got removed! ") + MString(node.apiTypeStr()));
}

void fAddCallbacks()
{
	MStatus res;
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
	temp = MDGMessage::addNodeRemovedCallback(fOnNodeRemoved, kDefaultNodeType, NULL, &res);
	if (res == MStatus::kSuccess)
	{
		MGlobal::displayInfo("nodeRemoved success!");
		ids.append(temp);
	}
	temp = MDGMessage::addNodeRemovedCallback(fOnMeshRemoved, "mesh", NULL, &res);
	if (res == MStatus::kSuccess)
	{
		MGlobal::displayInfo("meshRemoved success!");
		ids.append(temp);
	}
	temp = MDGMessage::addNodeRemovedCallback(fOnTransformRemoved, "transform", NULL, &res);
	if (res == MStatus::kSuccess)
	{
		MGlobal::displayInfo("transformRemoved success!");
		ids.append(temp);
	}
	temp = MDGMessage::addNodeRemovedCallback(fOnCameraRemoved, "camera", NULL, &res);
	if (res == MStatus::kSuccess)
	{
		MGlobal::displayInfo("cameraRemoved success!");
		ids.append(temp);
	}
	temp = MDGMessage::addNodeRemovedCallback(fOnPointLightRemoved, "pointLight", NULL, &res);
	if (res == MStatus::kSuccess)
	{
		MGlobal::displayInfo("pointLightRemoved success!");
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
	LPCWSTR mtxName = TEXT("producerMutex");
	mtx = Mutex(mtxName);
	fAddCallbacks();

	gCb.initCircBuffer(TEXT("MessageBuffer"), BUFFERSIZE, 0, CHUNKSIZE, TEXT("VarBuffer"));

    float oldTime = gClockTime;
    gClockticks = clock();
    gClockTime = gClockticks / CLOCKS_PER_SEC;
    float dt = gClockTime - oldTime;

    gMeshUpdateTimer = 0;

	msg = new char[MAXMSGSIZE];

	int chunkSize = CHUNKSIZE;
	LPCWSTR varBuffName = TEXT("VarBuffer");
	producer = Producer(1, chunkSize, varBuffName);

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

	delete[] msg;

	return MS::kSuccess;
}

//int awesomeFunction()
//{
//	return 69;
//}