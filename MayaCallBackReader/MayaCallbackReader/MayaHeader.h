#ifndef MAYAHEADER_H

#define MAYAHEADER_H

#include <vector>

/*e for enum*/
enum eNodeType
{
	meshNode = 4,
	transformNode,
	dagNode,
	pointLightNode,
	cameraNode,
	notHandledNode,
	dependencyNode
};

/*Put the headers here.*/
/*By default the values are zeroed*/
struct hMainHeader
{
	unsigned int removedObjectCount = 0;
    unsigned int meshCount = 0;
    unsigned int cameraCount = 0;
    unsigned int transformCount = 0;
    unsigned int lightCount = 0;
    unsigned int materialCount = 0;
	unsigned int hierarchyCount = 0;
};
/*Contains the name, and type of the removed object*/
struct hRemovedObjectHeader
{
	unsigned int nodeType;
	unsigned int nameLength;
	char* name;
};

struct hMeshHeader
{
    unsigned int meshNameLen;
    const char* meshName;
    unsigned int materialId;

    unsigned int vertexCount;
};

struct hVertexHeader
{
    float dPoints[3];
    float dUV[2];
    float dNormal[3];
};

struct hMeshVertex
{
    std::vector<hVertexHeader> vertexList;
};

struct hCameraHeader
{
	unsigned int cameraNameLength;
	const char* cameraName;

	float projMatrix[16];

	float trans[3];
	float rot[4];
	float scale[3];
};

struct hTransformHeader
{
    unsigned int childNameLength;
    const char* childName;

	//unsigned int parentNameLength;
	//const char* parentName;

	float trans[3];
    float rot[4];
	float scale[3];
};

struct hHierarchyHeader
{
	const char* parentNodeName;
	int parentNodeNameLength;
	int childNodeCount = 0;
};
struct hChildNodeNameHeader
{
	//Should probably skip having these pointers be part of messages... But they do have some utility on the engine side.
	const char* objName;
	int objNameLength;
};

struct hLightHeader
{
    char lightName[256];

    float intensity;
    float color[3];
};

struct hMaterialHeader
{
	unsigned int materialNameLength;
	const char* materialName;

	unsigned int connectedMeshNameLength;
	const char* connectedMeshName;

    /*float reflectivity;*/

    float ambient[3];
    float diffuse[4];
    float specular[3];
    float color[3];

    /*char normalMap[256];
    char diffuseMap[256];
    char specularMap[256];*/
};

#endif MAYAHEADER_H