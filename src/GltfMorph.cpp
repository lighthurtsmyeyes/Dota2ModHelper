#include "GltfMorph.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace gltfmorph {

namespace {

bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::streamoff n = f.tellg();
    if (n <= 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(n));
    f.read(reinterpret_cast<char*>(out.data()), n);
    return static_cast<bool>(f);
}

uint32_t ru32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }

// Parse GLB into json text + bin bytes. Returns false if not a GLB.
bool ParseGlb(const std::vector<uint8_t>& file, std::string& jsonText, std::vector<uint8_t>& bin) {
    if (file.size() < 12) return false;
    if (ru32(file.data()) != 0x46546C67u) return false; // 'glTF'
    // uint32_t version = ru32(file.data()+4); uint32_t length = ru32(file.data()+8);
    size_t off = 12;
    bool gotJson = false;
    while (off + 8 <= file.size()) {
        uint32_t clen = ru32(file.data() + off);
        uint32_t ctype = ru32(file.data() + off + 4);
        off += 8;
        if (off + clen > file.size()) return false;
        if (ctype == 0x4E4F534Au && !gotJson) { // 'JSON'
            jsonText.assign(reinterpret_cast<const char*>(file.data() + off), clen);
            gotJson = true;
        } else if (ctype == 0x004E4942u) { // 'BIN\0'
            bin.assign(file.data() + off, file.data() + off + clen);
        }
        off += clen;
    }
    return gotJson;
}

int ComponentSize(uint32_t ct) {
    switch (ct) {
        case 5120: case 5121: return 1; // BYTE / UBYTE
        case 5122: case 5123: return 2; // SHORT / USHORT
        case 5125: case 5126: return 4; // UINT / FLOAT
        default: return 0;
    }
}
int TypeCount(const std::string& t) {
    if (t == "SCALAR") return 1;
    if (t == "VEC2") return 2;
    if (t == "VEC3") return 3;
    if (t == "VEC4") return 4;
    return 0;
}

// Read a float VEC3 array from an accessor (only FLOAT component supported).
bool ReadVec3Accessor(const json& root, const std::vector<std::vector<uint8_t>>& buffers,
                      int accIdx, std::vector<std::array<float,3>>& out, std::string& err) {
    out.clear();
    if (accIdx < 0 || accIdx >= (int)root["accessors"].size()) return true; // missing -> empty
    const auto& acc = root["accessors"][accIdx];
    uint32_t compType = acc.value("componentType", 5126u);
    if (compType != 5126u) { err = "accessor componentType not FLOAT"; return false; }
    int comps = TypeCount(acc.value("type", std::string("VEC3")));
    if (comps < 2) { err = "accessor type not vector"; return false; }
    int count = acc.value("count", 0);
    int bvIdx = acc.value("bufferView", -1);
    if (bvIdx < 0) { out.assign(count, {0,0,0}); return true; }
    const auto& bv = root["bufferViews"][bvIdx];
    int bufIdx = bv.value("buffer", 0);
    if (bufIdx < 0 || bufIdx >= (int)buffers.size()) { err = "bufferView buffer OOB"; return false; }
    const auto& buf = buffers[bufIdx];
    size_t base = (size_t)acc.value("byteOffset", 0) + (size_t)bv.value("byteOffset", 0);
    int stride = bv.value("byteStride", comps * 4);
    if (stride < comps * 4) stride = comps * 4;
    out.resize(count);
    for (int i = 0; i < count; ++i) {
        size_t off = base + (size_t)i * (size_t)stride;
        std::array<float,3> v{0,0,0};
        for (int c = 0; c < comps && c < 3; ++c) {
            if (off + (size_t)(c+1)*4 > buf.size()) { err = "accessor read OOB"; return false; }
            std::memcpy(&v[c], buf.data() + off + (size_t)c*4, 4);
        }
        out[i] = v;
    }
    return true;
}

bool BuildFromJson(const json& root, const std::vector<std::vector<uint8_t>>& buffers,
                   std::vector<GltfMesh>& out, std::string& err) {
    out.clear();
    if (!root.contains("meshes") || root["meshes"].empty()) { err = "glTF has no meshes"; return false; }

    const auto& meshes = root["meshes"];
    for (size_t mi = 0; mi < meshes.size(); ++mi) {
        const auto& mesh = meshes[mi];
        if (!mesh.contains("primitives") || !mesh["primitives"].is_array()) continue;

        std::string meshName = mesh.value("name", std::string());
        if (meshName.empty()) meshName = "mesh_" + std::to_string(mi);

        // target names are per-mesh (mesh.extras.targetNames), shared by its primitives.
        std::vector<std::string> names;
        if (mesh.contains("extras") && mesh["extras"].is_object()) {
            const auto& ex = mesh["extras"];
            if (ex.contains("targetNames") && ex["targetNames"].is_array())
                for (const auto& n : ex["targetNames"]) if (n.is_string()) names.push_back(n.get<std::string>());
        }

        const auto& prims = mesh["primitives"];
        for (size_t pi = 0; pi < prims.size(); ++pi) {
            const auto& prim = prims[pi];
            if (!prim.contains("targets") || !prim["targets"].is_array() || prim["targets"].empty())
                continue;

            GltfMesh gm;
            gm.name = meshName;
            if (prims.size() > 1) gm.name += "#prim" + std::to_string(pi);

            if (prim.contains("attributes") && prim["attributes"].contains("POSITION")) {
                int posAcc = prim["attributes"]["POSITION"].get<int>();
                if (posAcc >= 0 && posAcc < (int)root["accessors"].size())
                    gm.vertexCount = root["accessors"][posAcc].value("count", 0);
            }

            const auto& targets = prim["targets"];
            for (size_t ti = 0; ti < targets.size(); ++ti) {
                const auto& t = targets[ti];
                MorphTarget mt;
                int pa = t.value("POSITION", -1);
                int na = t.value("NORMAL", -1);
                // Name: prefer the POSITION accessor's name (VRF stores the flex name there),
                // then mesh.extras.targetNames, then a generic fallback.
                if (pa >= 0 && pa < (int)root["accessors"].size()) {
                    const auto& acc = root["accessors"][pa];
                    if (acc.contains("name") && acc["name"].is_string() && !acc["name"].get<std::string>().empty())
                        mt.name = acc["name"].get<std::string>();
                }
                if (mt.name.empty() && ti < names.size()) mt.name = names[ti];
                if (mt.name.empty()) mt.name = "morph_" + std::to_string(ti);
                if (!ReadVec3Accessor(root, buffers, pa, mt.position, err)) return false;
                if (na >= 0) if (!ReadVec3Accessor(root, buffers, na, mt.normal, err)) return false;
                gm.targets.push_back(std::move(mt));
            }

            if (!gm.targets.empty()) out.push_back(std::move(gm));
        }
    }

    if (out.empty()) { err = "glTF has no primitives with morph targets"; return false; }
    return true;
}

} // namespace

bool Load(const std::string& path, std::vector<GltfMesh>& out, std::string& err) {
    std::vector<uint8_t> file;
    if (!ReadWholeFile(path, file)) { err = "cannot read: " + path; return false; }

    json root;
    std::vector<std::vector<uint8_t>> buffers;

    std::string jsonText, glbBinUnused;
    std::vector<uint8_t> bin;
    if (ParseGlb(file, jsonText, bin)) {
        try { root = json::parse(jsonText); }
        catch (const std::exception& e) { err = std::string("glTF JSON parse: ") + e.what(); return false; }
        buffers.push_back(std::move(bin));
    } else {
        try { root = json::parse(reinterpret_cast<const char*>(file.data()), reinterpret_cast<const char*>(file.data()) + file.size()); }
        catch (const std::exception& e) { err = std::string("glTF JSON parse: ") + e.what(); return false; }
        fs::path dir = fs::path(path).parent_path();
        if (root.contains("buffers")) {
            for (const auto& b : root["buffers"]) {
                std::vector<uint8_t> data;
                if (b.contains("uri") && b["uri"].is_string()) {
                    std::string uri = b["uri"].get<std::string>();
                    if (uri.rfind("data:", 0) == 0) { err = "data-uri buffers not supported"; return false; }
                    fs::path bp = dir / uri;
                    if (!ReadWholeFile(bp.string(), data)) { err = "cannot read buffer: " + bp.string(); return false; }
                }
                buffers.push_back(std::move(data));
            }
        }
    }
    return BuildFromJson(root, buffers, out, err);
}

} // namespace gltfmorph
