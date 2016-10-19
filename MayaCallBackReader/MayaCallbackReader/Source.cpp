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
circularBuffer* gCb;
Producer* producer;
Mutex mtx;

float gClockTime;
float gClockticks;
float gMeshUpdateTimer;
float gTransformUpdateTimer;
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

MCallbackIdArray ids;
std::queue<MObject> gObjQueue;

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

void fOnMeshAttrChange(MNodeMessage::AttributeMessage attrMessage, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	///*Limit the number of "updates per second" of this function*/
	//if (gMeshUpdateTimer > gDt30Fps)
	//{
	if (attrMessage & MNodeMessage::AttributeMessage::kAttributeSet)
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
			fMakeMeshMessage(temp, false);
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

void fLoadCamera()
{
	hCameraHeader hCam;
	MStatus res;
	MFloatMatrix projMatrix;
	
	MDagPath cameraPath;

	M3dView activeView = M3dView::active3dView();

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
				MVector tempTrans = fnTransform.getTranslation(MSpace::kTransform, &res);

				double camTrans[3];
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
	static MMatrix oldMat;
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
			MFnTransform fnTransform(camFn.parent(0), &res);

			MMatrix newMat = fnTransform.transformationMatrix();

			MFloatMatrix projMatrix = camFn.projectionMatrix();

			projMatrix.matrix[2][2] = -projMatrix.matrix[2][2];
			projMatrix.matrix[3][2] = -projMatrix.matrix[3][2];

			static MFloatMatrix oldProjMatrix = projMatrix;

			memcpy(hCam.projMatrix, &camFn.projectionMatrix(), sizeof(MFloatMatrix));
			hCam.cameraName = camFn.name().asChar();
			hCam.cameraNameLength = camFn.name().length();

			if (memcmp(&newMat, &oldMat, sizeof(MMatrix)) != 0 || memcmp(&projMatrix, &oldProjMatrix, sizeof(MFloatMatrix)) != 0)
			{
				oldMat = newMat;
				oldProjMatrix = projMatrix;

				if (res == MStatus::kSuccess)
				{
					MVector tempTrans = fnTransform.getTranslation(MSpace::kTransform, &res);

					double camTrans[3];
					tempTrans.get(camTrans);
					double camScale[3];
					fnTransform.getScale(camScale);
					double camQuat[4];
					fnTransform.getRotationQuaternion(camQuat[0], camQuat[1], camQuat[2], camQuat[3], MSpace::kTransform);

					std::copy(camTrans, camTrans + 3, hCam.trans);
					std::copy(camScale, camScale + 3, hCam.scale);
					std::copy(camQuat, camQuat + 4, hCam.rot);
				}

				fMakeCameraMessage(hCam);
			}
		}	
	}
		
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
				fLoadCamera();
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

MObject fFindMaterialConnected(MObject node)
{
	MStatus res;

	if (node.hasFn(MFn::kMesh))
	{
		MDagPath dp = MDagPath::getAPathTo(node);

		MFnMesh fnMesh(dp, &res);

		unsigned int instNum = dp.instanceNumber();

		MObjectArray sets, comps;

		if (!fnMesh.getConnectedSetsAndMembers(instNum, sets, comps, true))
			MGlobal::displayInfo("FAILED!");

		if (sets.length())
		{
			MObject set = sets[0];
			MObject comp = comps[0];

			return set;
		}
	}

	return MObject::kNullObj;
}

MObject fFindShader(MObject& setNode)
{
	MFnDependencyNode fnNode(setNode);
	MPlug shaderPlug = fnNode.findPlug("surfaceShader");

	if (!shaderPlug.isNull())
	{
		MPlugArray connectedPlugs;

		shaderPlug.connectedTo(connectedPlugs, true, false);

		if (connectedPlugs.length() != 1)
		{
			MGlobal::displayInfo("Error getting the shader...");
		}

		else
		{
			return connectedPlugs[0].node();
		}
	}

	return MObject::kNullObj;
}

void fMakeMaterialMessage(hMaterialHeader hMaterial)
{
	MGlobal::displayInfo("MaterialMsg!");

	hMainHeader HMainHead;
	size_t mainMem = sizeof(hMainHeader);
	size_t materialMem = sizeof(hMaterialHeader);

	HMainHead.materialCount = 1;

	hMaterial.materialNameLength++;
	hMaterial.connectedMeshNameLength++;
	int totalSize = mainMem + materialMem + hMaterial.materialNameLength + hMaterial.connectedMeshNameLength;

	mtx.lock();
	memcpy(msg, (void*)&HMainHead, mainMem);
	memcpy(msg + mainMem, (void*)&hMaterial, materialMem);
	memcpy(msg + mainMem + materialMem, (void*)hMaterial.materialName, hMaterial.materialNameLength - 1);
	memcpy(msg + mainMem + materialMem + hMaterial.materialNameLength, (void*)hMaterial.connectedMeshName, hMaterial.connectedMeshNameLength - 1);

	*(char*)(msg + mainMem + materialMem + hMaterial.materialNameLength - 1) = '\0';
	*(char*)(msg + mainMem + materialMem + hMaterial.materialNameLength + hMaterial.connectedMeshNameLength - 1) = '\0';

	producer->runProducer(gCb, (char*)msg, totalSize);
	mtx.unlock();
}

void fOnMaterialAttrChanges(MNodeMessage::AttributeMessage attrMessage, MPlug& plug, MPlug& otherPlug, void* clientData)
{
	MStatus res;
	MObject tempData;
	float rgb[3];

	hMaterialHeader hMaterial;

	hMaterial.materialName = MFnDependencyNode(plug.node()).name().asChar();
	hMaterial.materialNameLength = MFnDependencyNode(plug.node()).name().length();

	/**(MObject*)clientData;

	if (clientData != NULL)
	{
		int test = 0;
	}*/

	/*hMaterial.connectedMeshName = plug.name().asChar();
	hMaterial.connectedMeshNameLength = plug.name().length();*/

	MPlug colorPlug = MFnDependencyNode(plug.node()).findPlug("color", &res);
	if (res == MStatus::kSuccess)
	{
		if (res == MStatus::kSuccess)
		{
			colorPlug.getValue(tempData);
			MFnNumericData colorData(tempData);

			colorData.getData(rgb[0], rgb[1], rgb[2]);

			hMaterial.diffuseColor[0] = (float)rgb[0];
			hMaterial.diffuseColor[1] = (float)rgb[1];
			hMaterial.diffuseColor[2] = (float)rgb[2];
			hMaterial.diffuseColor[3] = 1.0f;
		}

		MPlug diffusePlug = MFnDependencyNode(plug.node()).findPlug("diffuse", &res);
		if (res == MStatus::kSuccess)
		{
			float diffExp;
			diffusePlug.getValue(diffExp);

			hMaterial.diffuseColor[0] *= (float)diffExp;
			hMaterial.diffuseColor[1] *= (float)diffExp;
			hMaterial.diffuseColor[2] *= (float)diffExp;

			MGlobal::displayInfo(MString("Diffuse Color: ") +
				MString("R: ") + hMaterial.diffuseColor[0] + MString(" ") +
				MString("G: ") + hMaterial.diffuseColor[1] + MString(" ") +
				MString("B: ") + hMaterial.diffuseColor[2]);
		}
	}

	MPlug ambientPlug = MFnDependencyNode(plug.node()).findPlug("ambientColor", &res);
	if (res == MStatus::kSuccess)
	{
		ambientPlug.getValue(tempData);
		MFnNumericData ambientData(tempData);

		ambientData.getData(rgb[0], rgb[1], rgb[2]);

		hMaterial.ambient[0] = rgb[0];
		hMaterial.ambient[1] = rgb[1];
		hMaterial.ambient[2] = rgb[2];

		MGlobal::displayInfo(MString("Ambient: ") +
			MString("R: ") + rgb[0] + MString(" ") +
			MString("G: ") + rgb[1] + MString(" ") +
			MString("B: ") + rgb[2]);
	}

	if (plug.node().hasFn(MFn::kPhong) || plug.node().hasFn(MFn::kBlinn))
	{
		MPlug specularPlug = MFnDependencyNode(plug.node()).findPlug("specularColor", &res);
		if (res == MStatus::kSuccess)
		{
			specularPlug.getValue(tempData);
			MFnNumericData specularData(tempData);

			specularData.getData(rgb[0], rgb[1], rgb[2]);

			hMaterial.specular[0] = rgb[0];
			hMaterial.specular[1] = rgb[1];
			hMaterial.specular[2] = rgb[2];

			MGlobal::displayInfo(MString("Specular: ") +
				MString("R: ") + rgb[0] + MString(" ") +
				MString("G: ") + rgb[1] + MString(" ") +
				MString("B: ") + rgb[2]);
		}
	}

	/*The material is kLambert, assign zero to specular.*/
	else if (plug.node().hasFn(MFn::kLambert))
	{
		/*Set specular to zero in RGB.*/
		MGlobal::displayInfo(MString("No specular for kLambert!"));

		hMaterial.specular[0] = 0;
		hMaterial.specular[1] = 0;
		hMaterial.specular[2] = 0;
	}

	fMakeMaterialMessage(hMaterial);
}

void fOnMaterialChange(MNodeMessage::AttributeMessage attrMessage, MPlug& plug, MPlug& otherPlug, void* clientData)
{
	MStatus res;

	MObject set = fFindMaterialConnected(plug.node());

	MObject shaderNode = fFindShader(set);

	MGlobal::displayInfo(shaderNode.apiTypeStr());
			
	if (shaderNode != MObject::kNullObj)
	{
		MCallbackId id = MNodeMessage::addAttributeChangedCallback(shaderNode, fOnMaterialAttrChanges, clientData, &res);
				
		if (res == MStatus::kSuccess)
		{
			ids.append(id);
		}
	}
}

void fLoadActiveMaterial(MObject& shaderNode, MFnMesh& mesh)
{
	MStatus res;
	MObject tempData;
	float rgb[3];

	hMaterialHeader hMaterial;

	hMaterial.materialName = MFnDependencyNode(shaderNode).name().asChar();
	hMaterial.materialNameLength = MFnDependencyNode(shaderNode).name().length();

	hMaterial.connectedMeshName = mesh.name().asChar();
	hMaterial.connectedMeshNameLength = mesh.name().length();

	MPlug colorPlug = MFnDependencyNode(shaderNode).findPlug("color", &res);
	if (res == MStatus::kSuccess)
	{
		if (res == MStatus::kSuccess)
		{
			colorPlug.getValue(tempData);
			MFnNumericData colorData(tempData);

			colorData.getData(rgb[0], rgb[1], rgb[2]);

			hMaterial.diffuseColor[0] = (float)rgb[0];
			hMaterial.diffuseColor[1] = (float)rgb[1];
			hMaterial.diffuseColor[2] = (float)rgb[2];
			hMaterial.diffuseColor[3] = 1.0f;
		}

		MPlug diffusePlug = MFnDependencyNode(shaderNode).findPlug("diffuse", &res);
		if (res == MStatus::kSuccess)
		{
			float diffExp;
			diffusePlug.getValue(diffExp);

			hMaterial.diffuseColor[0] *= (float)diffExp;
			hMaterial.diffuseColor[1] *= (float)diffExp;
			hMaterial.diffuseColor[2] *= (float)diffExp;

			MGlobal::displayInfo(MString("Diffuse Color: ") +
				MString("R: ") + hMaterial.diffuseColor[0] + MString(" ") +
				MString("G: ") + hMaterial.diffuseColor[1] + MString(" ") +
				MString("B: ") + hMaterial.diffuseColor[2]);
		}
	}

	MPlug ambientPlug = MFnDependencyNode(shaderNode).findPlug("ambientColor", &res);
	if (res == MStatus::kSuccess)
	{
		ambientPlug.getValue(tempData);
		MFnNumericData ambientData(tempData);

		ambientData.getData(rgb[0], rgb[1], rgb[2]);

		hMaterial.ambient[0] = rgb[0];
		hMaterial.ambient[1] = rgb[1];
		hMaterial.ambient[2] = rgb[2];

		MGlobal::displayInfo(MString("Ambient: ") +
			MString("R: ") + rgb[0] + MString(" ") +
			MString("G: ") + rgb[1] + MString(" ") +
			MString("B: ") + rgb[2]);
	}

	if (shaderNode.hasFn(MFn::kPhong) || shaderNode.hasFn(MFn::kBlinn))
	{
		MPlug specularPlug = MFnDependencyNode(shaderNode).findPlug("specularColor", &res);
		if (res == MStatus::kSuccess)
		{
			specularPlug.getValue(tempData);
			MFnNumericData specularData(tempData);

			specularData.getData(rgb[0], rgb[1], rgb[2]);

			hMaterial.specular[0] = rgb[0];
			hMaterial.specular[1] = rgb[1];
			hMaterial.specular[2] = rgb[2];

			MGlobal::displayInfo(MString("Specular: ") +
				MString("R: ") + rgb[0] + MString(" ") +
				MString("G: ") + rgb[1] + MString(" ") +
				MString("B: ") + rgb[2]);
		}
	}

	/*The material is kLambert, assign zero to specular.*/
	else if (shaderNode.hasFn(MFn::kLambert))
	{
		/*Set specular to zero in RGB.*/
		MGlobal::displayInfo(MString("No specular for kLambert!"));

		hMaterial.specular[0] = 0;
		hMaterial.specular[1] = 0;
		hMaterial.specular[2] = 0;
	}

	fMakeMaterialMessage(hMaterial);
}

void fGetMeshMaterial(MObject& node)
{
	MStatus res;

	if (node.hasFn(MFn::kMesh))
	{
		MFnMesh meshFn(node);
		/*Register callback when a mesh changes material.*/
		/*MCallbackId id = MNodeMessage::addAttributeChangedCallback(node, fOnMaterialChange, &node, &res);
		if (res == MStatus::kSuccess)
		{
			ids.append(id);
		}*/

		MObject set = fFindMaterialConnected(node);

		MObject shaderNode = fFindShader(set);

		fLoadActiveMaterial(shaderNode, meshFn);

		/*if (shaderNode != MObject::kNullObj)
		{
			MCallbackId id = MNodeMessage::addAttributeChangedCallback(shaderNode, fOnMaterialAttrChanges, &node, &res);

			if (res == MStatus::kSuccess)
			{
				ids.append(id);
			}
		}*/
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

	MFnDependencyNode depFn(node, &res);
	if (res == MStatus::kSuccess)
	{

	}
}

void fLoadTransform(MObject& obj, bool isFromQueue)
{
	MStatus res;
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
		transMat.getRotation(rot, rotOrder);
		transMat.getRotationQuaternion(rot[0], rot[1], rot[2], rot[3], MSpace::kWorld);
		transMat.getScale(scale, MSpace::kWorld);

		hTransformHeader hTrans;

		std::copy(trans, trans + 3, hTrans.trans);
		std::copy(rot, rot + 4, hTrans.rot);
		std::copy(scale, scale + 3, hTrans.scale);

		fMakeTransformMessage(obj, hTrans);
	}
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
					fLoadTransform(obj, true);
				}
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
			{	
				fCameraAddCbks(node, clientData);
			}
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

	if ((res != MStatus::kSuccess && nt == eNodeType::transformNode) ||
		(res != MStatus::kSuccess && nt == eNodeType::cameraNode) ||
		(res != MStatus::kSuccess && nt == eNodeType::meshNode))
		gObjQueue.push(node);

    if (clientData)
    {
		if (*(int*)clientData == 2)
		{
			gObjQueue.pop();
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
	gTransformUpdateTimer += gDt30Fps;

	if (gObjQueue.size() > 0)
	{
		int type = 2;
		fOnNodeCreate(gObjQueue.front(), &type);
	}
}

void fMakeMeshMessage(MObject obj, bool isFromQueue)
{
	MGlobal::displayInfo(MString("Mesh message created!"));
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
    meshH.meshNameLen = mesh.name().length() + 1;
    meshH.vertexCount = meshVertices.size();
    
    /*Now put the mainHeader first, then the meshHeader, then the vertices*/
    size_t mainHMem = sizeof(mainH);
    size_t meshHMem = sizeof(meshH);
    size_t meshVertexMem = sizeof(sBuiltVertex);

    int totPackageSize = mainHMem + meshHMem + meshH.meshNameLen + meshH.vertexCount * meshVertexMem;

    /*
    In the initialize-function, maybe have an array of msg. Pre-sized so that there's a total of 4 messages
    check if a message is "active", if it's unactive, i.e already read, the queued thing may memcpy into it.
    */
	mtx.lock();
    memcpy(msg, (void*)&mainH, (size_t)mainHMem);
    memcpy(msg + mainHMem, (void*)&meshH, (size_t)meshHMem);
    memcpy(msg + mainHMem + meshHMem, (void*)meshH.meshName, meshH.meshNameLen-1);
	// Insert \0
	*(char*)(msg + mainHMem + meshHMem + meshH.meshNameLen-1) = '\0';
    //memcpy(msg + mainHMem + meshHMem + meshH.meshNameLen, (void*)meshH.prntTransName, meshH.prntTransNameLen);
    memcpy(msg + mainHMem + meshHMem + meshH.meshNameLen, meshVertices.data(), meshVertices.size() * meshVertexMem);

    producer->runProducer(gCb, (char*)msg, totPackageSize);
	mtx.unlock();

	fGetMeshMaterial(obj);
}

void fMakeCameraMessage(hCameraHeader& gCam)
{
	MGlobal::displayInfo("CameraMsg!");
	hMainHeader hMainHead;
	size_t mainMem = sizeof(hMainHead);
	size_t camMem = sizeof(hCameraHeader);

	hMainHead.cameraCount = 1;
	gCam.cameraNameLength++;
	int totalSize = mainMem + camMem + gCam.cameraNameLength;

	mtx.lock();
	memcpy(msg, (void*)&hMainHead, mainMem);
	memcpy(msg + mainMem, (void*)&gCam, camMem);
	memcpy(msg + mainMem + camMem, (void*)gCam.cameraName, gCam.cameraNameLength-1);
	*(char*)(msg + mainMem + camMem + gCam.cameraNameLength-1) = '\0';

	producer->runProducer(gCb, (char*)msg, totalSize);
	mtx.unlock();
}

void fMakeTransformMessage(MObject obj, hTransformHeader transH)
{ /*use mtx.lock() before the memcpys', and mtx.unlock() right after producer.runProducer()*/
    MStatus res;
    MFnTransform trans(obj);
	
	//MObject parentObj;
	hMainHeader mainH;
	mainH.transformCount = 1;
	bool hasMeshChild = false;
	bool foundChild = false;
	
	MObject childObj;
	/*Getting the first child for mesh, camera and light.*/
	for (unsigned int i = 0; i < trans.childCount(); i++)
	{
		childObj = trans.child(i, &res);

		if (childObj.hasFn(MFn::kMesh))
		{
			hasMeshChild = true;
			MFnMesh meshFn(childObj, &res);
			if (res == MStatus::kSuccess)
			{
				/*Can you copy pointers to maya memory?*/
				transH.childName = meshFn.name().asChar();
				transH.childNameLength = meshFn.name().length() + 1;

				foundChild = true;
				break;
			}
			MGlobal::displayInfo(MString("ERROR: ") + MString(res.errorString()));
		}
	}
	
	if (foundChild)
	{
		mtx.lock();
		memcpy(msg, &mainH, sizeof(hMainHeader));
		memcpy(msg + sizeof(hMainHeader), &transH, sizeof(hTransformHeader));
		//Copying the name to the shared memory
		memcpy(msg + sizeof(hMainHeader) + sizeof(hTransformHeader), transH.childName, transH.childNameLength-1);
		//Making the name null-terminated
		*(char*)(msg + sizeof(hMainHeader) + sizeof(hTransformHeader) + transH.childNameLength - 1) = '\0';
		producer->runProducer(
			gCb,
			msg,
			sizeof(hMainHeader) +
			sizeof(hTransformHeader) +
			transH.childNameLength
		);
		MGlobal::displayInfo("Trans message made");
		mtx.unlock();
	}
	else if(hasMeshChild && !foundChild)
	{
		gObjQueue.push(obj);
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
		int aids = 1337;
        while (!nodeIt.isDone())
        {
			/*
			If this function is called by iterateScene, 
			save all transforms in a queue that you 
			loop through after this node iteration is done. 
			That way all of the possible children
			(except transforms) are present in the scene.
			*/
			fOnNodeCreate(nodeIt.currentItem(), &aids);
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
			roh.nameLength = std::strlen(mesh.name().asChar()) + 1;
			memcpy(msg, &mainH, sizeof(hMainHeader));
			/*First removed object header*/
			memcpy(msg + sizeof(hMainHeader), &roh, sizeof(hRemovedObjectHeader));
			/*Then the name*/
			memcpy(msg + sizeof(hMainHeader) + sizeof(hRemovedObjectHeader), mesh.name().asChar(), roh.nameLength-1);
			/*Then add the '\0'*/
			*(char*)(msg + sizeof(hMainHeader) + sizeof(hRemovedObjectHeader) + roh.nameLength - 1) = '\0';
			MGlobal::displayInfo(MString("Deleted mesh: ") + MString(mesh.name()));
		}
	}
	if (res == MStatus::kSuccess)
	{
		producer->runProducer(gCb, msg, sizeof(hMainHeader) + sizeof(hRemovedObjectHeader) + roh.nameLength);
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

	gCb = new circularBuffer;
	gCb->initCircBuffer(TEXT("MessageBuffer"), BUFFERSIZE, 0, CHUNKSIZE, TEXT("VarBuffer"));

    float oldTime = gClockTime;
    gClockticks = clock();
    gClockTime = gClockticks / CLOCKS_PER_SEC;
    float dt = gClockTime - oldTime;

    gMeshUpdateTimer = 0;
	gTransformUpdateTimer = 0;

	msg = new char[MAXMSGSIZE];

	int chunkSize = CHUNKSIZE;
	LPCWSTR varBuffName = TEXT("VarBuffer");
	producer = new Producer(1, chunkSize, varBuffName);

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

	delete gCb;
	delete producer;
	delete[] msg;

	return MS::kSuccess;
}
