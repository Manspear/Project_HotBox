#ifndef MAYAHEADER_H

#define MAYAHEADER_H

#include <vector>

/*Put the headers here.*/

struct hMainHeader
{
    unsigned int meshCount;
    unsigned int cameraCount;
    unsigned int transformCount;
    unsigned int lightCount;
    unsigned int materialCount;
};

struct hMeshHeader
{
    unsigned int meshNameLen;
    const char* meshName;
    unsigned int materialId;
    unsigned int prntTransNameLen;
    const char* prntTransName;

    unsigned int vertexCount;
};

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

static std::vector<hMeshVertex> meshVertexList;

struct hCameraHeader
{
	const char* cameraName;

	float projMatrix[16];
};

static std::vector<hCameraHeader> cameraList;

struct hTransformHeader
{
    unsigned int childNameLength;
    const char* childName;

    float trans[3];
    float rot[3];
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