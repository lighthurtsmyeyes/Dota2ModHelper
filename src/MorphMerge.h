#pragma once

#include "DmxBinary.h"
#include "GltfMorph.h"

#include <string>

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

// Injects morph targets (flexes) from `mesh` into the DmeMesh of `doc`,
// adding DmeVertexDeltaData + DmeCombinationOperator + DmeCombinationInputControl
// elements and wiring DmeMesh.deltaStates / deltaStateWeights / baseStates /
// combinationOperator. Modifies `doc` in place. `log` receives a human summary.
bool Merge(dmxbin::Doc& doc, const gltfmorph::GltfMesh& mesh, const Options& opt, std::string& log);

// Removes any pre-existing morph elements (DmeVertexDeltaData /
// DmeCombinationInputControl / DmeCombinationOperator) and clears the mesh's
// delta*/combinationOperator attrs, so a subsequent Merge produces a single
// authoritative morph set instead of layering on top of VRF's (lossy) decompile.
void StripMorphs(dmxbin::Doc& doc);

// Returns the bind-pose vertex count of the first DmeMesh (position$0 of its
// bindState), or -1 if it cannot be determined.
int GetBindVertexCount(const dmxbin::Doc& doc);

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
