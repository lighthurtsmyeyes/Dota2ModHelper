#pragma once

#include <string>
#include <vector>

namespace model_helper {

struct ActivityModifierResult {
    bool success = false;
    std::string message;
    int matchedSourceAnimations = 0;
    int replacedTargetAnimations = 0;
};

ActivityModifierResult ApplyActivityModifiers(const std::string& vmdlPath,
                                              const std::vector<std::string>& userModifiers);

struct MergeMeshesOptions {
    std::string sourceVmdlPath;    // source .vmdl on disk (the model being modified)
    std::string targetVmdlPath;    // target .vmdl on disk (provides meshes/attachments/skeleton)
    std::string targetSearchRoot;  // root dir used to resolve target .dmx refs (content root or decompile output dir)
    std::string outputVmdlPath;    // where to write the merged .vmdl
};

struct MergeMeshesResult {
    bool success = false;
    std::string message;
    std::string log;
    int meshesAdded = 0;
    int attachmentsAdded = 0;
    int bonesAdded = 0;
    int lodReferencesAdded = 0;
};

MergeMeshesResult MergeMeshes(const MergeMeshesOptions& options);

} // namespace model_helper
