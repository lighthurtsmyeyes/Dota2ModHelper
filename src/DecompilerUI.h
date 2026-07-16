#pragma once

#include <string>
#include <functional>
#include <mutex>

namespace decompiler_ui {

struct DecompilerConfig {
    char inputPath[512] = "models/heroes/axe/axe.vmdl";
    char outputDir[512] = "decompiled_models";
    char inputVpkEntry[512] = "";

    bool recursive = false;
    bool recursiveVpk = false;
    char vpkExtensions[128] = "";
    char vpkFilepath[256] = "";
    bool vpkCache = false;
    bool vpkVerify = false;

    bool allBlocks = false;
    char blockName[64] = "DATA";
    bool vpkDecompile = true;
    int textureDecodeFlags = 0; // 0=auto, 1=none, 2=ForceLDR
    bool vpkList = false;
    bool vpkDir = false;

    int gltfExportFormat = 0; // 0=none, 1=gltf, 2=glb
    bool gltfExportMaterials = true;
    bool gltfExportAnimations = true;
    char gltfMeshList[256] = "";
    char gltfAnimationList[256] = "";
    bool gltfTexturesAdapt = false;
    bool gltfExportExtras = false;

    bool toolsAssetInfoShort = false;
    int threads = 1;

    int enchantmentFlags = 0; // bit mask, see DecompilerEnhancements.h

    char modelHelperVmdlPath[512] = "";

    char mergeTargetPath[512] = "";
    char mergeTargetVpkEntry[512] = "";

    char transferTargetPath[512] = "";
    char transferTargetVpkEntry[512] = "";
};

struct DecompilerState {
    DecompilerConfig config;

    std::vector<std::string> modelHelperModifiers;

    std::mutex mutex;
    bool busy = false;
    float progress = 0.0f;
    std::string status;
    std::string lastError;
    bool showStatus = false;
    std::string lastDecompilerOutput;

    std::function<void()> onCompletion;
};

void DrawDecompilerUI(std::shared_ptr<DecompilerState> state);
void LoadDecompilerState(DecompilerState& state);
void SaveDecompilerState(const DecompilerState& state);

} // namespace decompiler_ui
