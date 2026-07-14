#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace gltfmorph {

struct MorphTarget {
    std::string name;
    std::vector<std::array<float, 3>> position;
    std::vector<std::array<float, 3>> normal;
};

struct GltfMesh {
    std::string name;                 // glTF mesh node name (for matching to a DMX mesh)
    int vertexCount = 0;              // vertex count of this primitive's POSITION
    std::vector<MorphTarget> targets;
};

// Loads every primitive that carries morph targets from a .gltf (external .bin)
// or .glb. Each such primitive becomes one GltfMesh with its own vertexCount,
// name and target list. Primitives without morph targets are skipped.
bool Load(const std::string& path, std::vector<GltfMesh>& out, std::string& err);

// glTF -> Source axis remap for direction vectors ((x, y, z) -> (x, z, -y)).
// Only needed if the glTF exporter bakes the Source->glTF rotation (Y-up,
// meters) into geometry. The pinned Source2Viewer-CLI (VRF 15.0) does NOT: it
// writes base positions and morph deltas in Source space (Z-up, inches), so
// deltas must be used as-is. MorphMerge::Options::applyAxisTransform therefore
// defaults to false; passing these deltas through this remap would rotate them
// -90 deg around the forward axis and permute every flex's function.
inline std::array<float, 3> GltfToSource(std::array<float, 3> v) {
    return { v[0], v[2], -v[1] };
}

} // namespace gltfmorph
