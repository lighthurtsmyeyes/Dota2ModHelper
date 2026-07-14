#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_map>

namespace decompiler_enhancements {

struct EnchantmentFlags {
    static constexpr int None = 0;
    static constexpr int FixAnimationOrder = 1 << 0;
    static constexpr int RestoreActivityModifiers = 1 << 1;
    static constexpr int RestoreWeightLists = 1 << 2;
    static constexpr int RestoreBlendAnimations = 1 << 3;
    static constexpr int RestoreMorphs = 1 << 4;
    static constexpr int Count = 5;
};

struct EnchantmentInfo {
    int flag;
    const char* label;
    const char* description;
};

inline const std::vector<EnchantmentInfo>& GetEnchantments() {
    static const std::vector<EnchantmentInfo> kEnchantments = {
        { EnchantmentFlags::FixAnimationOrder, "Fix animation order", "Add an AnimOrder node that matches the ASEQ block order instead of rearranging AnimationList children." },
        { EnchantmentFlags::RestoreActivityModifiers, "Restore activity modifiers", "Add ActivityModifier children for every activity listed in ASEQ beyond the primary one." },
        { EnchantmentFlags::RestoreWeightLists, "Restore weight lists", "Add weight_list_name to AnimFile nodes using the ASEQ m_nLocalWeightlist index." },
        { EnchantmentFlags::RestoreBlendAnimations, "Restore blend animations", "Convert AnimFile nodes that are 1D/2D blends into 1DBlend/2DBlend nodes using ASEQ pose parameters." },
        { EnchantmentFlags::RestoreMorphs, "Restore morphs (facial flex)", "Re-inject facial flexes into every render mesh .dmx referenced by the .vmdl (all LODs / body groups) by merging morph targets from a glTF export. Writes DmeVertexDeltaData (shapes) + DmeCombinationInputControl (channels) + DmeCombinationOperator; corrective ('A__B') shapes are kept as shapes only, and meshes with no matching morph data (e.g. lower LODs) are skipped." },
    };
    return kEnchantments;
}

struct SequenceInfo {
	std::string name;
	std::vector<std::pair<std::string, int>> activities;
	std::string weightListName;

    // Blend animation data parsed from ASEQ m_fetch.
    enum class BlendType { None, Blend1D, Blend2D, Blend2DTri };
    BlendType blendType = BlendType::None;
    std::vector<int> referenceArray;
    std::array<int, 2> groupSize = { 1, 1 };
    std::array<int, 2> localPose = { -1, -1 };
    std::vector<float> poseKeyArray0;
    std::vector<float> poseKeyArray1;
};

bool ApplyEnhancements(const std::string& vmdlPath, const std::string& compiledInputPath,
                       const std::string& aseqBlockText, int enchantmentFlags,
                       std::string& outLog);

} // namespace decompiler_enhancements
