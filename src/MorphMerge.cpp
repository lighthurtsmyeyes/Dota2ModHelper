#include "MorphMerge.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <unordered_set>

namespace morphmerge {

namespace {

std::array<uint8_t, 16> MakeGuid(uint32_t seed) {
    std::mt19937 rng(seed);
    std::array<uint8_t, 16> g{};
    for (int i = 0; i < 16; i += 4) {
        uint32_t r = rng();
        g[i] = r & 0xFF; g[i+1] = (r >> 8) & 0xFF;
        g[i+2] = (r >> 16) & 0xFF; g[i+3] = (r >> 24) & 0xFF;
    }
    return g;
}

struct Str {
    dmxbin::Doc& d;
    uint32_t operator()(const std::string& s) { return d.AddString(s); }
};

std::array<float,3> Xf(std::array<float,3> v, bool apply) {
    return apply ? gltfmorph::GltfToSource(v) : v;
}

} // namespace

std::vector<int> GetMeshElements(const dmxbin::Doc& doc) {
    std::vector<int> out;
    uint32_t tDmeMesh = doc.FindString("DmeMesh");
    if (tDmeMesh == dmxbin::kNullElement) return out;
    for (int i = 0; i < (int)doc.elems.size(); ++i)
        if (doc.elems[i].type == tDmeMesh) out.push_back(i);
    return out;
}

int GetBindVertexCount(const dmxbin::Doc& doc, int meshIdx) {
    if (meshIdx < 0 || meshIdx >= (int)doc.elems.size()) return -1;

    auto posCountOf = [&](uint32_t elemIdx) -> int {
        if (elemIdx == dmxbin::kNullElement || elemIdx >= doc.elems.size()) return -1;
        uint32_t aPos = doc.FindString("position$0");
        if (aPos == dmxbin::kNullElement) return -1;
        const dmxbin::Attr* pa = doc.FindAttr(doc.elems[elemIdx], aPos);
        std::vector<std::array<float,3>> tmp;
        if (pa && dmxbin::DecodeVec3Array(*pa, tmp)) return (int)tmp.size();
        return -1;
    };
    auto readElem = [&](const char* attrName) -> uint32_t {
        uint32_t ni = doc.FindString(attrName);
        if (ni == dmxbin::kNullElement) return dmxbin::kNullElement;
        const dmxbin::Attr* a = doc.FindAttr(doc.elems[meshIdx], ni);
        uint32_t idx = dmxbin::kNullElement;
        if (a) dmxbin::DecodeElement(*a, idx);
        return idx;
    };

    // VRF-decompiled meshes often leave bindState null and store the bind pose in
    // currentState (and baseStates[0]). Try them in order.
    int c = posCountOf(readElem("bindState"));    if (c >= 0) return c;
    c = posCountOf(readElem("currentState"));      if (c >= 0) return c;
    {
        uint32_t aBase = doc.FindString("baseStates");
        if (aBase != dmxbin::kNullElement) {
            const dmxbin::Attr* ba = doc.FindAttr(doc.elems[meshIdx], aBase);
            std::vector<uint32_t> arr;
            if (ba && dmxbin::DecodeElementArray(*ba, arr) && !arr.empty()) {
                c = posCountOf(arr[0]); if (c >= 0) return c;
            }
        }
    }
    return -1;
}

int GetBindVertexCount(const dmxbin::Doc& doc) {
    std::vector<int> meshes = GetMeshElements(doc);
    if (meshes.empty()) return -1;
    return GetBindVertexCount(doc, meshes.front());
}

std::string GetMeshName(const dmxbin::Doc& doc, int meshIdx) {
    if (meshIdx < 0 || meshIdx >= (int)doc.elems.size()) return {};
    const dmxbin::Element& e = doc.elems[meshIdx];
    uint32_t tDmeMesh = doc.FindString("DmeMesh");
    if (tDmeMesh == dmxbin::kNullElement || e.type != tDmeMesh) return {};
    if (e.name < doc.strings.size()) return doc.strings[e.name];
    return {};
}

std::string GetMeshName(const dmxbin::Doc& doc) {
    std::vector<int> meshes = GetMeshElements(doc);
    if (meshes.empty()) return {};
    return GetMeshName(doc, meshes.front());
}

bool Merge(dmxbin::Doc& doc, const std::vector<MeshTarget>& targets, const Options& opt, std::string& log) {
    std::ostringstream L;

    std::vector<MeshTarget> valid;
    for (const auto& t : targets) {
        if (t.elem < 0 || t.elem >= (int)doc.elems.size() || !t.gm) continue;
        if (t.gm->targets.empty()) continue;
        valid.push_back(t);
    }
    if (valid.empty()) { log = "no morph targets in glTF"; return false; }

    Str S{ doc };
    uint32_t tDelta = S("DmeVertexDeltaData");
    uint32_t tCombo = S("DmeCombinationOperator");
    uint32_t tCtrl = S("DmeCombinationInputControl");

    // attribute-name string indices
    uint32_t aVertexFormat = S("vertexFormat");
    uint32_t aPosition = S("position$0"), aPositionIdx = S("position$0Indices");
    uint32_t aNormal = S("normal$0"), aNormalIdx = S("normal$0Indices");
    uint32_t aRawNames = S("rawControlNames"), aStereo = S("stereo"), aEyelid = S("eyelid");
    uint32_t aFlexMin = S("flexMin"), aFlexMax = S("flexMax"), aWrinkle = S("wrinkleScales");
    uint32_t aControls = S("controls"), aControlValues = S("controlValues");
    uint32_t aControlValuesLagged = S("controlValuesLagged"), aUsesLagged = S("usesLaggedValues");
    uint32_t aDominators = S("dominators"), aTargets = S("targets");
    uint32_t aDeltaStates = S("deltaStates"), aDeltaWeights = S("deltaStateWeights");
    uint32_t aDeltaWeightsLagged = S("deltaStateWeightsLagged"), aBaseStates = S("baseStates");
    uint32_t sComboName = S("combinationOperator");

    size_t totalElems = 0;
    for (const auto& t : valid) totalElems += t.gm->targets.size();
    doc.elems.reserve(doc.elems.size() + totalElems * 2 + 1);

    uint32_t guidSeed = 0xC0FFEEu;
    int totalPosVerts = 0, totalNrmVerts = 0;
    int totalCorrectives = 0;
    std::vector<std::vector<uint32_t>> deltasPerMesh;
    std::vector<int> patchedMeshes;
    std::vector<std::string> channelNames; // from the first merged mesh's target list

    for (size_t ti = 0; ti < valid.size(); ++ti) {
        const gltfmorph::GltfMesh& mesh = *valid[ti].gm;
        const int meshIdx = valid[ti].elem;
        const int bindVertCount = GetBindVertexCount(doc, meshIdx);

        // Resolve the bind-pose vertex-data element (what baseStates should point at).
        // VRF meshes store the bind pose in currentState and leave bindState null.
        auto readElem = [&](const char* attrName) -> uint32_t {
            uint32_t ni = doc.FindString(attrName);
            if (ni == dmxbin::kNullElement) return dmxbin::kNullElement;
            const dmxbin::Attr* a = doc.FindAttr(doc.elems[meshIdx], ni);
            uint32_t idx = dmxbin::kNullElement;
            if (a) dmxbin::DecodeElement(*a, idx);
            return idx;
        };
        auto validElem = [&](uint32_t idx) { return idx != dmxbin::kNullElement && idx < doc.elems.size(); };
        uint32_t bindIdx = readElem("bindState");
        if (!validElem(bindIdx)) bindIdx = readElem("currentState");
        if (!validElem(bindIdx)) {
            uint32_t aBaseLookup = doc.FindString("baseStates");
            if (aBaseLookup != dmxbin::kNullElement) {
                const dmxbin::Attr* ba = doc.FindAttr(doc.elems[meshIdx], aBaseLookup);
                std::vector<uint32_t> arr;
                if (ba && dmxbin::DecodeElementArray(*ba, arr) && !arr.empty()) bindIdx = arr[0];
            }
        }

        std::vector<uint32_t> deltaIndices;
        const int N = (int)mesh.targets.size();
        deltaIndices.reserve(N);
        int meshCorrectives = 0;

        for (int k = 0; k < N; ++k) {
            const gltfmorph::MorphTarget& mt = mesh.targets[k];
            const std::string mtName = mt.name.empty() ? ("morph_" + std::to_string(k)) : mt.name;
            const bool corrective = mtName.find("__") != std::string::npos;
            if (ti == 0 && !corrective) channelNames.push_back(mtName);

            std::vector<int32_t> pIdx; std::vector<std::array<float,3>> pVal;
            std::vector<int32_t> nIdx; std::vector<std::array<float,3>> nVal;

            int pn = (int)mt.position.size();
            for (int i = 0; i < pn; ++i) {
                if (bindVertCount > 0 && i >= bindVertCount) break;
                auto v = Xf(mt.position[i], opt.applyAxisTransform);
                float m = std::fabs(v[0]) + std::fabs(v[1]) + std::fabs(v[2]);
                if (m > opt.posEps) { pIdx.push_back(i); pVal.push_back(v); }
            }
            int nn = (int)mt.normal.size();
            for (int i = 0; i < nn; ++i) {
                if (bindVertCount > 0 && i >= bindVertCount) break;
                auto v = Xf(mt.normal[i], opt.applyAxisTransform);
                float m = std::fabs(v[0]) + std::fabs(v[1]) + std::fabs(v[2]);
                if (m > opt.nrmEps) { nIdx.push_back(i); nVal.push_back(v); }
            }

            std::vector<std::string> vfmt;
            if (!pIdx.empty()) vfmt.push_back("position$0");
            if (!nIdx.empty()) vfmt.push_back("normal$0");
            if (vfmt.empty()) continue; // target has no non-zero deltas on this mesh; skip shape

            uint32_t nameIdx = S(mtName);
            dmxbin::Element delta;
            delta.type = tDelta; delta.name = nameIdx; delta.guid = MakeGuid(guidSeed++);
            dmxbin::SetAttr(delta, dmxbin::MakeStringArray(aVertexFormat, vfmt));
            if (!pIdx.empty()) {
                dmxbin::SetAttr(delta, dmxbin::MakeVec3Array(aPosition, pVal));
                dmxbin::SetAttr(delta, dmxbin::MakeIntArray(aPositionIdx, pIdx));
            }
            if (!nIdx.empty()) {
                dmxbin::SetAttr(delta, dmxbin::MakeVec3Array(aNormal, nVal));
                dmxbin::SetAttr(delta, dmxbin::MakeIntArray(aNormalIdx, nIdx));
            }
            deltaIndices.push_back((uint32_t)doc.elems.size());
            doc.elems.push_back(std::move(delta));
            if (corrective) ++meshCorrectives;

            totalPosVerts += (int)pIdx.size();
            totalNrmVerts += (int)nIdx.size();
        }

        if (deltaIndices.empty()) {
            L << "mesh[" << meshIdx << "] '" << GetMeshName(doc, meshIdx)
              << "': no non-empty morph deltas produced; ";
            continue;
        }
        totalCorrectives += meshCorrectives;

        // Patch DmeMesh (re-acquire reference: appends above may have reallocated).
        dmxbin::Element& meshEl = doc.elems[meshIdx];
        std::vector<std::array<float,2>> zeros2delta(deltaIndices.size(), {0.f,0.f});
        dmxbin::SetAttr(meshEl, dmxbin::MakeElementArray(aDeltaStates, deltaIndices));
        dmxbin::SetAttr(meshEl, dmxbin::MakeVec2Array(aDeltaWeights, zeros2delta));
        dmxbin::SetAttr(meshEl, dmxbin::MakeVec2Array(aDeltaWeightsLagged, zeros2delta));
        if (bindIdx != dmxbin::kNullElement) {
            std::vector<uint32_t> existingBase;
            const dmxbin::Attr* ba = doc.FindAttr(meshEl, aBaseStates);
            if (ba) dmxbin::DecodeElementArray(*ba, existingBase);
            if (existingBase.empty())
                dmxbin::SetAttr(meshEl, dmxbin::MakeElementArray(aBaseStates, std::vector<uint32_t>{ bindIdx }));

            // VRF-decompiled meshes store the bind pose in currentState and leave bindState
            // null. The morph base is resolved through bindState, and the authoring/reference
            // layout always has it set; point it at the bind-pose vertex data when missing.
            uint32_t aBindStateAttr = S("bindState");
            const dmxbin::Attr* existingBind = doc.FindAttr(meshEl, aBindStateAttr);
            uint32_t existingBindIdx = dmxbin::kNullElement;
            if (existingBind) dmxbin::DecodeElement(*existingBind, existingBindIdx);
            if (existingBindIdx == dmxbin::kNullElement || existingBindIdx >= doc.elems.size())
                dmxbin::SetAttr(meshEl, dmxbin::MakeElement(aBindStateAttr, bindIdx));
        }

        deltasPerMesh.push_back(std::move(deltaIndices));
        patchedMeshes.push_back(meshIdx);
    }

    if (patchedMeshes.empty()) { log = "no non-empty morph deltas produced"; return false; }

    // Shared channels (DmeCombinationInputControl), one per non-corrective morph
    // name. Combination/corrective shapes (e.g. "browLowerer__noseWrinkler") are
    // driven by the combination operator from their "__" parent controls and must
    // NOT be exposed as their own control; the model compiler infers the
    // corrective rule from the "__" name.
    std::vector<uint32_t> ctrlIndices;
    ctrlIndices.reserve(channelNames.size());
    for (const std::string& cn : channelNames) {
        uint32_t nameIdx = S(cn);
        dmxbin::Element ctrl;
        ctrl.type = tCtrl; ctrl.name = nameIdx; ctrl.guid = MakeGuid(guidSeed++);
        dmxbin::SetAttr(ctrl, dmxbin::MakeStringArray(aRawNames, std::vector<std::string>{ cn }));
        dmxbin::SetAttr(ctrl, dmxbin::MakeBool(aStereo, false));
        dmxbin::SetAttr(ctrl, dmxbin::MakeBool(aEyelid, false));
        dmxbin::SetAttr(ctrl, dmxbin::MakeFloat(aFlexMin, 0.0f));
        dmxbin::SetAttr(ctrl, dmxbin::MakeFloat(aFlexMax, 1.0f));
        dmxbin::SetAttr(ctrl, dmxbin::MakeFloatArray(aWrinkle, std::vector<float>{ 0.0f }));
        ctrlIndices.push_back((uint32_t)doc.elems.size());
        doc.elems.push_back(std::move(ctrl));
    }

    // One combination operator for all patched meshes. controlValues /
    // controlValuesLagged are per-control; deltaStateWeights(Lagged) on each
    // mesh are per-delta.
    std::vector<std::array<float,3>> zeros3ctrl(ctrlIndices.size(), {0.f,0.f,0.f});
    dmxbin::Element combo;
    combo.type = tCombo; combo.name = sComboName; combo.guid = MakeGuid(guidSeed++);
    dmxbin::SetAttr(combo, dmxbin::MakeElementArray(aControls, ctrlIndices));
    dmxbin::SetAttr(combo, dmxbin::MakeVec3Array(aControlValues, zeros3ctrl));
    dmxbin::SetAttr(combo, dmxbin::MakeVec3Array(aControlValuesLagged, zeros3ctrl));
    dmxbin::SetAttr(combo, dmxbin::MakeBool(aUsesLagged, false));
    dmxbin::SetAttr(combo, dmxbin::MakeElementArray(aDominators, std::vector<uint32_t>{}));
    dmxbin::SetAttr(combo, dmxbin::MakeElementArray(aTargets,
        std::vector<uint32_t>(patchedMeshes.begin(), patchedMeshes.end())));
    doc.elems.push_back(std::move(combo));

    size_t nDelta = 0;
    for (const auto& v : deltasPerMesh) nDelta += v.size();
    L << "merged " << nDelta << " morph shapes / " << ctrlIndices.size() << " channels into "
      << patchedMeshes.size() << " mesh(es)";
    if (totalCorrectives > 0) L << " (" << totalCorrectives << " corrective shapes kept as shapes only)";
    L << " (posDeltaVerts=" << totalPosVerts << ", nrmDeltaVerts=" << totalNrmVerts << ")";
    log = L.str();
    return true;
}

bool Merge(dmxbin::Doc& doc, const gltfmorph::GltfMesh& mesh, const Options& opt, std::string& log) {
    std::vector<int> meshes = GetMeshElements(doc);
    if (meshes.empty()) { log = "base DMX has no DmeMesh element"; return false; }
    return Merge(doc, std::vector<MeshTarget>{ MeshTarget{ meshes.front(), &mesh } }, opt, log);
}

void StripMorphs(dmxbin::Doc& doc) {
    std::unordered_set<uint32_t> kill;
    auto add = [&](const char* n) { uint32_t i = doc.FindString(n); if (i != dmxbin::kNullElement) kill.insert(i); };
    add("DmeVertexDeltaData");
    add("DmeCombinationInputControl");
    add("DmeCombinationOperator");

    std::vector<dmxbin::Element> kept;
    kept.reserve(doc.elems.size());
    for (size_t i = 0; i < doc.elems.size(); ++i) {
        if (kill.count(doc.elems[i].type)) continue;
        kept.push_back(std::move(doc.elems[i]));
    }
    doc.elems = std::move(kept);

    // VRF-decompiled meshes append morph elements at high indices, after the mesh
    // and its vertex data, so dropping them does not shift surviving low-index
    // references (bindState/currentState/baseStates->DmeVertexData). Clear the
    // mesh morph attrs; Merge rebuilds them from the glTF deltas.
    // `baseStates` must NOT be cleared: it is the bind-pose reference, not a
    // morph attr, and any DmeMesh left without it (e.g. a second mesh group in
    // the same file that Merge does not re-add it to) makes resourcecompiler /
    // modeldoc crash with an access violation.
    uint32_t tMesh = doc.FindString("DmeMesh");
    const char* clearAttrs[] = { "deltaStates", "deltaStateWeights", "deltaStateWeightsLagged", "combinationOperator" };
    for (auto& e : doc.elems) {
        if (e.type != tMesh) continue;
        for (const char* n : clearAttrs) {
            uint32_t ni = doc.FindString(n); if (ni == dmxbin::kNullElement) continue;
            e.attrs.erase(std::remove_if(e.attrs.begin(), e.attrs.end(),
                [&](const dmxbin::Attr& x) { return x.name == ni; }), e.attrs.end());
        }
    }
}

void WireCombinationOperatorToScene(dmxbin::Doc& doc) {
    Str S{ doc };

    uint32_t tCombo = doc.FindString("DmeCombinationOperator");
    int comboIdx = -1;
    if (tCombo != dmxbin::kNullElement)
        for (int i = 0; i < (int)doc.elems.size(); ++i)
            if (doc.elems[i].type == tCombo) { comboIdx = i; break; }
    if (comboIdx < 0) return;

    uint32_t aCombo = S("combinationOperator");

    for (int i = 0; i < (int)doc.elems.size(); ++i) {
        if (i == comboIdx) continue;
        const dmxbin::Attr* a = doc.FindAttr(doc.elems[i], aCombo);
        uint32_t idx = dmxbin::kNullElement;
        if (a && dmxbin::DecodeElement(*a, idx) && idx == (uint32_t)comboIdx) return;
    }

    uint32_t tModel = doc.FindString("DmeModel");
    int modelIdx = -1;
    if (tModel != dmxbin::kNullElement)
        for (int i = 0; i < (int)doc.elems.size(); ++i)
            if (doc.elems[i].type == tModel) { modelIdx = i; break; }

    auto references = [&](int e, int target) {
        for (const auto& a : doc.elems[e].attrs) {
            if (a.type == dmxbin::AT_ELEMENT) {
                uint32_t v; if (dmxbin::DecodeElement(a, v) && v == (uint32_t)target) return true;
            } else if (a.type == dmxbin::AT_ELEMENT_ARRAY) {
                std::vector<uint32_t> v;
                if (dmxbin::DecodeElementArray(a, v))
                    for (auto x : v) if (x == (uint32_t)target) return true;
            }
        }
        return false;
    };

    int rootIdx = -1;
    if (modelIdx >= 0) {
        for (int i = 0; i < (int)doc.elems.size(); ++i)
            if (i != modelIdx && references(i, modelIdx)) { rootIdx = i; break; }
    }
    if (rootIdx < 0) {
        uint32_t tDelta = doc.FindString("DmeVertexDeltaData");
        uint32_t tCtrl  = doc.FindString("DmeCombinationInputControl");
        std::vector<char> referenced(doc.elems.size(), 0);
        for (int i = 0; i < (int)doc.elems.size(); ++i)
            for (const auto& a : doc.elems[i].attrs) {
                if (a.type == dmxbin::AT_ELEMENT) {
                    uint32_t v; if (dmxbin::DecodeElement(a, v) && v < referenced.size()) referenced[v] = 1;
                } else if (a.type == dmxbin::AT_ELEMENT_ARRAY) {
                    std::vector<uint32_t> v;
                    if (dmxbin::DecodeElementArray(a, v))
                        for (auto x : v) if (x < referenced.size()) referenced[x] = 1;
                }
            }
        for (int i = 0; i < (int)doc.elems.size(); ++i) {
            if (referenced[i] || i == comboIdx) continue;
            uint32_t ty = doc.elems[i].type;
            if (ty == tDelta || ty == tCtrl || ty == tCombo) continue;
            rootIdx = i; break;
        }
    }
    if (rootIdx < 0) rootIdx = 0;
    if (rootIdx == comboIdx) return;

    dmxbin::Element& root = doc.elems[rootIdx];
    uint32_t tRoot = doc.FindString("DmeRoot");
    if (tRoot != dmxbin::kNullElement && root.type == tRoot) {
        root.type = S("DmElement");
        root.name = S("Scene");
    }
    dmxbin::SetAttr(root, dmxbin::MakeElement(aCombo, (uint32_t)comboIdx));
}

} // namespace morphmerge
