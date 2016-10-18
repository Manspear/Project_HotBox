#ifndef MAYAHEADER_H

#define MAYAHEADER_H

#include <vector>

/*e for enum*/
enum eNodeType
{
	meshNode,
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
};
/*Contains the name, and type of the removed object*/
struct hRemovedObjectHeader
{
	unsigned int nodeType;
	unsigned int nameLength;
	char* name;
};

static std::vector<hRemovedObjectHeader> removedList;
/*
Maybe it would be more "organized" if a mesh had it's vertices incorporated into itself
  
*/
struct hMeshHeader
{
    unsigned int meshNameLen;
    const char* meshName;
    unsigned int materialId;
    unsigned int prntTransNameLen;
    const char* prntTransName;

    unsigned int vertexCount;
};
/*Meshes added in HMessageReader::processMesh()*/
static std::vector<hMeshHeader> meshList;

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
/*Vertices added in HMessageReader::processMesh()*/
static std::vector<hMeshVertex> meshVertexList;

struct hCameraHeader
{
	unsigned int cameraNameLength;
	const char* cameraName;

	float projMatrix[16];

	float trans[3];
	float rot[4];
	float scale[3];
};

static std::vector<hCameraHeader> cameraList;

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

static std::vector<hTransformHeader> transformList;

struct hLightHeader
{
    char lightName[256];

    float intensity;
    float color[3];
};

static std::vector<hLightHeader> lightList;

struct hMaterialHeader
{
    char materialName[256];

    float reflectivity;

    float ambient[3];
    float diffuse[3];
    float specular[3];
    float color[3];

    char normalMap[256];
    char diffuseMap[256];
    char specularMap[256];
};

static std::vector<hMaterialHeader> materialList;

#endif MAYAHEADER_H