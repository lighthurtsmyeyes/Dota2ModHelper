#pragma once

#include "DmxBinary.h"
#include "GltfMorph.h"

#include <string>
#include <vector>

namespace morphmerge {

struct Options {
    // Do NOT remap axes: the pinned Source2Viewer-CLI (VRF 15.0) exports morph
    // deltas already in Source space (Z-up, inches, unscaled), matching the DMX
    // bind pose. Applying the remap rotates every delta -90 deg around the
    // forward axis, which swaps each flex's function (eyeUp <-> eyeLeft, ...).
    // Only enable if the exporter is ever upgraded to a build that bakes the
    // Source->glTF rotation into geometry (see GltfMorph.h).
    bool applyAxisTransform = false;
    float posEps = 1e-5f;             // sparsify threshold for position deltas
    float nrmEps = 1e-5f;             // sparsify threshold for normal deltas
};

// One DmeMesh element in the DMX paired with the glTF primitive that carries
// its morph targets.
struct MeshTarget {
    int elem = -1;                              // index of the DmeMesh element in doc
    const gltfmorph::GltfMesh* gm = nullptr;    // matching glTF morph primitive
};

// Injects morph targets (flexes) into every DmeMesh listed in `targets`,
// adding DmeVertexDeltaData shapes per mesh plus one shared set of
// DmeCombinationInputControl channels and one DmeCombinationOperator whose
// targets[] reference all patched meshes (the layout the model compiler
// expects for multi-mesh-group files, e.g. face + eye shadow overlay).
// Channels are built from the first target's morph names; other meshes must
// use the same names/order (true for VRF exports of a model's mesh groups).
// Modifies `doc` in place. `log` receives a human summary.
bool Merge(dmxbin::Doc& doc, const std::vector<MeshTarget>& targets, const Options& opt, std::string& log);

// Single-mesh convenience overload: merges into the first DmeMesh.
bool Merge(dmxbin::Doc& doc, const gltfmorph::GltfMesh& mesh, const Options& opt, std::string& log);

// Removes any pre-existing morph elements (DmeVertexDeltaData /
// DmeCombinationInputControl / DmeCombinationOperator) and clears the meshes'
// delta*/combinationOperator attrs, so a subsequent Merge produces a single
// authoritative morph set instead of layering on top of VRF's (lossy) decompile.
// `baseStates` is deliberately NOT cleared: it is not a morph attr, and any
// DmeMesh left without it makes resourcecompiler/modeldoc crash.
void StripMorphs(dmxbin::Doc& doc);

// Element indices of all DmeMesh elements in the doc, in file order.
std::vector<int> GetMeshElements(const dmxbin::Doc& doc);

// Returns the bind-pose vertex count of the given DmeMesh element
// (position$0 of its bindState/currentState/baseStates[0]), or -1.
int GetBindVertexCount(const dmxbin::Doc& doc, int meshElem);

// Returns the bind-pose vertex count of the first DmeMesh, or -1.
int GetBindVertexCount(const dmxbin::Doc& doc);

// Returns the name of the given DmeMesh element, or empty string.
std::string GetMeshName(const dmxbin::Doc& doc, int meshElem);

// Returns the name of the first DmeMesh element, or empty string if none.
std::string GetMeshName(const dmxbin::Doc& doc);

// Wires the DmeCombinationOperator to the scene root via a `combinationOperator`
// element attribute. This is the link resourcecompiler/studiomdl uses to discover
// the flex operator (and thus the DmeCombinationInputControl channels); without it
// the operator is orphaned and the compiler bakes the mesh deltas as shapes but
// emits no flex controllers. If the scene root is a VRF-style DmeRoot it is
// re-typed to the classic generic `DmElement` named "Scene" (children preserved).
// Attr-only: no element removal, no index remap.
void WireCombinationOperatorToScene(dmxbin::Doc& doc);

} // namespace morphmerge
