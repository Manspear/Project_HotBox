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
    /*
    What to put into the vertex shader:
    *Shared by all*
    list of points
    list of uvs
    list of normals

    *For each individual vertex (per face per polygon)*
    point index
    uv index
    normal index
    */

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

    MFloatArray uArr;
    MFloatArray vArr;
    mesh.getUVs(uArr, vArr);

    /*GET RAW DATA*/

    /*UV set names, we only need to support one*/
    MStringArray uvSetNames;
    mesh.getUVSetNames(uvSetNames);
    //We only support one UV set
    MObjectArray texArr;
    for (int i = 0; i < uvSetNames.length(); i++)
    {
        mesh.getAssociatedUVSetTextures(uvSetNames[i], texArr);
    }

    MFloatPointArray pointArr;
    mesh.getPoints(pointArr, MSpace::kObject);
    sPoint tempPoint;
    for (int i = 0; i < pointArr.length(); i++)
    {
        tempPoint.x = pointArr[i].x;
        tempPoint.y = pointArr[i].y;
        tempPoint.z = pointArr[i].z;
        pnt.push_back(tempPoint);
        MGlobal::displayInfo("Point: " + MString() + tempPoint.x + " " + tempPoint.y + " " + tempPoint.z);
    }

    sUV tempUV;
    for (int i = 0; i < uArr.length(); i++)
    {
        tempUV.u = uArr[i];
        tempUV.v = vArr[i];
        uv.push_back(tempUV);
        MGlobal::displayInfo("UV: " + MString() + uv[i].u + " " + uv[i].v);
    }

    MFloatVectorArray norArr;
    mesh.getNormals(norArr, MSpace::kObject);
    sNormal tempNor;
    for (int i = 0; i < norArr.length(); i++)
    {
        tempNor.x = norArr[i].x;
        tempNor.y = norArr[i].y;
        tempNor.z = norArr[i].z;
        nor.push_back(tempNor);

        MGlobal::displayInfo("Nor: " + MString() + norArr[i].x + " " + norArr[i].y + " " + norArr[i].z);
    }

    /*GET INDICES*/

    /*UVs for each vertex in each triangle*/
    //mesh.getPolygonUV()

    /*Normal IDs. Returns normal indices for all vertices for all faces. Used to index into the array returned by getNormals()*/
    MIntArray normalCnts;
    MIntArray normalIDs;
    mesh.getNormalIds(normalCnts, normalIDs);

    /*Returns the number of triangles for every polygon face and the offset into the vertex indices array for each triangle vertex (see getVertices()). */
    MIntArray triangleCounts; 
    MIntArray triangleIndices; //I... Think this is used on all of the vertex-related id variables. Especially getVertices().
    mesh.getTriangleOffsets(triangleCounts, triangleIndices);

    /*Returns the object-relative vertex indices for all polygons. The indices refer to the elements in the array returned by getPoints()*/
    MIntArray vertexCount; //Vertex count per polygon. 
    MIntArray vertexList; 
    mesh.getVertices(vertexCount, vertexList);

    /*See which list is the largest (we guess normal list for now) and adapt all indexing after it's size. "Stretch" the data. */
    
    /*You construct the mesh in the shader in this manner. */

    /*
        I need the vertex data to be structured like: pos, nor, uv
        Otherwise stuff won't work. 
        
        The vertices will have to be a conglomerate of "indexes". 
        For cube:
        pointIndices for a cube: 8
        
    */
    
    MIntArray pntIdTest;
    MIntArray triIdTest;
    MIntArray normalIdTest;
    int counter = 0;
    for (int i = 0; i < normalCnts.length(); i++)
    {
        for (int j = 0; j < normalCnts[i]; j++)
        {
            normalIdTest.append(normalIDs[j + counter]);
        }
        counter += normalCnts[i];
    }
    //For each triangle
    counter = 0;
    for (int i = 0; i < triangleCounts.length(); i++) //"Should" only have "index-ranges" between 0 and 7... But HAS from 0 to 35
    {
        ////0-35 mode
        ////For each triangle in face
        //for (int j = 0; j < triangleCounts[i]; j++)
        //{
        //    for(int k = 0; k < 3; k++)
        //        triIdTest.append(triangleIndices[j + counter + k]);
        //}
        //counter += triangleCounts[i] * 3; 
        //For each triangle in face
        for (int j = 0; j < triangleCounts[i]; j++)
        {
            for(int k = 0; k < 3; k++)
                triIdTest.append(triangleIndices[j + counter + k]);
        }
        counter += triangleCounts[i] * 3; 
    }
    /*__Count says "how many per polygon". As such we must loop through each polygon, loop through the number of "things" on that polygon, add the "things" count on top of a counter to be used next time*/
    counter = 0;
    for (int i = 0; i < vertexCount.length(); i++)
    {
        for (int j = 0; j < vertexCount[i]; j++) 
        {
            pntIdTest.append(vertexList[j + counter]);
        }
        counter += vertexCount[i];
    }
    //triangleCounts.length() == 12
    //triangleCounts[0] == 1 --> if cube is triangulated. == 2 if quads. Literally "The number of triangles per polygon face"
    //three triangleIndices per triangle. 
    //So how to use triangleIndices:

    //for (int i = 0; i < triIdTest.length(); i++)
    //{
    //    MGlobal::displayInfo(MString(" ") + triIdTest[i]);
    //}
    for (int i = 0; i < pntIdTest.length(); i++)
    {
        MGlobal::displayInfo(MString(" ") + pntIdTest[i]);
    }

    MGlobal::displayInfo(MString("VertexIndexList: ") + pntIdTest.length() + MString(" TriangleIndexList: ") + triIdTest.length() + MString(" NormalIndexList: ") + normalIdTest.length());


    MGlobal::displayInfo(MString("VertList: ") + vertexList.length() + MString(" vertexCount ") + vertexCount.length() + MString(" TriangleIndices: ") + triangleIndices.length() + MString(" TriangleCount: ") + triangleCounts.length() + MString(" NormalIDs: ") + normalIDs.length() + MString(" NormalCount: ") + normalCnts.length());



    /*FUCK INDEXING! LET'S DO IT THE SHITTY WAY!!!*/
    MIntArray triCnt;
    MIntArray triVert;
    mesh.getTriangles(triCnt, triVert);
    MIntArray polygonVertices;
    MVector normal;
    float u;
    float v;
    int vertices[3];
    //For each polygon, be it quad or triangle...
    for (int i = 0; i < mesh.numPolygons(); i++)
    {
        
        for (int o = 0; o < triCnt.length(); o++)
        {
            for (int j = 0; j < triVert[i]; j++)
            {
                //Vertices seem to only be indexes to the MPoint-list
                mesh.getPolygonTriangleVertices(i, o, vertices);
                //mesh.getPolygonUV();
            }
        }
       
        mesh.getPolygonNormal(i, normal);

        mesh.getPolygonVertices(i, polygonVertices);

        for (int j = 0; j < polygonVertices.length(); j++)
        {
            for (int k = 0; k < uvSetNames.length(); k++)
            {
                const MString nam = uvSetNames[k];
                mesh.getPolygonUV(i, j, u, v, &nam);
            }
        }

        //mesh.getTriangleOffset()
        //mesh.polyTriangulate()

       //Now, how to "split" the polygon if it is a quad or ngon?
       //Below link is to an example that triangulates a mesh. Might be useful.
       //http://help.autodesk.com/view/MAYAUL/2016/ENU/?guid=__cpp_ref_gpu_cache_2gpu_cache_util_8h_example_html
        
    }












   

    ///*Prolly doesn't work*/
    //MIntArray triVertices;
    //MIntArray triCounts;
    //mesh.getTriangles(triCounts, triVertices);
    ////For each triangle
    //for (int i = 0; i < triCounts.length(); i++)
    //{
    //    //For this triangle
    //    for (int p = 0; p < triCounts[i]; p++)
    //    {
    //        //For this triangles vertices
    //        for (int o = 0; o < triVertices[p]; o++)
    //            mesh.getTangentId(p, o);
    //    }
    //}
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