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
	dependencyNode,
	materialNode
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
	unsigned int childAddedCount = 0;
	unsigned int childRemovedCount = 0;
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

struct hParChildHeader
{
	unsigned int parentNameLength;
	const char* parentName;
	unsigned int childNameLength;
	const char* childName;
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
	unsigned int lightNameLength;
	const char* lightName;

	unsigned int lightId;

    float color[3];
};

struct hMeshConnectMaterialHeader
{
	const char* connectMeshName;
	unsigned int connectMeshNameLength = 0;
};

struct hMaterialHeader
{
	int numConnectedMeshes;
	std::vector<hMeshConnectMaterialHeader> connectMeshList;

	float ambient[3];
	float diffuseColor[3];
	float specular[3];

	const char* colorMap;
	int colorMapLength;

	bool isTexture = false;
};

#endif MAYAHEADER_H