#include "mayaIncludes.h"
#include "Linker.h"

/*CallBack Ids. For now we have these separate, should have a array instead.*/
MCallbackIdArray idList;

/*Most functions will use this variable to verify for errors.*/
MStatus res = MS::kSuccess;

/*Callback when a node have been renamed.*/
void NodeRenamed(MObject &node, const MString &str, void* clientData)
{
    /*Print out the nodepath for the object that triggered the callback.*/

    if (node.hasFn(MFn::kTransform) && !node.hasFn(MFn::kFreePointManip) && !node.hasFn(MFn::kTranslateManip)) {

        MGlobal::displayInfo("A node was renamed...");

        MFnTransform transNode(node);

        MGlobal::displayInfo(MString("Type: ") + node.apiTypeStr() + MString("\nName: ") + transNode.name());
    }

    if (node.hasFn(MFn::kMesh))
    {
        MGlobal::displayInfo("A node was renamed...");

        MFnMesh meshNode(node);

        MGlobal::displayInfo(MString("Type: ") + node.apiTypeStr() + MString("\nName: ") + meshNode.name());
    }
}

/*Callback that prints out the elapsed time since last callback.*/
void TimePassed(float elapsedTime, float lastTime, void* clientData)
{
    MGlobal::displayInfo("Time elapsed since last callback: " + MString() + elapsedTime);
}

/*Callback that notify if a transform matrix is changed, when moving nodes.*/
void WorldMatrixChanged(MObject &transformNode, MDagMessage::MatrixModifiedFlags &modified, void *clientData)
{
    MGlobal::displayInfo("A transformation node was changed...");

    MFnTransform transNode(transformNode);

    MGlobal::displayInfo(MString("Type: ") + transformNode.apiTypeStr() + MString("\nName: ") + transNode.name());
}

/*Callback that notify if a vertex position was changed in a mesh.*/
void VertexPosChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void* clientData)
{
    /*When processing the input mesh node, we can filter out things we don't want to gain access to.
    In this case the "Pnts" attribute for vertex positions is to be considered as an element of the whole
    "ATTRIBUTE" array.*/
    if (msg & MNodeMessage::kAttributeSet && !plug.isArray() && plug.isElement()) {

        MGlobal::displayInfo("Vertex/vertices was changed in the mesh.");

        MFnMesh meshNode(plug.node());

        /*Print out the nodepath for the object that triggered the callback.*/
        MGlobal::displayInfo(MString("Type: ") + plug.node().apiTypeStr() + MString("\nName: ") + meshNode.name());

        /*Obtain the children of this plug.*/
        MPlug plugX = plug.child(0);
        MPlug plugY = plug.child(1);
        MPlug plugZ = plug.child(2);

        /*Get the x, y, z position values of the vertices.*/
        float x = 0;
        float y = 0;
        float z = 0;

        plugX.getValue(x);
        plugY.getValue(y);
        plugZ.getValue(z);

        /*Print out the changing vertex positions.*/
        MGlobal::displayInfo("x: " + MString() + x);
        MGlobal::displayInfo("y: " + MString() + y);
        MGlobal::displayInfo("z: " + MString() + z);
    }
}

/*Process mesh nodes that are created in realtime.*/
void meshNodes(MObject& node)
{
    MDagPath dp = MDagPath::getAPathTo(node);

    MObject shapeNode;

    /*Checks that there are not several meshes under this parent node.*/
    if (dp.extendToShape()) {

        shapeNode = dp.node();
    }

    else {
        MGlobal::displayInfo("ERROR: There are several meshes under this node.");
    }

    MCallbackId vertId = MNodeMessage::addAttributeChangedCallback(
        shapeNode,
        VertexPosChanged,
        NULL,
        &res);

    if (res == MS::kSuccess) {
        idList.append(vertId);
    }

    else {
        MGlobal::displayInfo("ERROR: Could not trigger callback VertexPosChanged.");
    }
}

/*Process transform nodes that are created in realtime.*/
void transformNodes(MObject& node)
{
    MFnTransform transformNode(node);

    /*Gets the first child under this transform node. Works with all
    meshes, cameras and lights etc.*/
    MDagPath dp = MDagPath::getAPathTo(transformNode.child(0));

    MCallbackId transformId = MDagMessage::addWorldMatrixModifiedCallback(
        dp,
        WorldMatrixChanged,
        NULL,
        &res);

    if (res == MS::kSuccess) {
        idList.append(transformId);
    }

    else {
        MGlobal::displayInfo("ERROR: Could not trigger callback WorldMatrixChanged.");
    }
}

/*Iteration of the nodes that already exist in the scene.*/
void IterationNodes()
{
    /*All DAG nodes that are not transform nodes must exist as a child of a transform node.*/
    MItDag transIteration(MItDag::kBreadthFirst, MFn::kTransform, &res);

    for (; !transIteration.isDone(); transIteration.next())
    {
        MFnTransform aTransform(transIteration.currentItem());

        /*Obtain the first child of each transform.*/
        MDagPath path = MDagPath::getAPathTo(aTransform.child(0));

        MCallbackId transformId = MDagMessage::addWorldMatrixModifiedCallback(
            path,
            WorldMatrixChanged,
            NULL,
            &res);

        if (res == MS::kSuccess) {
            idList.append(transformId);
        }

        else {
            MGlobal::displayInfo("ERROR: Could not trigger callback WorldMatrixChanged.");
        }
    }

    MItDag meshIteration(MItDag::kBreadthFirst, MFn::kMesh, &res);

    for (; !meshIteration.isDone(); meshIteration.next())
    {
        MObject node = meshIteration.currentItem();

        MCallbackId vertId = MNodeMessage::addAttributeChangedCallback(
            node,
            VertexPosChanged,
            NULL,
            &res);

        if (res == MS::kSuccess) {
            idList.append(vertId);
        }

        else {
            MGlobal::displayInfo("ERROR: Could not trigger callback VertexPosChanged.");
        }
    }

    /*When a node is renamed, this callback will be triggered and returned.*/
    MCallbackId nameChangedId = MNodeMessage::addNameChangedCallback(
        MObject::kNullObj,
        NodeRenamed,
        NULL,
        &res);

    if (res == MS::kSuccess) {
        idList.append(nameChangedId);
    }

    else {
        MGlobal::displayInfo("ERROR: Could not trigger callback for NodeRenamed.");
    }
}

/*The callback will be executed n times depending on the hierachy of a node type.*/
void NodeAdded(MObject &node, void* clientData)
{
    /*If "NODE TYPE" is kTransform.*/
    if (node.hasFn(MFn::kTransform)) {

        MGlobal::displayInfo("A node was added...");

        MFnTransform transNode(node);

        /*Print out the nodepath for the object that triggered the callback.*/
        MGlobal::displayInfo(MString("Type: ") + node.apiTypeStr() + MString("\nName: ") + transNode.name());

        transformNodes(node);
    }

    /*If "NODE TYPE" is kMesh.*/
    if (node.hasFn(MFn::kMesh)) {

        MGlobal::displayInfo("A node was added...");

        MFnMesh meshNode(node);

        /*Print out the nodepath for the object that triggered the callback.*/
        MGlobal::displayInfo(MString("Type: ") + node.apiTypeStr() + MString("\nName: ") + meshNode.name());

        meshNodes(node);
    }

    /*When a node is renamed, this callback will be triggered and returned.*/
    MCallbackId nameChangedId = MNodeMessage::addNameChangedCallback(
        MObject::kNullObj,
        NodeRenamed,
        NULL,
        &res);

    if (res == MS::kSuccess) {
        idList.append(nameChangedId);
    }

    else {
        MGlobal::displayInfo("ERROR: Could not trigger callback for NodeRenamed.");
    }
}

EXPORT MStatus initializePlugin(MObject obj)
{
    MFnPlugin aPlugin(obj, "Maya plugin", "1.0", "Any", &res);

    /*Checks if the plugin failed to initialize.*/
    if (MFAIL(res)) {
        CHECK_MSTATUS(res);
    }

    /*A callback id is returned when the callback addNodeAddedCallBack
    is registered in the plugin.*/
    MCallbackId nodeAddedId = (MDGMessage::addNodeAddedCallback(
        NodeAdded,
        kDefaultNodeType,
        NULL,
        &res
    ));

    if (res == MS::kSuccess) {
        idList.append(nodeAddedId);
    }

    else {
        MGlobal::displayInfo("ERROR: Could not trigger callback for NodeAdded.");
    }

    /*If there would already be an scene filled with nodes, the plugin should loop
    through all these type nodes.*/
    IterationNodes();

    MCallbackId timeId = MTimerMessage::addTimerCallback(
        5,
        TimePassed,
        NULL,
        &res);

    if (res == MS::kSuccess) {
        idList.append(timeId);
    }

    else {
        MGlobal::displayInfo("Error: Could not trigger callback for TimePassed.");
    }

    MGlobal::displayInfo("Maya plugin was loaded!");

    return res;
}

EXPORT MStatus uninitializePlugin(MObject obj)
{
    /*Initializes the function set with MObject, which represents the plugin.*/
    MFnPlugin plugin(obj);

    //////////////////////////////////////////////////////////////////////////////////

    /*Any resources allocated should be released/deleted before unloading the plugin.*/

    //////////////////////////////////////////////////////////////////////////////////

    /*Remove all callbacks when unitializing the plugin.*/
    MMessage::removeCallbacks(idList);

    /*Display in Maya that the plugin was unloaded.*/
    MGlobal::displayInfo("Maya plugin unloaded!");

    return MS::kSuccess;
}
