//------------------------------------------------------------------------------
//  VisTree.cc
//------------------------------------------------------------------------------
#include "Pre.h"
#include "Config.h"
#include "VisTree.h"
#include "glm/trigonometric.hpp"

using namespace Oryol;

//------------------------------------------------------------------------------
void
VisTree::Setup(int displayWidth, float fov) {

    // compute K for screen space error computation
    // (see: http://tulrich.com/geekstuff/sig-notes.pdf )
    this->K = float(displayWidth) / (2.0f * glm::tan(fov*0.5f));

    // setup VisNodes pool
    this->freeNodes.Reserve(MaxNumNodes);
    for (int i = MaxNumNodes-1; i >=0; i--) {
        this->freeNodes.Add(i);
    }
    this->rootNode = this->AllocNode();
}

//------------------------------------------------------------------------------
void
VisTree::Discard() {
    this->freeNodes.Clear();
}

//------------------------------------------------------------------------------
VisNode&
VisTree::At(int16 nodeIndex) {
    o_assert_dbg((nodeIndex >= 0) && (nodeIndex < MaxNumNodes));
    return this->nodes[nodeIndex];
}

//------------------------------------------------------------------------------
int16
VisTree::AllocNode() {
    int16 index = this->freeNodes.PopBack();
    VisNode& node = this->nodes[index];
    node.Reset();
    return index;
}

//------------------------------------------------------------------------------
void
VisTree::Split(int16 nodeIndex) {
    VisNode& node = this->At(nodeIndex);
    o_assert_dbg(node.IsLeaf());
    for (int i = 0; i < VisNode::NumChilds; i++) {
        node.childs[i] = this->AllocNode();
    }
}

//------------------------------------------------------------------------------
void
VisTree::Merge(int16 nodeIndex) {
    VisNode& node = this->At(nodeIndex);
    o_assert_dbg(InvalidIndex == node.geoms[0]);
    for (int i = 0; i < VisNode::NumChilds; i++) {
        if (InvalidIndex != node.childs[i]) {
            this->Merge(node.childs[i]);
            this->freeNodes.Add(node.childs[i]);
            node.childs[i] = InvalidIndex;
        }
    }
}

//------------------------------------------------------------------------------
float
VisTree::ScreenSpaceError(const VisBounds& bounds, int lvl, int posX, int posY) const {
    // see http://tulrich.com/geekstuff/sig-notes.pdf

    // we just fudge the geometric error of the chunk by doubling it for
    // each tree level
    const float delta = float(1<<lvl);
    const float D = float(bounds.MinDist(posX, posY)+1);
    float rho = (delta/D) * this->K;
    return rho;
}

//------------------------------------------------------------------------------
void
VisTree::Traverse(int posX, int posY) {
    int lvl = NumLevels;
    int nodeIndex = this->rootNode;
    VisBounds bounds = VisTree::Bounds(lvl, 0, 0);
    this->traverse(nodeIndex, bounds, lvl, posX, posY);
}

//------------------------------------------------------------------------------
void
VisTree::traverse(int16 nodeIndex, const VisBounds& bounds, int lvl, int posX, int posY) {
    VisNode& node = this->At(nodeIndex);
    float rho = this->ScreenSpaceError(bounds, lvl, posX, posY);
    const float tau = 100.0f;
    if ((rho <= tau) || (0 == lvl)) {
        // FIXME: this will become a visible leaf node
        Log::Info("draw: x0=%d, x1=%d, y0=%d, y1=%d\n",
            bounds.x0, bounds.x1, bounds.y0, bounds.y1);
    }
    else {
        if (node.IsLeaf()) {
            this->Split(nodeIndex);
        }
        const int halfX = (bounds.x1 - bounds.x0)/2;
        const int halfY = (bounds.y1 - bounds.y0)/2;
        for (int x = 0; x < 2; x++) {
            for (int y = 0; y < 2; y++) {
                VisBounds childBounds;
                childBounds.x0 = bounds.x0 + x*halfX;
                childBounds.x1 = childBounds.x0 + halfX;
                childBounds.y0 = bounds.y0 + y*halfY;
                childBounds.y1 = childBounds.y0 + halfY;
                const int childIndex = (y<<1)|x;
                this->traverse(node.childs[childIndex], childBounds, lvl-1, posX, posY);
            }
        }
    }
}

//------------------------------------------------------------------------------
VisBounds
VisTree::Bounds(int lvl, int x, int y) {
    o_assert_dbg(lvl <= NumLevels);
    // level 0 is most detailed, level == NumLevels is the root node
    int dim = (1<<lvl) * Config::ChunkSizeXY;
    VisBounds bounds;
    bounds.x0 = (x>>lvl) * dim;
    bounds.x1 = bounds.x0 + dim;
    bounds.y0 = (y>>lvl) * dim;
    bounds.y1 = bounds.y0 + dim;
    return bounds;
}

//------------------------------------------------------------------------------
glm::vec3
VisTree::Translation(int x0, int y0) {
    return glm::vec3(x0, y0, 0.0f);
}

//------------------------------------------------------------------------------
glm::vec3
VisTree::Scale(int x0, int x1, int y0, int y1) {
    const float s = Config::ChunkSizeXY;
    return glm::vec3(float(x1-x0)/s, float(y1-y0)/s, 1.0f);
}
