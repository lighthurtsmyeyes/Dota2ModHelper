#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace cloth_sim_restore {

struct ClothShape {
    virtual ~ClothShape() = default;
    std::string parentBone;
    std::string name;
    int collisionMask = 15;
    int collisionPriority = 0;
    bool invertedCollision = false;
    bool planarize = false;
    float bounciness = 0.0f;
    std::string vertexMap;
};

struct ClothSphere : ClothShape {
    float radius = 0.0f;
    std::array<float, 3> center{};
};

struct ClothCapsule : ClothShape {
    float radius0 = 0.0f;
    float radius1 = 0.0f;
    std::array<float, 3> point0{};
    std::array<float, 3> point1{};
};

struct ClothBox : ClothShape {
    std::array<float, 3> min{};
    std::array<float, 3> max{};
};

struct ClothSDF : ClothShape {
    std::string filename;
};

struct ParsedClothData {
    std::vector<std::unique_ptr<ClothShape>> shapes;
    std::vector<std::string> ctrlNames;
};

struct JiggleChainData {
    struct Group {
        std::string boneName;
        int n0 = -1;
        int n1 = -1;
        bool static0 = false;
        bool static1 = false;
        float radius0 = 0.0f;
        float radius1 = 0.0f;
        float fric0 = 0.0f;
        float fric1 = 0.0f;
        float forceAttr0 = 0.0f;
        float vertAttr0 = 0.0f;
        float forceAttr1 = 0.0f;
        float vertAttr1 = 0.0f;
        std::array<float, 3> pos0{};
        std::array<float, 3> pos1{};
    };
    std::vector<Group> groups; // in m_CtrlName order

    struct VtxMap {
        std::string name;
        uint32_t nameHash = 0;
        uint32_t color = 0xFFFFFFFF;
        float volumetricSolve = 0.0f;
        std::vector<std::pair<int, float>> nodeWeights; // original node index -> weight 0..1
    };
    std::vector<VtxMap> vertexMaps;

    struct Effect {
        std::string name;
        int64_t type = 0;
        float stiffness = 0.0f;
        float boneOverlay = 0.0f;
        uint32_t vertexMapHash = 0;
    };
    std::vector<Effect> effects;
};

bool ParsePhysBlock(const std::string& physBlockText, ParsedClothData& out, std::string& outLog);
bool ApplyClothSimRestore(const std::string& vmdlPath, const std::string& physBlockText, std::string& outLog);
bool ParseJiggleChainData(const std::string& physBlockText, JiggleChainData& out, std::string& outLog);

} // namespace cloth_sim_restore
