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

void fLoadMaterial(MObject& node);
void fOnMaterialAttrChanges(MNodeMessage::AttributeMessage attrMessage, MPlug& plug, MPlug& otherPlug, void* clientData);

void fTransAddCbks(MObject& node, void* clientData);
circularBuffer* gCb;
Producer* producer;
Mutex mtx;

float gClockTime;
float gClockticks;
float gMeshUpdateTimer;
float gTransformUpdateTimer;
float gDt30Fps;

#define BUFFERSIZE 20<<20
#define MAXMSGSIZE 5<<20
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

MCallbackIdArray ids;
std::queue<MObject> gObjQueue;
std::queue<MObject> gHierarchyQueue;
std::queue<MObject> gMeshDelayQueue;

//void* HelloWorld::creator() { 
//	return new HelloWorld;
//};
//MStatus HelloWorld::doIt(const MArgList& argList)
//{
//	MGlobal::displayInfo("Hello World!");
//	return MS::kSuccess;
//};

/*Saves the names of the object-children of this transform's transform-children*/
void fFindChildrenOfTransform(MFnTransform& trans, hHierarchyHeader& coh, std::vector<hChildNodeNameHeader>& conh)
{
	/*
	Save the name of the first direct non-transform child of this transform as the child
	of the "root" transform
	*/
	hChildNodeNameHeader lconh;

	MStatus res;

	if (res == MStatus::kSuccess)
	{
		MObject childObj;
		/*
		Looping through the children, finding a transform, and then looping though it's children
		in search of a mesh node, and if found adding it to the conh vector.
		*/
		for (unsigned int i = 0; i < trans.childCount(); i++)
		{
			childObj = trans.child(i, &res);
			if (res == MStatus::kSuccess)
			{
				if (childObj.hasFn(MFn::kTransform))
				{
					MFnTransform tranfn(childObj, &res);
					if (res == MStatus::kSuccess)
					{
						MObject childChildObj;
						for (int j = 0; j < tranfn.childCount(); j++)
						{
							childChildObj = tranfn.child(j, &res);
							if (res == MStatus::kSuccess)
							{
								coh.childNodeCount++;
								if (childChildObj.hasFn(MFn::kMesh))
								{
									MFnMesh meshFn(childChildObj, &res);
									if (res == MStatus::kSuccess)
									{
										lconh.objName = meshFn.name().asChar();
										lconh.objNameLength = strlen(meshFn.name().asChar()) + 1;
										conh.push_back(lconh);
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

/*
Uses the obj to make a transFn object that in turn finds and stores the object-children of it's
transform children. The obj obviously needs to have a function set for a transform.

Use this function once per transformation node in "onNodeCreate".
Then use this function in DAG-callbacks.
The transformation node must be connected to a mesh!
*/
void fMakeHierarchyMessage(MObject obj)
{
	bool hasMeshFn = false;
	bool foundMeshName = false;
	MStatus res;
	hMainHeader mainH;
	mainH.hierarchyCount = 1;
	MFnTransform trans(obj, &res);
	hHierarchyHeader coh;

	for (int o = 0; o < trans.childCount(); o++)
	{
		MObject childObj = trans.child(o);
		if (childObj.hasFn(MFn::kMesh))
		{
			hasMeshFn = true;
			MFnMesh childMesh(childObj, &res);
			if (res == MStatus::kSuccess)
			{
				coh.parentNodeName = childMesh.name().asChar();
				coh.parentNodeNameLength = strlen(childMesh.name().asChar()) + 1;
				foundMeshName = true;
			}
		}
	}

	if (foundMeshName)
	{
		std::vector<hChildNodeNameHeader> conh;

		fFindChildrenOfTransform(trans, coh, conh);

		mtx.lock();

		memcpy(msg, &mainH, sizeof(hMainHeader));
		memcpy(msg + sizeof(hMainHeader), &coh, sizeof(hHierarchyHeader));
		memcpy(msg + sizeof(hMainHeader) + sizeof(hHierarchyHeader), coh.parentNodeName, coh.parentNodeNameLength - 1);
		*(char*)(msg + sizeof(hMainHeader) + sizeof(hHierarchyHeader) + coh.parentNodeNameLength - 1) = '\0';
		int pastSize = 0;
		for (int i = 0; i < coh.childNodeCount; i++)
		{
			/*Copying te childnameheader into the message, so that we get the name length in the engine*/
			memcpy(msg + sizeof(hMainHeader) + sizeof(hHierarchyHeader) + coh.parentNodeNameLength + pastSize, &conh[i], sizeof(hChildNodeNameHeader));
			/*Copying the name to the spot after the childnameheader*/
			memcpy(msg + sizeof(hMainHeader) + sizeof(hHierarchyHeader) + coh.parentNodeNameLength + sizeof(hChildNodeNameHeader) + pastSize, conh[i].objName, conh[i].objNameLength - 1);
			*(char*)(msg + sizeof(hMainHeader) + sizeof(hHierarchyHeader) + coh.parentNodeNameLength + +sizeof(hChildNodeNameHeader) + pastSize + conh[i].objNameLength - 1) = '\0';
			pastSize += sizeof(hChildNodeNameHeader) + conh[i].objNameLength;
		}
		MGlobal::displayInfo("Hierarchy Message Made!");
		producer->runProducer(gCb, msg, sizeof(hMainHeader) + sizeof(hHierarchyHeader) + coh.parentNodeNameLength + pastSize);

		mtx.unlock();
	}
	else if (hasMeshFn && !foundMeshName)
	{
		/*Add this hierarchy to the queue...*/
		gHierarchyQueue.push(obj);
	}
}

void fLoadMesh(MFnMesh& mesh, bool isFromQueue, std::vector<sBuiltVertex> &allVert)
{
    MStatus res;
    MIntArray triCnt;
    MIntArray triVert; //vertex Ids for each tri vertex
	
	MIntArray vertexCount;

	mesh.getTriangles(triCnt, triVert);

	allVert.resize(triVert.length());

	MVector tempVec;
	MPoint tempPoint;
	/*In this loop we get both the normals and the points*/
    for (int i = 0; i < allVert.size(); i++)
    {
		/*
		Since we get the normals based on the control point list, and not
		by per-triangle-vertex, the normal we get is always the average
		of all the per-triangle-normals connected to that control point
		*/
        mesh.getVertexNormal(triVert[i], tempVec, MSpace::kObject);

		/*
		This is the ideal usage scenario for getPoint, since the 
		triVert-list contains indexes right into the control-point-list
		*/
        mesh.getPoint(triVert[i], tempPoint, MSpace::kObject);
		
        allVert[i].pnt.x = tempPoint.x;
        allVert[i].pnt.y = tempPoint.y;
        allVert[i].pnt.z = tempPoint.z;

        allVert[i].nor.x = tempVec.x;
        allVert[i].nor.y = tempVec.y;
        allVert[i].nor.z = tempVec.z;
    }

	MStringArray uvSetNames;
	mesh.getUVSetNames(uvSetNames);

	/*Get the UV list*/
	MFloatArray u;
	MFloatArray v;
	mesh.getUVs(u, v);

	/*Get the UV-index per polygon*/
	MIntArray uvCount;
	MIntArray uvIDs;
	mesh.getAssignedUVs(uvCount, uvIDs, &uvSetNames[0]);

	/*
	Get the polygon-vertex-index per triangle vertex.
	(for cube)
	UVList(14) <-- udIDs(24) <-- triIndices(36)
	*/
	MIntArray triCount;
	MIntArray triIndices;
	mesh.getTriangleOffsets(triCount, triIndices);
	
	sUV uv;
	/*In this loop we get UVs. Looping per polygon vertex*/
	for (int i = 0; i < allVert.size(); i++)
	{
		uv.u = u[uvIDs[triIndices[i]]];
		uv.v = v[uvIDs[triIndices[i]]];
		allVert[i].uv = uv;
	}
}

/*
When the actual number of vertices on a mesh changes, you have to send a message
saying that a "new" mesh with the same name has been made.

When you delete a face, you gotta call a function like this...
*/
void fOnMeshTopoChange(MObject &node, void *clientData)
{
	MGlobal::displayInfo("TOPOLOGY!");
    MStatus res;
    MFnMesh meshFn(node, &res);
    if (res == MStatus::kSuccess)
    {
		MGlobal::displayInfo("TOPOLOGY2!");
		//fMakeMeshMessage(node, false);
    }
}

void fOnMeshAttrChange(MNodeMessage::AttributeMessage attrMessage, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	//Maybe this is how you compare attrMessage? THis is how you compare messages... Odd
	/*When you click on a face with move tool, you change an attribute named "uvPivot" */
	MGlobal::displayInfo("I GOT CALLED! " + plug.name());

	MStatus res;

	if (attrMessage & MNodeMessage::AttributeMessage::kAttributeSet)
	{
		MObject temp = plug.node();

		MFnMesh meshFn(temp, &res);
		
		if (res == MStatus::kSuccess)
		{
			MGlobal::displayInfo("I GOT CALLED AND THEN MADE A MESHMESSAGE! " + plug.name());

			gMeshDelayQueue.push(temp);
			//fMakeMeshMessage(temp, false);
		}
	}
	/*When a mesh changes a material, we are only interested in when connections are made and broken.*/
	/*Gets in here when this mesh's material gets changed. I.E the connection to the old material is broken, OR when the connection to the new material is made*/
	else if (attrMessage & MNodeMessage::AttributeMessage::kConnectionMade | MNodeMessage::AttributeMessage::kConnectionBroken)
	{
		MFnMesh meshFn(plug.node(), &res);
		if (res == MStatus::kSuccess)
		{
			/*
			Only load the material change on the mesh if the plug name is ".instObjGroups[0]".
			If we had multiple materials on a mesh, this would only work for the first material.
			*/
			
			if (plug.name() == MString(meshFn.name() + ".instObjGroups[0]"))
			{
				MGlobal::displayInfo("I GOT CALLED AND MADE A MATERIAL" + plug.name());
				fLoadMaterial(plug.node());
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

void fOnGeometryDelete(MObject &node, MDGModifier &modifier, void *clientData)
{
	MGlobal::displayInfo("GEOMETRY DELETED!");
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

		/*
		Triggers on extrudes. Extrudes cannot be displayed because a change of material is necessary 
		for the mesh to be displayed in Gameplay3d.
		*/
        id = MPolyMessage::addPolyTopologyChangedCallback(node, fOnMeshTopoChange, NULL, &res);
        if (res == MStatus::kSuccess)
        {
			MGlobal::displayInfo("Polytopo SUcess!");
			ids.append(id);
		}

		id = MNodeMessage::addNodeAboutToDeleteCallback(node, fOnGeometryDelete, NULL, &res);
		if (res == MStatus::kSuccess)
		{
			MGlobal::displayInfo("Delete SUcess!");
			ids.append(id);
		}
	}
}

void fLoadCamera()
{
	MStatus res;
	MDagPath cameraPath;
	M3dView activeView = M3dView::active3dView();
	hCameraHeader hCam;
	
	/*Find the MDagPath to the camera connected to the active viewport.*/
	if(activeView.getCamera(cameraPath))
	{
		MFnCamera camFn(cameraPath.node(), &res);
		if (res == MStatus::kSuccess)
		{
			/*Obtain the projection matrix from the first loaded active camera.*/
			MFloatMatrix projMatrix = camFn.projectionMatrix();

			memcpy(hCam.projMatrix, &camFn.projectionMatrix(), sizeof(MFloatMatrix));
			/*Obtain the camera's name & length for ID usage in Gameplay3D scene.*/
			hCam.cameraName = camFn.name().asChar();
			hCam.cameraNameLength = camFn.name().length();

			MFnTransform fnTransform(camFn.parent(0), &res);
			if (res == MStatus::kSuccess)
			{
				/*Obtain the camera's transform parent's translation, scale & Quaternion.*/
				MVector tempTrans = fnTransform.getTranslation(MSpace::kTransform, &res);
				double camTrans[3];
				tempTrans.get(camTrans);
	
				double camScale[3];
				fnTransform.getScale(camScale);
				/*NOTE: Camera's rotation quaternion should ways be obtained in it's transform space.*/
				double camQuat[4];
				fnTransform.getRotationQuaternion(camQuat[0], camQuat[1], camQuat[2], camQuat[3], MSpace::kTransform);

				/*std::copy, especially used for simply copying from double to float.*/
				std::copy(camTrans, camTrans + 3, hCam.trans);
				std::copy(camScale, camScale + 3, hCam.scale);
				std::copy(camQuat, camQuat + 4, hCam.rot);
			}
		}
	}
	/*Camera data is obtained, send a camera message.*/
	fMakeCameraMessage(hCam);
}

void fCameraChanged(const MString &str, void* clientData)
{
	//A static variable is only initialized once. So after the first execution, this line gets ignored by the compiler.
	static MFloatMatrix oldProjMatrix;
	static MMatrix oldMat;
	hCameraHeader hCam;
	MStatus res;

	M3dView activeView = M3dView::active3dView();

	/*Obtain the current name of the panel and compare this with the
	desired panels we want to obtain camera information from.*/
	MString panelName = MGlobal::executeCommandStringResult("getPanel -wf");
	if (strcmp(panelName.asChar(), str.asChar()) == 0)
	{
		MDagPath cameraPath;
		if (activeView.getCamera(cameraPath))
		{
			MFnCamera camFn(cameraPath.node(), &res);

			if (res == MStatus::kSuccess)
			{
				MFnTransform fnTransform(camFn.parent(0), &res);

				MMatrix newMat = fnTransform.transformationMatrix();

				MFloatMatrix projMatrix = camFn.projectionMatrix();

				memcpy(hCam.projMatrix, &camFn.projectionMatrix(), sizeof(MFloatMatrix));
				hCam.cameraName = camFn.name().asChar();
				hCam.cameraNameLength = camFn.name().length();

				/*This callback also registers if the mouse is hovering over objects in the viewport,
				which is not wanted. To only send cameras message when camera changes, solution is to
				memory compare the old values with the new values of both the proj and transform matrices.*/
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
					/*Camera data changes is obtained, send a camera message.*/
					fMakeCameraMessage(hCam);
				}
			}
		}
	}
}

void fCameraAddCbks(MObject& node, void* clientData)
{
	MStatus res;
	MCallbackId id;
	
	/*Collect changes when active camera changes.*/
	id = MUiMessage::add3dViewPreRenderMsgCallback(MString("modelPanel4"), fCameraChanged, NULL, &res);
	/*Only registering changes for modelPanel4, actually registers for all camera viewports in Maya.*/
	if (res == MStatus::kSuccess)
	{
		MGlobal::displayInfo("cameraChanged success!");
		ids.append(id);
	}
}

MObject fFindShadingGroup(MObject node)
{
	MStatus res;

	if (node.hasFn(MFn::kMesh))
	{
		MFnMesh fnMesh(node, &res);

		MDagPath dp;

		fnMesh.getPath(dp);
		/*Find the instance number from the kMesh to query.*/
		unsigned int instNum = dp.instanceNumber();

		/*Get the set connected to the specified instance of kMesh.*/
		MObjectArray sets, comps;
		if (!fnMesh.getConnectedSetsAndMembers(instNum, sets, comps, true))
			MGlobal::displayInfo("FAILED!");

		/*If the set have a length, return the first set in the MObjectArray.*/
		if (sets.length())
		{
			MObject set = sets[0];
			return set;
		}
	}
	/*If there is nothing to find in the MObjectArray for sets, return a null Obj.*/
	return MObject::kNullObj;
}

MObject fFindShader(MObject& setNode)
{
	/*With the set, which is a shading group, we can now find 
	the surfaceShader plug, from the SG Node.*/
	MFnDependencyNode fnNode(setNode);
	MPlug shaderPlug = fnNode.findPlug("surfaceShader");

	/*If the shader plug contains a surface Shader, we obtain this shader node
	from the surface shader plug itself and return it.*/
	if (!shaderPlug.isNull())
	{
		MPlugArray connectedPlugs;
		shaderPlug.connectedTo(connectedPlugs, true, false);

		if (connectedPlugs.length() != 1)
			MGlobal::displayInfo("Error getting the shader...");

		else
			return connectedPlugs[0].node();
	}
	/*No surface shader was found, return a null obj.*/
	return MObject::kNullObj;
}

void fMakeMaterialMessage(hMaterialHeader hMaterial)
{
	MGlobal::displayInfo("MaterialMsg!");

	hMainHeader HMainHead;
	size_t mainMem = sizeof(hMainHeader);
	size_t materialMem = sizeof(hMaterialHeader);

	HMainHead.materialCount = 1;

	hMaterial.numConnectedMeshes = hMaterial.connectMeshList.size();

	/*Depending if there is a texture or not assigned to material, the message
	is sent in two different ways, altered by the texture flag.*/
	if (hMaterial.isTexture == false)
	{
		int totalSize = mainMem + materialMem;

		mtx.lock();
		memcpy(msg, (void*)&HMainHead, mainMem);
		memcpy(msg + mainMem, (void*)&hMaterial, materialMem);

		int prevSize = 0;
		for (int i = 0; i < hMaterial.connectMeshList.size(); i++)
		{
			memcpy(msg + mainMem + materialMem + prevSize, &hMaterial.connectMeshList[i], sizeof(hMeshConnectMaterialHeader));
			memcpy(msg + mainMem + materialMem + sizeof(hMeshConnectMaterialHeader) + prevSize, (char*)hMaterial.connectMeshList[i].connectMeshName, hMaterial.connectMeshList[i].connectMeshNameLength - 1);
			*(char*)(msg + mainMem + materialMem + sizeof(hMeshConnectMaterialHeader) + prevSize + hMaterial.connectMeshList[i].connectMeshNameLength - 1) = '\0';

			prevSize += sizeof(hMeshConnectMaterialHeader) + hMaterial.connectMeshList[i].connectMeshNameLength;

			totalSize += sizeof(hMeshConnectMaterialHeader) + hMaterial.connectMeshList[i].connectMeshNameLength;
		}

		producer->runProducer(gCb, msg, totalSize);
		mtx.unlock();
	}

	else
	{
		int totalSize = mainMem + materialMem + hMaterial.colorMapLength;

		mtx.lock();
		memcpy(msg, (void*)&HMainHead, mainMem);
		memcpy(msg + mainMem, (void*)&hMaterial, materialMem);
		memcpy(msg + mainMem + materialMem, (void*)hMaterial.colorMap, hMaterial.colorMapLength - 1);
		*(char*)(msg + mainMem + materialMem + hMaterial.colorMapLength - 1) = '\0';

		int prevSize = 0;
		for (int i = 0; i < hMaterial.connectMeshList.size(); i++)
		{
			memcpy(msg + mainMem + materialMem + hMaterial.colorMapLength + prevSize, &hMaterial.connectMeshList[i], sizeof(hMeshConnectMaterialHeader));
			memcpy(msg + mainMem + materialMem + hMaterial.colorMapLength + sizeof(hMeshConnectMaterialHeader) + prevSize, (char*)hMaterial.connectMeshList[i].connectMeshName, hMaterial.connectMeshList[i].connectMeshNameLength - 1);
			*(char*)(msg + mainMem + materialMem + hMaterial.colorMapLength + sizeof(hMeshConnectMaterialHeader) + prevSize + hMaterial.connectMeshList[i].connectMeshNameLength - 1) = '\0';

			prevSize += sizeof(hMeshConnectMaterialHeader) + hMaterial.connectMeshList[i].connectMeshNameLength;

			totalSize += sizeof(hMeshConnectMaterialHeader) + hMaterial.connectMeshList[i].connectMeshNameLength;
		}

		producer->runProducer(gCb, msg, totalSize);
		mtx.unlock();
	}
}

void fFindMeshConnectedToMaterial(MObject shaderNode, hMaterialHeader& hMaterial)
{
	MStatus res;
	hMeshConnectMaterialHeader hMeshMaterial;

	/*Find the "outColor" plug, to go from the shader to the connected shading group.*/
	MPlugArray connectionsToShader;
	MPlug outColorPlug = MFnDependencyNode(shaderNode).findPlug("outColor", &res);
	outColorPlug.connectedTo(connectionsToShader, false, true, &res);

	/*If there are shader groups "kShadingEngines", loop through these.*/
	if (connectionsToShader.length())
	{
		for (int connectIndex = 0; connectIndex < connectionsToShader.length(); connectIndex++)
		{
			if (connectionsToShader[connectIndex].node().hasFn(MFn::kShadingEngine))
			{
				MFnDependencyNode shadingNode;
				shadingNode.setObject(connectionsToShader[connectIndex].node());
				/*There is always a kShadingEngine called "initialParticleSE", this we 
				check for and skip it. No need to process this.*/
				if (shadingNode.name() == "initialParticleSE")
					continue;

				/*Find the "dagSetMembers" plug in the SG, which connects to one or several meshes.*/
				MPlug dagSetMemberPlug = shadingNode.findPlug("dagSetMembers", &res);
				MPlugArray instObjGroups;

				/*If there are Dag Set Members connected to the SG, loop through and obtain the mesh information.*/
				for (int dagSetIndex = 0; dagSetIndex < dagSetMemberPlug.numConnectedElements(); dagSetIndex++)
				{
					dagSetMemberPlug[dagSetIndex].connectedTo(instObjGroups, true, false, &res);
					/*If there is a mesh in this dag set Member array index, process it.*/
					if (instObjGroups.length())
					{
						for (int meshIndex = 0; meshIndex < instObjGroups.length(); meshIndex++)
						{
							MFnMesh mesh(instObjGroups[meshIndex].node());
							/*Strangely there is a shape called "shaderBallGeomShape!", if we find it
							then we simply skip it. No need to process this strange shape.*/
							if (mesh.name() == "shaderBallGeomShape1")
								continue;

							MGlobal::displayInfo(mesh.name());
							/*Obtain the connected mesh's name and length.*/
							hMeshMaterial.connectMeshName = mesh.name().asChar();
							hMeshMaterial.connectMeshNameLength = mesh.name().length() + 1;
							/*All meshes connected to this shading group is put in a "special" connect mesh list.*/
							hMaterial.connectMeshList.push_back(hMeshMaterial);
						}
					}
				}
			}
		}
	}
}

void fOnMaterialAttrChanges(MNodeMessage::AttributeMessage attrMessage, MPlug& plug, MPlug& otherPlug, void* clientData)
{
	/*Important to check if the message is when a attribute is set in the material. Also
	the plug nodes are only allowed to be lambert, blinn and phong.*/
	if (attrMessage & MNodeMessage::kAttributeSet || plug.node().hasFn(MFn::kLambert)
		|| plug.node().hasFn(MFn::kBlinn) || plug.node().hasFn(MFn::kPhong))
	{
		MStatus res;
		MObject tempData;
		float rgb[3];

		hMaterialHeader hMaterial;

		/*Find one or several meshes connected to this material.*/
		fFindMeshConnectedToMaterial(plug.node(), hMaterial);

		/*Find the "color" plug and obtain the RGB values from this. NOTE: This is not diffuse color.*/
		MPlug colorPlug = MFnDependencyNode(plug.node()).findPlug("color", &res);
		if (res == MStatus::kSuccess)
		{
			if (res == MStatus::kSuccess)
			{
				/*Obtain the RGB values and store them and the alpha value.*/
				colorPlug.getValue(tempData);
				MFnNumericData colorData(tempData);

				colorData.getData(rgb[0], rgb[1], rgb[2]);

				hMaterial.diffuseColor[0] = (float)rgb[0];
				hMaterial.diffuseColor[1] = (float)rgb[1];
				hMaterial.diffuseColor[2] = (float)rgb[2];
				hMaterial.diffuseColor[3] = 1.0f;
			}
			/*Find the "diffuse" plug. This is how the light scatters on the material.*/
			MPlug diffusePlug = MFnDependencyNode(plug.node()).findPlug("diffuse", &res);
			if (res == MStatus::kSuccess)
			{
				float diffExp;
				diffusePlug.getValue(diffExp);
				/*NOTE: To obtain the "diffuse" color, it's required to multiply the
				color earlier obtained with the diffuse float exponent*/
				hMaterial.diffuseColor[0] *= (float)diffExp;
				hMaterial.diffuseColor[1] *= (float)diffExp;
				hMaterial.diffuseColor[2] *= (float)diffExp;
			}
		}
		/*From the "color" plug, iterate and see if there are any "color" textures assigned to the plug.*/
		MItDependencyGraph colorTexIt(colorPlug, MFn::kFileTexture, MItDependencyGraph::kUpstream);

		while (!colorTexIt.isDone())
		{
			MFnDependencyNode texture(colorTexIt.currentItem());
			/*Find the "fileTextureName" plug.*/
			MPlug colorTexturePlug = texture.findPlug("fileTextureName", &res);
			if (res == MStatus::kSuccess)
			{
				/*Obtain the filepath name to the assigned color texture.*/
				MString filePathName;
				colorTexturePlug.getValue(filePathName);

				if (filePathName.numChars() > 0)
				{
					MGlobal::displayInfo(filePathName);

					hMaterial.colorMap = filePathName.asChar();
					hMaterial.colorMapLength = filePathName.length() + 1;
					/*Set the texture flag to true.*/
					hMaterial.isTexture = true;
				}
				/*If there are no chars in the filepath name, set the texture flag to false.*/
				else
					hMaterial.isTexture = false;
			}
			/*Process to the next texture.*/
			colorTexIt.next();
		}
		/*Find the "ambientColor" plug and obtain the RGB values.*/
		MPlug ambientPlug = MFnDependencyNode(plug.node()).findPlug("ambientColor", &res);
		if (res == MStatus::kSuccess)
		{
			ambientPlug.getValue(tempData);
			MFnNumericData ambientData(tempData);

			ambientData.getData(rgb[0], rgb[1], rgb[2]);

			hMaterial.ambient[0] = rgb[0];
			hMaterial.ambient[1] = rgb[1];
			hMaterial.ambient[2] = rgb[2];
		}
		/*If the connected shader is a PHONG or BLINN, find the "specularColor" plug and RGB values.*/
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
			}
		}

		/*The material is kLambert, assign zero to specular.*/
		else if (plug.node().hasFn(MFn::kLambert))
		{
			hMaterial.specular[0] = 0;
			hMaterial.specular[1] = 0;
			hMaterial.specular[2] = 0;
		}
		/*Obtained material attributes, send a material message.*/
		fMakeMaterialMessage(hMaterial);
	}
}

void fLoadMaterial(MObject& node)
{
	MStatus res;
	MObject tempData;
	float rgb[3];

	hMaterialHeader hMaterial;

	MObject connectedSet;
	MObject	connectedShader;

	/*If the input is a kMesh, find the surface shader connected to it's shading group.*/
	if (node.hasFn(MFn::kMesh))
	{
		/*Find the shading group and return the set, which the mesh is connected to the SG.*/
		connectedSet = fFindShadingGroup(node);
		/*Find the surfaced shader connected with the shading group.*/
		connectedShader = fFindShader(connectedSet);
	}
	/*
	If the input is a surface shader, assign this directly to the variable "connectedShader".
	As of now this else if never gets entered.
	*/
	else if (node.hasFn(MFn::kLambert) || node.hasFn(MFn::kBlinn) || node.hasFn(MFn::kPhong))
	{
		connectedShader = node;
	}

	/*With the surface shader, find all meshes connected to shader via the shading group.*/
	fFindMeshConnectedToMaterial(connectedShader, hMaterial);

	/*Find the "color" plug and obtain the RGB values from this. NOTE: This is not diffuse color.*/
	MPlug colorPlug = MFnDependencyNode(connectedShader).findPlug("color", &res);
	if (res == MStatus::kSuccess)
	{
		if (res == MStatus::kSuccess)
		{
			/*Obtain the RGB values and store them and the alpha value.*/
			colorPlug.getValue(tempData);
			MFnNumericData colorData(tempData);

			colorData.getData(rgb[0], rgb[1], rgb[2]);

			hMaterial.diffuseColor[0] = (float)rgb[0];
			hMaterial.diffuseColor[1] = (float)rgb[1];
			hMaterial.diffuseColor[2] = (float)rgb[2];
			hMaterial.diffuseColor[3] = 1.0f;
		}
		/*Find the "diffuse" plug. This is how the light scatters on the material.*/
		MPlug diffusePlug = MFnDependencyNode(connectedShader).findPlug("diffuse", &res);
		if (res == MStatus::kSuccess)
		{
			float diffExp;
			diffusePlug.getValue(diffExp);
			/*NOTE: To obtain the "diffuse" color, it's required to multiply the
			color earlier obtained with the diffuse float exponent*/
			hMaterial.diffuseColor[0] *= (float)diffExp;
			hMaterial.diffuseColor[1] *= (float)diffExp;
			hMaterial.diffuseColor[2] *= (float)diffExp;
		}
	}

	/*From the "color" plug, iterate and see if there are any "color" textures assigned to the plug.*/
	MItDependencyGraph colorTexIt(colorPlug, MFn::kFileTexture, MItDependencyGraph::kUpstream);

	while (!colorTexIt.isDone())
	{
		MFnDependencyNode texture(colorTexIt.currentItem());
		/*Find the "fileTextureName" plug.*/
		MPlug colorTexturePlug = texture.findPlug("fileTextureName", &res);

		if (res == MStatus::kSuccess)
		{
			/*Obtain the filepath name to the assigned color texture.*/
			MString filePathName;
			colorTexturePlug.getValue(filePathName);

			if (filePathName.numChars() > 0)
			{
				hMaterial.colorMap = filePathName.asChar();
				hMaterial.colorMapLength = filePathName.length() + 1;
				/*Set the texture flag to true.*/
				hMaterial.isTexture = true;
			}
			/*If there are no chars in the filepath name, set the texture flag to false.*/
			else
				hMaterial.isTexture = false;
		}
		/*Process to the next texture.*/
		colorTexIt.next();
	}
	/*Find the "ambientColor" plug and obtain the RGB values.*/
	MPlug ambientPlug = MFnDependencyNode(connectedShader).findPlug("ambientColor", &res);
	if (res == MStatus::kSuccess)
	{
		ambientPlug.getValue(tempData);
		MFnNumericData ambientData(tempData);

		ambientData.getData(rgb[0], rgb[1], rgb[2]);

		hMaterial.ambient[0] = rgb[0];
		hMaterial.ambient[1] = rgb[1];
		hMaterial.ambient[2] = rgb[2];
	}
	/*If the connected shader is a PHONG or BLINN, find the "specularColor" plug and RGB values.*/
	if (connectedShader.hasFn(MFn::kPhong) || connectedShader.hasFn(MFn::kBlinn))
	{
		MPlug specularPlug = MFnDependencyNode(connectedShader).findPlug("specularColor", &res);
		if (res == MStatus::kSuccess)
		{
			specularPlug.getValue(tempData);
			MFnNumericData specularData(tempData);

			specularData.getData(rgb[0], rgb[1], rgb[2]);

			hMaterial.specular[0] = rgb[0];
			hMaterial.specular[1] = rgb[1];
			hMaterial.specular[2] = rgb[2];
		}
	}

	/*The material is kLambert, assign zero to specular color.*/
	else if (connectedShader.hasFn(MFn::kLambert))
	{
		hMaterial.specular[0] = 0;
		hMaterial.specular[1] = 0;
		hMaterial.specular[2] = 0;
	}
	/*Obtained material attributes, send a material message.*/
	fMakeMaterialMessage(hMaterial);
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

void fMakeChildMessage(hMainHeader& mainH, MDagPath& child, MDagPath& parent)
{
	MStatus res;
	hParChildHeader aC;

	MFnTransform childT(child, &res);
	if (res == MStatus::kSuccess)
	{
		MFnTransform parT(parent, &res);
		if (res == MStatus::kSuccess)
		{
			bool foundParName = false;
			bool foundChildName = false;
			childT.name().asChar();

			for (int i = 0; i < childT.childCount(); i++)
			{
				MObject chi = childT.child(i);

				if (chi.hasFn(MFn::kMesh))
				{
					MFnMesh meshFn(chi, &res);
					if (res == MStatus::kSuccess)
					{
						foundChildName = true;
						aC.childName = meshFn.name().asChar();
						aC.childNameLength = strlen(meshFn.name().asChar()) + 1;
					}
				}
			}
			if (foundChildName)
			{
				for (int i = 0; i < parT.childCount(); i++)
				{
					MObject par = parT.child(i);

					if (par.hasFn(MFn::kMesh))
					{
						MFnMesh meshFn(par, &res);
						if (res == MStatus::kSuccess)
						{
							foundParName = true;
							aC.parentName = meshFn.name().asChar();
							aC.parentNameLength = strlen(meshFn.name().asChar()) + 1;
						}
					}
				}
				if (foundParName)
				{
					mtx.lock();

					memcpy(msg, &mainH, sizeof(hMainHeader));
					memcpy(msg + sizeof(hMainHeader), &aC, sizeof(hParChildHeader));
					memcpy(msg + sizeof(hMainHeader) + sizeof(hParChildHeader), aC.parentName, aC.parentNameLength - 1);
					*(char*)(msg + sizeof(hMainHeader) + sizeof(hParChildHeader) + aC.parentNameLength - 1) = '\0';

					memcpy(msg + sizeof(hMainHeader) + sizeof(hParChildHeader) + aC.parentNameLength, aC.childName, aC.childNameLength - 1);
					*(char*)(msg + sizeof(hMainHeader) + sizeof(hParChildHeader) + aC.parentNameLength + aC.childNameLength - 1) = '\0';

					producer->runProducer(gCb, msg, sizeof(hMainHeader) + sizeof(hParChildHeader) + aC.parentNameLength + aC.childNameLength);

					mtx.unlock();
				}
			}
		}
	}
}

void fOnHierarchyChildAdded(MDagPath &child, MDagPath &parent, void *clientData)
{
	MGlobal::displayInfo("Added child");
	hMainHeader mainH;
	mainH.childAddedCount = 1;
	fMakeChildMessage(mainH, child, parent);
}

void fOnHierarchyChildRemoved(MDagPath &child, MDagPath &parent, void *clientData)
{
	MGlobal::displayInfo("Removed child");
	hMainHeader mainH;
	mainH.childRemovedCount = 1;
	fMakeChildMessage(mainH, child, parent);
}

void fTransAddCbks(MObject& node, void* clientData)
{
	MStatus res;
	MCallbackId id;
	MFnTransform transFn(node, &res);
	if (res == MStatus::kSuccess)
	{
		id = MNodeMessage::addAttributeChangedCallback(node, fOnTransformAttrChange, clientData, &res);
		if (res == MStatus::kSuccess)
		{
			ids.append(id);
		}
		MDagPath pth;
		transFn.getPath(pth);

		id = MDagMessage::addChildAddedDagPathCallback(pth, fOnHierarchyChildAdded, clientData, &res);
		if (res == MStatus::kSuccess)
		{
			MGlobal::displayInfo("AddChild Success!");
			ids.append(id);
		}else
			MGlobal::displayInfo(MString("AddChild Failed. Error: ") + res.errorString());
		
		id = MDagMessage::addChildRemovedDagPathCallback(pth, fOnHierarchyChildRemoved, clientData, &res);
		if (res == MStatus::kSuccess)
		{
			MGlobal::displayInfo("RemoveChild Success!");
			ids.append(id);
		}
		else
			MGlobal::displayInfo(MString("RemoveChild Failed. Error: ") + res.errorString());
		}
}

void fMaterialAddCbks(MObject node, void* clientData)
{
	MStatus res;
	MCallbackId id;
	/*Callback registers when attributes are changed on current and new materials.*/
	id = MNodeMessage::addAttributeChangedCallback(node, fOnMaterialAttrChanges, NULL, &res);
	if (res == MStatus::kSuccess)
	{
		ids.append(id);
	}
}

void fMakeLightMessage(hLightHeader hLight)
{
	MGlobal::displayInfo("LightMsg!");

	hMainHeader hMainHead;
	hMainHead.lightCount = 1;
	size_t mainMem = sizeof(hMainHead);
	size_t lightMem = sizeof(hLightHeader);

	int totalSize = mainMem + lightMem + hLight.lightNameLength;

	mtx.lock();
	memcpy(msg, (void*)&hMainHead, mainMem);
	memcpy(msg + mainMem, (void*)&hLight, lightMem);
	memcpy(msg + mainMem + lightMem, (void*)hLight.lightName, hLight.lightNameLength - 1);
	*(char*)(msg + mainMem + lightMem + hLight.lightNameLength - 1) = '\0';

	producer->runProducer(gCb, msg, totalSize);
	mtx.unlock();
}

void fOnLightAttributeChanges(MNodeMessage::AttributeMessage attrMessage, MPlug& plug, MPlug& otherPlug, void* clientData)
{
	MStatus res;

	MFnPointLight pointLightFn(plug.node(), &res);
	if (res == MStatus::kSuccess)
	{
		/*Strangely when creating a new light, the callback registers this. This is why
		this have to be filtered for the things we only want to process, which is light 
		attribute when they change.*/
		if (attrMessage & MNodeMessage::AttributeMessage::kAttributeSet &&
			attrMessage & MNodeMessage::AttributeMessage::kIncomingDirection &&
			plug.name() == MString(pointLightFn.name() + ".color") ||
			plug.name() == MString(pointLightFn.name() + ".intensity"))
		{
			hLightHeader hLight;

			/*Obtain the color, intensity and range of the light.*/
			MColor lightColor = pointLightFn.color(&res);
			float intensity = pointLightFn.intensity(&res);

			/*The center of illumination will always be the same, with 5 as value.*/
			float centerIllum = pointLightFn.centerOfIllumination(&res);

			/*RGB values of the color need to be multiplied with the intensity.*/
			hLight.color[0] = lightColor.r * intensity;
			hLight.color[1] = lightColor.g * intensity;
			hLight.color[2] = lightColor.b * intensity;

			MGlobal::displayInfo(MString() + centerIllum);
			hLight.range = centerIllum;

			hLight.lightName = pointLightFn.name().asChar();
			hLight.lightNameLength = pointLightFn.name().length() + 1;

			/*Light attribute changes are obtained, send a light message.*/
			fMakeLightMessage(hLight);
		}
	}
}

void fLightAddCbks(MObject node, void* clientData)
{
	MStatus res;
	MCallbackId id;

	/*Callback registers light attribute changes for current and new lights.*/
	id = MNodeMessage::addAttributeChangedCallback(node, fOnLightAttributeChanges, NULL, &res);
	if (res == MStatus::kSuccess)
	{
		ids.append(id);
	}
}

void fLoadLight(MObject lightNode)
{
	/*Function only have support for loading point lights, for now.*/
	MStatus res;

	MFnPointLight pointLightFn(lightNode, &res);
	if (res == MStatus::kSuccess)
	{
		hLightHeader hLight;
		/*Obtain the color, intensity and range of the light.*/
		MColor lightColor = pointLightFn.color(&res);
		float intensity = pointLightFn.intensity(&res);
		float centerIllum = pointLightFn.centerOfIllumination(&res);

		/*The RGB values of the color should be multiplied with the intensity.*/
		hLight.color[0] = lightColor.r * intensity;
		hLight.color[1] = lightColor.g * intensity;
		hLight.color[2] = lightColor.b * intensity; 

		/*The center of illumination will always be the same, with 5 as value.*/
		hLight.range = centerIllum;

		/*Obtaining the name and length for ID usage in Gameplay3D*/
		hLight.lightName = pointLightFn.name().asChar();
		hLight.lightNameLength = pointLightFn.name().length() + 1;

		/*Light attributes are obtained, send a light message.*/
		fMakeLightMessage(hLight);

		/*Update all materials in Gameplay3D when adding point lights.*/
		MItDependencyNodes matIter(MFn::kLambert, &res);

		while (!matIter.isDone())
		{
			fLoadMaterial(matIter.item());

			matIter.next();
		}
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
	else if (node.hasFn(MFn::kLambert) || node.hasFn(MFn::kBlinn) || node.hasFn(MFn::kPhong))
	{
		nt = eNodeType::materialNode; MGlobal::displayInfo("MATERIAL!");
	}

    switch (nt)
    {
        case(eNodeType::meshNode):
        {
            MFnMesh meshFn(node, &res);
            if (res == MStatus::kSuccess)
            {
                fMeshAddCbks(node, clientData);
                //This gets called when the move tool is selected, for some reason
				MGlobal::displayInfo("MESH NODE CREATED!");
				/*Make a mesh message for current and new meshes.*/
				fMakeMeshMessage(node, false);
				/*Load the material assigned to current and new meshes.*/
				fLoadMaterial(node);
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
				fMakeHierarchyMessage(node);
			}
			break;
        }
		case(eNodeType::cameraNode):
		{
			MFnCamera camFn(node, &res);
			if (res == MStatus::kSuccess)
			{	
				M3dView activeCamView = M3dView::active3dView(&res);
				if (res == MStatus::kSuccess)
				{
					if (firstActiveCam == true)
					{
						/*Load the first active camera when plugin is initialized.*/
						fLoadCamera();
						firstActiveCam = false;
					}
				}
			}
			/*All camera related callbacks are registered here.*/
			fCameraAddCbks(node, clientData);
			break;
		}
        case(eNodeType::dagNode):
        {
			/*THINK: comment this out?*/
            MFnDagNode dagFn(node, &res);
            if (res == MStatus::kSuccess)
                fDagNodeAddCbks(node, clientData);

			/*The light node is regarded as a dagNode and that is why
			it's important to check if there is dagPath to this light.*/
			MDagPath lightPath;
			if (dagFn.getPath(lightPath))
			{
				if (lightPath.node().hasFn(MFn::kPointLight))
				{
					/*Load every current and new light's information.*/
					fLoadLight(lightPath.node());
					/*Function containing callbacks related to light*/
					fLightAddCbks(lightPath.node(), clientData);
				}
			}
            break;
        }
		case(eNodeType::materialNode):
		{
			MFnDependencyNode materialFn(node, &res);
			if (res == MStatus::kSuccess)
			{
				/*Function that registers callbacks related to materials.*/
				fMaterialAddCbks(node, clientData);
			}
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
	if (gHierarchyQueue.size() > 0)
	{
		fMakeHierarchyMessage(gHierarchyQueue.front());
		gHierarchyQueue.pop();
	}
	if (gMeshDelayQueue.size() > 0)
	{
		fMakeMeshMessage(gMeshDelayQueue.front(), true);
		gMeshDelayQueue.pop();
	}
}

void fMakeMeshMessage(MObject obj, bool isFromQueue)
{
	MGlobal::displayInfo(MString("Mesh message created!"));
    MStatus res;

	std::vector<sBuiltVertex> meshVertices;

    MFnMesh mesh(obj);

	fLoadMesh(mesh, isFromQueue, meshVertices);

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

	mtx.lock();
    memcpy(msg, (void*)&mainH, (size_t)mainHMem);
    memcpy(msg + mainHMem, (void*)&meshH, (size_t)meshHMem);
    memcpy(msg + mainHMem + meshHMem, (void*)meshH.meshName, meshH.meshNameLen-1);
	*(char*)(msg + mainHMem + meshHMem + meshH.meshNameLen-1) = '\0'; // Insert \0
    memcpy(msg + mainHMem + meshHMem + meshH.meshNameLen, meshVertices.data(), meshVertices.size() * meshVertexMem);

    producer->runProducer(gCb, (char*)msg, totPackageSize);
	mtx.unlock();
}

void fMakeCameraMessage(hCameraHeader& gCam)
{
	MGlobal::displayInfo("CameraMsg!");
	hMainHeader hMainHead;
	size_t mainMem = sizeof(hMainHead);
	size_t camMem = sizeof(hCameraHeader);
	/*Assign the cameraCount with 1.*/
	hMainHead.cameraCount = 1;
	/*Add one to the camera's name length, to add the '\0' byte.*/
	gCam.cameraNameLength++;
	int totalSize = mainMem + camMem + gCam.cameraNameLength;
	
	/*Mutex lock and unlock for precautions with other messages being sent.*/
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

	hMainHeader mainH;
	mainH.transformCount = 1;
	bool hasMeshChild = false;
	bool hasLightChild = false;
	bool hasTransChild = false;
	bool foundChild = false;
	
	MObject childObj;
	/*Getting the first child for mesh, camera or light.*/
	for (unsigned int i = 0; i < trans.childCount(); i++)
	{
		childObj = trans.child(i, &res);
		if (res == MStatus::kSuccess)
		{
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
			}

			if (childObj.hasFn(MFn::kLight))
			{
				hasLightChild = true;
				
				if (childObj.hasFn(MFn::kPointLight))
				{
					hasLightChild = true;
					MFnPointLight pointLightFn(childObj, &res);
					if (res == MStatus::kSuccess)
					{
						transH.childName = pointLightFn.name().asChar();
						transH.childNameLength = pointLightFn.name().length() + 1;

						foundChild = true;
						break;
					}
				}
			}
		}
	}

	if (foundChild)
	{
		mtx.lock();
		int prevSize;
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

void fIterateScene()
{
	MStatus res;
	/*Think about adding the mesh-callbacks like "OnGeometryChange" with this iterator.*/
	MItDependencyNodes dependNodeIt(MFn::kDependencyNode, &res);
	if (res == MStatus::kSuccess)
	{
		while (!dependNodeIt.isDone())
		{
			fOnNodeCreate(dependNodeIt.item(), NULL);
			dependNodeIt.next();
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

			producer->runProducer(gCb, msg, sizeof(hMainHeader) + sizeof(hRemovedObjectHeader) + roh.nameLength);
		}	
	}

	else if (nodeType == eNodeType::pointLightNode)
	{
		MFnPointLight pointLight(node, &res);
		if (res == MStatus::kSuccess)
		{
			roh.nodeType = eNodeType::pointLightNode;
			roh.nameLength = std::strlen(pointLight.name().asChar()) + 1;
			memcpy(msg, &mainH, sizeof(hMainHeader));
			/*First removed object header*/
			memcpy(msg + sizeof(hMainHeader), &roh, sizeof(hRemovedObjectHeader));
			/*Then the name*/
			memcpy(msg + sizeof(hMainHeader) + sizeof(hRemovedObjectHeader), pointLight.name().asChar(), roh.nameLength - 1);
			/*Then add the '\0'*/
			*(char*)(msg + sizeof(hMainHeader) + sizeof(hRemovedObjectHeader) + roh.nameLength - 1) = '\0';
			MGlobal::displayInfo(MString("Deleted light: ") + MString(pointLight.name()));

			producer->runProducer(gCb, msg, sizeof(hMainHeader) + sizeof(hRemovedObjectHeader) + roh.nameLength);

			MItDependencyNodes matIter(MFn::kLambert, &res);
			if (res == MStatus::kSuccess)
			{
				while (!matIter.isDone())
				{
					fLoadMaterial(matIter.item());
					matIter.next();
				}
			}
		}
	}
	mtx.unlock();
}

void fOnMeshRemoved(MObject& node, void* clientData)
{
	if (node.hasFn(MFn::kMesh))
	{
		MGlobal::displayInfo(MString("Mesh got removed! ") + MString(node.apiTypeStr()));
		/*Make a meshRemoved message*/
		fMakeRemovedMessage(node, eNodeType::meshNode);
	}

	else if (node.hasFn(MFn::kDagNode))
	{
		MFnDagNode dagFn(node);

		MDagPath pLightPath;
		if (dagFn.getPath(pLightPath))
		{
			if (pLightPath.node().hasFn(MFn::kPointLight))
			{
				MGlobal::displayInfo(MString("Point light got removed! ") + MString(node.apiTypeStr()));
				/*Make a pointLightRemoved message.*/
				fMakeRemovedMessage(pLightPath.node(), eNodeType::pointLightNode);
			}
		}
	}
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
	
	temp = MDGMessage::addNodeRemovedCallback(fOnMeshRemoved, "dependNode", NULL, &res);
	if (res == MStatus::kSuccess)
	{
		MGlobal::displayInfo("meshRemoved success!");
		ids.append(temp);
	}
	temp = MNodeMessage::addNameChangedCallback(MObject::kNullObj, fOnNodeNameChange, NULL, &res);
	if (res == MStatus::kSuccess)
	{
		MGlobal::displayInfo("nameChange success!");
		ids.append(temp);
	}
	temp = MTimerMessage::addTimerCallback(0.06, fOnElapsedTime, NULL, &res);
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
	//MStatus status = myPlugin.registerCommand("helloWorld", HelloWorld::creator);
	//CHECK_MSTATUS_AND_RETURN_IT(status);
 //   
	MGlobal::displayInfo("Maya plugin loaded!");

	LPCWSTR mtxName = TEXT("producerMutex");
	mtx = Mutex(mtxName);

	/*THIS IS WHERE WE ADD CALLBACKS*/
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
	//MStatus status = plugin.deregisterCommand("helloWorld");
	//CHECK_MSTATUS_AND_RETURN_IT(status);
	MGlobal::displayInfo("Maya plugin unloaded!");

	delete gCb;
	delete producer;
	delete[] msg;

	return MS::kSuccess;
}
