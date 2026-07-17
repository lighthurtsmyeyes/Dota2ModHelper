#include "ClothSimRestore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "DmxBinary.h"

namespace fs = std::filesystem;

namespace kv3t {

struct Value {
    enum class Kind { Scalar, Object, Array };
    Kind kind = Kind::Scalar;
    std::string scalar;
    std::vector<std::pair<std::string, Value>> obj;
    std::vector<Value> arr;

    bool isObject() const { return kind == Kind::Object; }
    bool isArray() const { return kind == Kind::Array; }
    bool isScalar() const { return kind == Kind::Scalar; }

    const Value* find(const std::string& key) const {
        if (kind != Kind::Object) return nullptr;
        for (const auto& kv : obj) if (kv.first == key) return &kv.second;
        return nullptr;
    }
    double asDouble(double def = 0.0) const {
        if (kind != Kind::Scalar) return def;
        try { return std::stod(scalar); } catch (...) { return def; }
    }
    int64_t asInt(int64_t def = 0) const {
        if (kind != Kind::Scalar) return def;
        try { return std::stoll(scalar); } catch (...) { return def; }
    }
    std::vector<double> asDoubleArray() const {
        std::vector<double> r;
        if (kind != Kind::Array) return r;
        r.reserve(arr.size());
        for (const auto& v : arr) r.push_back(v.asDouble());
        return r;
    }
    std::vector<int64_t> asIntArray() const {
        std::vector<int64_t> r;
        if (kind != Kind::Array) return r;
        r.reserve(arr.size());
        for (const auto& v : arr) r.push_back(v.asInt());
        return r;
    }
    std::vector<std::string> asStringArray() const {
        std::vector<std::string> r;
        if (kind != Kind::Array) return r;
        r.reserve(arr.size());
        for (const auto& v : arr) r.push_back(v.scalar);
        return r;
    }
};

class Parser {
public:
    explicit Parser(std::string_view text) : t_(text) {}

    bool parseDocument(Value& out) {
        skipWs();
        while (pos_ < t_.size() && t_[pos_] != '{') {
            size_t nl = t_.find('\n', pos_);
            if (nl == std::string_view::npos) break;
            pos_ = nl + 1;
            skipWs();
        }
        if (pos_ >= t_.size() || t_[pos_] != '{') return false;
        return parseValue(out);
    }

private:
    std::string_view t_;
    size_t pos_ = 0;

    void skipWs() {
        for (;;) {
            while (pos_ < t_.size() && std::isspace(static_cast<unsigned char>(t_[pos_]))) ++pos_;
            if (pos_ + 1 < t_.size() && t_[pos_] == '/' && t_[pos_ + 1] == '/') {
                while (pos_ < t_.size() && t_[pos_] != '\n') ++pos_;
                continue;
            }
            break;
        }
    }

    bool parseValue(Value& out) {
        skipWs();
        if (pos_ >= t_.size()) return false;
        char c = t_[pos_];
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        return parseScalar(out);
    }

    bool parseObject(Value& out) {
        out.kind = Value::Kind::Object;
        ++pos_;
        for (;;) {
            skipWs();
            if (pos_ >= t_.size()) return false;
            if (t_[pos_] == '}') { ++pos_; return true; }
            std::string key;
            if (!parseKey(key)) return false;
            skipWs();
            if (pos_ >= t_.size() || t_[pos_] != '=') return false;
            ++pos_;
            Value v;
            if (!parseValue(v)) return false;
            out.obj.emplace_back(std::move(key), std::move(v));
        }
    }

    bool parseArray(Value& out) {
        out.kind = Value::Kind::Array;
        ++pos_;
        for (;;) {
            skipWs();
            if (pos_ >= t_.size()) return false;
            if (t_[pos_] == ']') { ++pos_; return true; }
            if (t_[pos_] == ',') { ++pos_; continue; }
            Value v;
            if (!parseValue(v)) return false;
            out.arr.push_back(std::move(v));
        }
    }

    bool parseKey(std::string& out) {
        skipWs();
        if (pos_ < t_.size() && t_[pos_] == '"') return parseQuoted(out);
        size_t start = pos_;
        while (pos_ < t_.size()) {
            char c = t_[pos_];
            if (std::isspace(static_cast<unsigned char>(c)) || c == '=') break;
            ++pos_;
        }
        if (pos_ == start) return false;
        out = std::string(t_.substr(start, pos_ - start));
        return true;
    }

    bool parseQuoted(std::string& out) {
        if (pos_ >= t_.size() || t_[pos_] != '"') return false;
        ++pos_;
        std::string r;
        while (pos_ < t_.size()) {
            char c = t_[pos_];
            if (c == '\\' && pos_ + 1 < t_.size()) {
                r.push_back(t_[pos_ + 1]);
                pos_ += 2;
                continue;
            }
            if (c == '"') { ++pos_; out = std::move(r); return true; }
            r.push_back(c);
            ++pos_;
        }
        return false;
    }

    bool parseScalar(Value& out) {
        out.kind = Value::Kind::Scalar;
        skipWs();
        if (pos_ < t_.size() && t_[pos_] == '"') return parseQuoted(out.scalar);
        size_t start = pos_;
        while (pos_ < t_.size()) {
            char c = t_[pos_];
            if (c == ',' || c == ']' || c == '}' || std::isspace(static_cast<unsigned char>(c))) break;
            ++pos_;
        }
        if (pos_ == start) return false;
        out.scalar = std::string(t_.substr(start, pos_ - start));
        return true;
    }
};

inline bool Parse(std::string_view text, Value& out) {
    Parser p(text);
    return p.parseDocument(out);
}

} // namespace kv3t

namespace cloth_sim_restore {

namespace {

std::string_view Trim(std::string_view s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

size_t LineStart(const std::string& text, size_t pos) {
    if (pos >= text.size()) return 0;
    while (pos > 0 && text[pos - 1] != '\n') --pos;
    return pos;
}

std::string ExtractIndent(const std::string& text, size_t lineStart) {
    size_t i = lineStart;
    while (i < text.size() && (text[i] == '\t' || text[i] == ' ')) ++i;
    return text.substr(lineStart, i - lineStart);
}

bool FindBalancedBlock(const std::string& text, size_t startPos, char openChar, char closeChar,
                       size_t& outStart, size_t& outEnd) {
    if (startPos >= text.size() || text[startPos] != openChar) return false;
    outStart = startPos;
    int depth = 0;
    bool inString = false;
    for (size_t i = startPos; i < text.size(); ++i) {
        char c = text[i];
        if (inString) {
            if (c == '\\' && i + 1 < text.size()) {
                ++i;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == openChar) {
            ++depth;
        } else if (c == closeChar) {
            --depth;
            if (depth == 0) {
                outEnd = i + 1;
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> ParseStringArray(const std::string& text, size_t start, size_t end) {
    std::vector<std::string> result;
    size_t pos = start;
    while (pos < end) {
        size_t quote = text.find('"', pos);
        if (quote == std::string::npos || quote >= end) break;
        size_t endQuote = text.find('"', quote + 1);
        if (endQuote == std::string::npos || endQuote > end) break;
        result.push_back(text.substr(quote + 1, endQuote - quote - 1));
        pos = endQuote + 1;
    }
    return result;
}

std::vector<float> ParseFloatArray(const std::string& text, size_t start, size_t end) {
    std::vector<float> result;
    size_t pos = start;
    while (pos < end) {
        while (pos < end && (std::isspace(static_cast<unsigned char>(text[pos])) || text[pos] == ',')) ++pos;
        if (pos >= end) break;
        if (text[pos] == ']' || text[pos] == '[' || text[pos] == '{') break;
        size_t numStart = pos;
        if (text[pos] == '-' || text[pos] == '+') ++pos;
        while (pos < end && (std::isdigit(static_cast<unsigned char>(text[pos])) || text[pos] == '.')) ++pos;
        if (numStart == pos) break;
        try {
            result.push_back(std::stof(text.substr(numStart, pos - numStart)));
        } catch (...) {
            break;
        }
    }
    return result;
}

std::vector<std::string> SplitTopLevelBlocks(const std::string& text, size_t start, size_t end) {
    std::vector<std::string> result;
    size_t pos = start;
    while (pos < end) {
        size_t brace = text.find('{', pos);
        if (brace == std::string::npos || brace >= end) break;
        size_t bStart, bEnd;
        if (!FindBalancedBlock(text, brace, '{', '}', bStart, bEnd) || bEnd > end) break;
        size_t lineStart = LineStart(text, brace);
        result.push_back(text.substr(lineStart, bEnd - lineStart));
        pos = bEnd;
    }
    return result;
}

bool ExtractKeyInt(const std::string& text, size_t pos, const char* key, int& outValue) {
    std::string keyEq = std::string(key) + " = ";
    size_t p = text.find(keyEq, pos);
    if (p == std::string::npos) {
        keyEq = std::string(key) + "=";
        p = text.find(keyEq, pos);
    }
    if (p == std::string::npos) return false;
    size_t numStart = p + keyEq.size();
    while (numStart < text.size() && std::isspace(static_cast<unsigned char>(text[numStart]))) ++numStart;
    size_t numEnd = numStart;
    if (numEnd < text.size() && (text[numEnd] == '-' || text[numEnd] == '+')) ++numEnd;
    while (numEnd < text.size() && std::isdigit(static_cast<unsigned char>(text[numEnd]))) ++numEnd;
    if (numStart == numEnd) return false;
    try {
        outValue = std::stoi(text.substr(numStart, numEnd - numStart));
        return true;
    }
    catch (...) {
        return false;
    }
}

bool IsKV3Balanced(const std::string& text) {
    int depthBrace = 0, depthBracket = 0, depthParen = 0;
    bool inString = false;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (inString) {
            if (c == '\\' && i + 1 < text.size()) {
                ++i;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == '{') {
            ++depthBrace;
        } else if (c == '}') {
            --depthBrace;
        } else if (c == '[') {
            ++depthBracket;
        } else if (c == ']') {
            --depthBracket;
        } else if (c == '(') {
            ++depthParen;
        } else if (c == ')') {
            --depthParen;
        }
        if (depthBrace < 0 || depthBracket < 0 || depthParen < 0) return false;
    }
    return depthBrace == 0 && depthBracket == 0 && depthParen == 0;
}

std::string FormatKVFloat(float v) {
    std::string s = std::to_string(v);
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        size_t end = s.size();
        while (end > dot + 2 && s[end - 1] == '0') --end;
        s.resize(end);
    } else {
        s += ".0";
    }
    return s;
}

std::string FormatVec3(const std::array<float, 3>& v) {
    return "[ " + FormatKVFloat(v[0]) + ", " + FormatKVFloat(v[1]) + ", " + FormatKVFloat(v[2]) + " ]";
}

std::string BuildShapeBlock(const ClothShape* shape, const std::string& shapeIndent,
                            const std::string& entryIndent) {
    std::string fieldIndent = shapeIndent;
    std::string block;
    block += entryIndent + "{\n";

    auto addField = [&](const std::string& indent, std::string& b, const std::string& key, const std::string& value) {
        b += indent + key + " = " + value + "\n";
    };

    bool layer0 = (shape->collisionMask & 1) != 0;
    bool layer1 = (shape->collisionMask & 2) != 0;
    bool layer2 = (shape->collisionMask & 4) != 0;
    bool layer3 = (shape->collisionMask & 8) != 0;

    addField(fieldIndent, block, "_class", "\"" + std::string(dynamic_cast<const ClothSphere*>(shape) ? "ClothShapeSphere" :
                                                                       dynamic_cast<const ClothCapsule*>(shape) ? "ClothShapeCapsule" :
                                                                       dynamic_cast<const ClothBox*>(shape) ? "ClothShapeBox" : "ClothShapeSDF") + "\"");
    addField(fieldIndent, block, "name", "\"" + shape->name + "\"");
    addField(fieldIndent, block, "parent_bone", "\"" + shape->parentBone + "\"");
    addField(fieldIndent, block, "cloth_collision_layer0", layer0 ? "true" : "false");
    addField(fieldIndent, block, "cloth_collision_layer1", layer1 ? "true" : "false");
    addField(fieldIndent, block, "cloth_collision_layer2", layer2 ? "true" : "false");
    addField(fieldIndent, block, "cloth_collision_layer3", layer3 ? "true" : "false");
    addField(fieldIndent, block, "cloth_collision_priority", std::to_string(shape->collisionPriority));
    addField(fieldIndent, block, "vertex_map", "\"" + shape->vertexMap + "\"");
    addField(fieldIndent, block, "inverted_collision", shape->invertedCollision ? "true" : "false");
    addField(fieldIndent, block, "planarize", shape->planarize ? "true" : "false");
    addField(fieldIndent, block, "bounciness", FormatKVFloat(shape->bounciness));

    if (const ClothSphere* s = dynamic_cast<const ClothSphere*>(shape)) {
        addField(fieldIndent, block, "radius", FormatKVFloat(s->radius));
        addField(fieldIndent, block, "center", FormatVec3(s->center));
    } else if (const ClothCapsule* c = dynamic_cast<const ClothCapsule*>(shape)) {
        addField(fieldIndent, block, "radius0", FormatKVFloat(c->radius0));
        addField(fieldIndent, block, "radius1", FormatKVFloat(c->radius1));
        addField(fieldIndent, block, "point0", FormatVec3(c->point0));
        addField(fieldIndent, block, "point1", FormatVec3(c->point1));
    } else if (const ClothBox* b = dynamic_cast<const ClothBox*>(shape)) {
        addField(fieldIndent, block, "min", FormatVec3(b->min));
        addField(fieldIndent, block, "max", FormatVec3(b->max));
    } else if (const ClothSDF* d = dynamic_cast<const ClothSDF*>(shape)) {
        addField(fieldIndent, block, "filename", "\"" + d->filename + "\"");
    }

    block += entryIndent + "}";
    return block;
}

std::string BuildSoftbodyBlock(const ParsedClothData& data,
                               const std::vector<std::string>& prefixBlocks,
                               const std::string& clothParamsBlock) {
    std::string outer = "\t\t";
    std::string inner = "\t\t\t";
    std::string entry = "\t\t\t\t";
    std::string shape = "\t\t\t\t\t";

    std::string block = outer + "{\n";
    block += inner + "_class = \"Softbody\"\n";
    block += inner + "children = \n";
    block += inner + "[\n";

    bool first = true;
    for (const std::string& pb : prefixBlocks) {
        if (!first) block += ",\n";
        block += pb;
        first = false;
    }
    if (!clothParamsBlock.empty()) {
        if (!first) block += ",\n";
        block += clothParamsBlock;
        first = false;
    }
    for (size_t i = 0; i < data.shapes.size(); ++i) {
        if (!first) block += ",\n";
        block += BuildShapeBlock(data.shapes[i].get(), shape, entry);
        first = false;
    }
    if (!first) block += "\n";
    block += inner + "]\n";
    block += inner + "stiffness_on_ragdoll = 0.0\n";
    block += inner + "motion_smooth_cdt = 0.0\n";
    block += inner + "cloth_sleep_enabled = false\n";
    block += inner + "cloth_immovable_hint = false\n";
    block += inner + "cloth_per_bone_scale_enabled = false\n";
    block += inner + "cloth_enable_empty_model = false\n";
    block += inner + "cloth_keychain_motion = false\n";
    block += outer + "}";
    return block;
}

bool ReadFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool WriteFile(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << data;
    return f.good();
}

bool FindRootChildrenArray(const std::string& vmdl, size_t& outStart, size_t& outEnd) {
    size_t rootPos = vmdl.find("_class = \"RootNode\"");
    if (rootPos == std::string::npos) return false;
    size_t childrenPos = vmdl.find("children =", rootPos);
    if (childrenPos == std::string::npos) return false;
    size_t arrOpen = vmdl.find('[', childrenPos);
    if (arrOpen == std::string::npos) return false;
    return FindBalancedBlock(vmdl, arrOpen, '[', ']', outStart, outEnd);
}

// ---------- stage 2: cloth proxy mesh reconstruction ----------

struct FeNode {
    std::string name;
    int mesh = -1;
    int point = -1;
    bool cloth = false;
    bool isStatic = false;
    float radius = 0.0f;
    float friction = 0.0f;
    float forceAttr = 0.0f;
    float vertAttr = 0.0f;
    std::array<float, 3> pos{};
    int ctrlParent = -1;
};

struct FeData {
    std::vector<FeNode> nodes;
    int nStatic = 0;
    int firstDriven = 0;
    bool hasFriction = false;
    std::unordered_map<int, std::vector<std::pair<int, float>>> fitWeights;
    std::set<std::pair<int, int>> structuralEdges;
    double expAirDrag = 0.0, velAirDrag = 0.0, gravityScale = 1.0, internalPressure = 0.0;
    double windage = 0.0, windDrag = 0.0, surfaceStretch = 0.0;
    double rodSmoothRate = 0.0;
    int64_t rodSmoothIters = 0;
    double localForce = 1.0, localRotation = 0.0, localDrag1 = 0.0;
    double addWorldCollisionRadius = 2.0;
    int64_t extraIter = 0, extraGoalIter = 0, extraPressureIter = 0;
    bool valid = false;
};

struct SkeletonBone {
    std::string name;
    int parent = -1;
    std::array<float, 3> origin{};
    std::array<float, 3> angles{};
};

bool ParseClothName(const std::string& s, int& outMesh, int& outPoint) {
    if (s.rfind("$cloth_m", 0) != 0) return false;
    const char* p = s.c_str() + 8;
    char* end = nullptr;
    long m = std::strtol(p, &end, 10);
    if (!end || *end != 'p') return false;
    long pt = std::strtol(end + 1, nullptr, 10);
    outMesh = (int)m;
    outPoint = (int)pt;
    return true;
}

bool ParseFeModel(const std::string& physBlockText, FeData& out, std::string& outLog) {
    kv3t::Value root;
    if (!kv3t::Parse(physBlockText, root)) {
        outLog = "Failed to parse PHYS KV3 text.";
        return false;
    }
    const kv3t::Value* fe = root.find("m_pFeModel");
    if (!fe) {
        outLog = "No m_pFeModel in PHYS block.";
        return false;
    }
    const kv3t::Value* cn = fe->find("m_CtrlName");
    if (!cn) {
        outLog = "No m_CtrlName in m_pFeModel.";
        return false;
    }
    std::vector<std::string> names = cn->asStringArray();
    out.nodes.resize(names.size());
    for (size_t i = 0; i < names.size(); ++i) out.nodes[i].name = names[i];

    out.nStatic = (int)(fe->find("m_nStaticNodes") ? fe->find("m_nStaticNodes")->asInt() : 0);
    out.firstDriven = (int)(fe->find("m_nFirstPositionDrivenNode") ? fe->find("m_nFirstPositionDrivenNode")->asInt() : (int64_t)names.size());

    std::vector<double> invmass = fe->find("m_NodeInvMasses") ? fe->find("m_NodeInvMasses")->asDoubleArray() : std::vector<double>{};
    std::vector<double> radii = fe->find("m_NodeCollisionRadii") ? fe->find("m_NodeCollisionRadii")->asDoubleArray() : std::vector<double>{};
    const kv3t::Value* fricV = fe->find("m_DynNodeFriction");
    std::vector<double> friction = fricV ? fricV->asDoubleArray() : std::vector<double>{};
    out.hasFriction = !friction.empty();

    for (size_t i = 0; i < out.nodes.size(); ++i) {
        FeNode& n = out.nodes[i];
        n.cloth = ParseClothName(n.name, n.mesh, n.point);
        n.isStatic = (int)i < out.nStatic;
        int dyn = (int)i - out.nStatic;
        if (dyn >= 0 && dyn < (int)radii.size()) n.radius = (float)radii[dyn];
        if (dyn >= 0 && dyn < (int)friction.size()) n.friction = (float)friction[dyn];
        if (i < (int)invmass.size()) {
            // invmass not stored in FeNode; static flag from node class partition
        }
    }

    if (const kv3t::Value* ip = fe->find("m_InitPose")) {
        for (size_t i = 0; i < ip->arr.size() && i < out.nodes.size(); ++i) {
            std::vector<double> v = ip->arr[i].asDoubleArray();
            if (v.size() >= 3) out.nodes[i].pos = {(float)v[0], (float)v[1], (float)v[2]};
        }
    }
    if (const kv3t::Value* ni = fe->find("m_NodeIntegrator")) {
        for (size_t i = 0; i < ni->arr.size() && i < out.nodes.size(); ++i) {
            if (const kv3t::Value* a = ni->arr[i].find("flAnimationForceAttraction")) out.nodes[i].forceAttr = (float)a->asDouble();
            if (const kv3t::Value* a = ni->arr[i].find("flAnimationVertexAttraction")) out.nodes[i].vertAttr = (float)a->asDouble();
        }
    }
    if (const kv3t::Value* co = fe->find("m_CtrlOffsets")) {
        for (const auto& e : co->arr) {
            int child = (int)(e.find("nCtrlChild") ? e.find("nCtrlChild")->asInt() : -1);
            int parent = (int)(e.find("nCtrlParent") ? e.find("nCtrlParent")->asInt() : -1);
            if (child >= 0 && child < (int)out.nodes.size()) out.nodes[child].ctrlParent = parent;
        }
    }

    std::vector<int> fitBoneOfRange;
    if (const kv3t::Value* fm = fe->find("m_FitMatrices")) {
        for (const auto& e : fm->arr) fitBoneOfRange.push_back((int)(e.find("nNode") ? e.find("nNode")->asInt() : -1));
    }
    if (const kv3t::Value* fw = fe->find("m_FitWeights")) {
        size_t idx = 0;
        if (const kv3t::Value* fm = fe->find("m_FitMatrices")) {
            for (const auto& e : fm->arr) {
                int boneNode = (int)(e.find("nNode") ? e.find("nNode")->asInt() : -1);
                int begin = (int)(e.find("nBeginDynamic") ? e.find("nBeginDynamic")->asInt() : 0);
                int end = (int)(e.find("nEnd") ? e.find("nEnd")->asInt() : 0);
                for (int k = begin; k < end && k < (int)fw->arr.size(); ++k) {
                    int node = (int)(fw->arr[k].find("nNode") ? fw->arr[k].find("nNode")->asInt() : -1);
                    float w = (float)(fw->arr[k].find("flWeight") ? fw->arr[k].find("flWeight")->asDouble() : 0.0);
                    if (node >= 0 && boneNode >= 0 && w > 0.0f) out.fitWeights[node].push_back({boneNode, w});
                }
                (void)idx;
            }
        }
    }

    if (const kv3t::Value* rd = fe->find("m_Rods")) {
        for (const auto& e : rd->arr) {
            const kv3t::Value* nn = e.find("nNode");
            const kv3t::Value* mn = e.find("flMinDist");
            const kv3t::Value* mx = e.find("flMaxDist");
            if (!nn || !mn || !mx) continue;
            std::vector<int64_t> idx = nn->asIntArray();
            if (idx.size() < 2) continue;
            double minD = mn->asDouble(), maxD = mx->asDouble();
            if (maxD <= 0.0) continue;
            if (std::fabs(minD / maxD - 0.75) > 0.02) continue;
            int a = (int)idx[0], b = (int)idx[1];
            if (a < 0 || b < 0 || a >= (int)out.nodes.size() || b >= (int)out.nodes.size()) continue;
            if (!out.nodes[a].cloth || !out.nodes[b].cloth) continue;
            if (out.nodes[a].mesh != out.nodes[b].mesh) continue;
            if (a == b) continue;
            out.structuralEdges.insert({std::min(a, b), std::max(a, b)});
        }
    }

    auto getF = [&](const char* k, double def) { return fe->find(k) ? fe->find(k)->asDouble() : def; };
    auto getI = [&](const char* k, int64_t def) { return fe->find(k) ? fe->find(k)->asInt() : def; };
    out.expAirDrag = getF("m_flDefaultExpAirDrag", 0.0);
    out.velAirDrag = getF("m_flDefaultVelAirDrag", 0.0);
    out.gravityScale = getF("m_flDefaultGravityScale", 1.0);
    out.internalPressure = getF("m_flInternalPressure", 0.0);
    out.windage = getF("m_flWindage", 0.0);
    out.windDrag = getF("m_flWindDrag", 0.0);
    out.surfaceStretch = getF("m_flDefaultSurfaceStretch", 0.0);
    out.rodSmoothRate = getF("m_flRodVelocitySmoothRate", 0.0);
    out.rodSmoothIters = getI("m_nRodVelocitySmoothIterations", 0);
    out.localForce = getF("m_flLocalForce", 1.0);
    out.localRotation = getF("m_flLocalRotation", 0.0);
    out.localDrag1 = getF("m_flLocalDrag1", 0.0);
    out.addWorldCollisionRadius = getF("m_flAddWorldCollisionRadius", 2.0);
    out.extraIter = getI("m_nExtraIterations", 0);
    out.extraGoalIter = getI("m_nExtraGoalIterations", 0);
    out.extraPressureIter = getI("m_nExtraPressureIterations", 0);

    out.valid = true;
    return true;
}

bool ParseVmdlSkeleton(const std::string& vmdlText, std::vector<SkeletonBone>& out, std::string& outLog) {
    kv3t::Value root;
    if (!kv3t::Parse(vmdlText, root)) {
        outLog = "Failed to parse .vmdl KV3 text.";
        return false;
    }
    const kv3t::Value* rn = root.find("rootNode");
    if (!rn) { outLog = "No rootNode in .vmdl."; return false; }
    const kv3t::Value* children = rn->find("children");
    if (!children) { outLog = "No rootNode children in .vmdl."; return false; }

    std::unordered_map<std::string, int> byName;
    std::function<void(const kv3t::Value&, int)> walk = [&](const kv3t::Value& node, int parent) {
        const kv3t::Value* nm = node.find("name");
        if (!nm) return;
        SkeletonBone b;
        b.name = nm->scalar;
        b.parent = parent;
        if (const kv3t::Value* o = node.find("origin")) {
            std::vector<double> v = o->asDoubleArray();
            if (v.size() >= 3) b.origin = {(float)v[0], (float)v[1], (float)v[2]};
        }
        if (const kv3t::Value* a = node.find("angles")) {
            std::vector<double> v = a->asDoubleArray();
            for (int i = 0; i < 3 && i < (int)v.size(); ++i) b.angles[i] = (float)v[i];
        }
        int idx = (int)out.size();
        if (byName.count(b.name)) return; // duplicate bone name in skeleton; keep first
        byName[b.name] = idx;
        out.push_back(b);
        if (const kv3t::Value* ch = node.find("children"))
            for (const auto& c : ch->arr) walk(c, idx);
    };
    for (const auto& c : children->arr) {
        const kv3t::Value* cl = c.find("_class");
        if (cl && cl->scalar == "Skeleton") {
            if (const kv3t::Value* ch = c.find("children"))
                for (const auto& b : ch->arr) walk(b, -1);
        }
    }
    if (out.empty()) { outLog = "No Skeleton bones found in .vmdl."; return false; }
    return true;
}

void QuatMul(const float a[4], const float b[4], float o[4]) {
    o[0] = a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1];
    o[1] = a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0];
    o[2] = a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3];
    o[3] = a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2];
}

void QuatFromSourceAngles(float pitch, float yaw, float roll, float o[4]) {
    auto aa = [](float deg, int axis, float q[4]) {
        float h = deg * 3.14159265358979f / 360.0f;
        float s = sinf(h), c = cosf(h);
        q[0] = q[1] = q[2] = 0.0f; q[3] = c; q[axis] = s;
    };
    float qx[4], qy[4], qz[4], t[4];
    aa(roll, 0, qx); aa(pitch, 1, qy); aa(yaw, 2, qz);
    QuatMul(qz, qy, t);
    QuatMul(t, qx, o);
}

void QuatRotateVec(const float q[4], const float v[3], float o[3]) {
    float ux = q[0], uy = q[1], uz = q[2], s = q[3];
    float uvx = uy * v[2] - uz * v[1];
    float uvy = uz * v[0] - ux * v[2];
    float uvz = ux * v[1] - uy * v[0];
    float uuvx = uy * uvz - uz * uvy;
    float uuvy = uz * uvx - ux * uvz;
    float uuvz = ux * uvy - uy * uvx;
    o[0] = v[0] + 2.0f * (s * uvx + uuvx);
    o[1] = v[1] + 2.0f * (s * uvy + uuvy);
    o[2] = v[2] + 2.0f * (s * uvz + uuvz);
}

std::vector<std::array<float, 3>> ComputeBoneWorldPositions(const std::vector<SkeletonBone>& bones) {
    std::vector<std::array<float, 3>> wpos(bones.size());
    std::vector<std::array<float, 4>> wquat(bones.size());
    std::vector<char> done(bones.size(), 0);
    std::function<void(int)> solve = [&](int i) {
        if (done[i]) return;
        const SkeletonBone& b = bones[i];
        float lq[4];
        QuatFromSourceAngles(b.angles[0], b.angles[1], b.angles[2], lq);
        if (b.parent < 0) {
            wpos[i] = b.origin;
            std::copy(lq, lq + 4, wquat[i].begin());
            done[i] = 1;
            return;
        }
        solve(b.parent);
        QuatMul(wquat[b.parent].data(), lq, wquat[i].data());
        float r[3];
        QuatRotateVec(wquat[b.parent].data(), b.origin.data(), r);
        wpos[i] = {r[0] + wpos[b.parent][0], r[1] + wpos[b.parent][1], r[2] + wpos[b.parent][2]};
        done[i] = 1;
    };
    for (size_t i = 0; i < bones.size(); ++i) solve((int)i);
    return wpos;
}

int ResolveBoneNode(const FeData& fe, int node) {
    int cur = node;
    std::set<int> seen;
    while (cur >= 0 && cur < (int)fe.nodes.size() && !seen.count(cur)) {
        seen.insert(cur);
        if (!fe.nodes[cur].cloth) return cur;
        cur = fe.nodes[cur].ctrlParent;
    }
    return -1;
}

struct ProxyFace { std::vector<int> corners; }; // cloth node indices

std::array<float, 3> FaceNormalOf(const ProxyFace& f, const FeData& fe) {
    // Newell's method
    std::array<float, 3> n{0, 0, 0};
    for (size_t i = 0; i < f.corners.size(); ++i) {
        const auto& a = fe.nodes[f.corners[i]].pos;
        const auto& b = fe.nodes[f.corners[(i + 1) % f.corners.size()]].pos;
        n[0] += (a[1] - b[1]) * (a[2] + b[2]);
        n[1] += (a[2] - b[2]) * (a[0] + b[0]);
        n[2] += (a[0] - b[0]) * (a[1] + b[1]);
    }
    float l = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
    if (l < 1e-12f) return {0, 0, 1};
    return {n[0] / l, n[1] / l, n[2] / l};
}

std::vector<ProxyFace> RecoverFaces(const std::set<std::pair<int,int>>& edges,
                                    const FeData& fe,
                                    const std::array<float, 3>& outwardRef,
                                    int& outUncovered) {
    std::vector<ProxyFace> faces;
    outUncovered = 0;
    if (edges.empty()) return faces;

    // Virtual static-static edges: the compiler emits no rods between two static
    // particles, so cells between static verts always miss those edges. Restore them
    // by completing K4s that miss exactly one (static-static) side.
    std::set<std::pair<int,int>> allEdges = edges;
    std::set<std::pair<int,int>> virtualEdges;
    {
        std::map<int, std::set<int>> adj;
        for (auto& e : edges) { adj[e.first].insert(e.second); adj[e.second].insert(e.first); }
        std::vector<int> statics;
        for (auto& e : edges) {
            for (int n : {e.first, e.second}) {
                if (fe.nodes[n].cloth && fe.nodes[n].isStatic &&
                    std::find(statics.begin(), statics.end(), n) == statics.end())
                    statics.push_back(n);
            }
        }
        for (size_t i = 0; i < statics.size(); ++i) {
            for (size_t j = i + 1; j < statics.size(); ++j) {
                int s1 = statics[i], s2 = statics[j];
                std::pair<int,int> p{std::min(s1, s2), std::max(s1, s2)};
                if (allEdges.count(p)) continue;
                bool completed = false;
                for (int c : adj[s1]) {
                    if (c == s2 || !adj[s2].count(c)) continue;
                    for (int d : adj[s1]) {
                        if (d == s2 || d == c || !adj[s2].count(d)) continue;
                        if (allEdges.count({std::min(c, d), std::max(c, d)})) { completed = true; break; }
                    }
                    if (completed) break;
                }
                if (completed) {
                    virtualEdges.insert(p);
                    allEdges.insert(p);
                    adj[s1].insert(s2);
                    adj[s2].insert(s1);
                }
            }
        }
    }

    std::map<int, std::set<int>> adj;
    std::set<std::pair<int,int>> covered;
    for (auto& e : allEdges) { adj[e.first].insert(e.second); adj[e.second].insert(e.first); }

    auto pos = [&](int n) { return fe.nodes[n].pos; };

    // 4-cliques -> quads
    std::vector<std::array<int,4>> k4s;
    for (auto& e : allEdges) {
        int a = e.first, b = e.second;
        for (int c : adj[a]) {
            if (c == b) continue;
            if (!adj[b].count(c)) continue;
            for (int d : adj[a]) {
                if (d == b || d == c) continue;
                if (!adj[b].count(d)) continue;
                if (!adj[c].count(d)) continue;
                std::array<int,4> q = {a, b, c, d};
                std::sort(q.begin(), q.end());
                bool dup = false;
                for (auto& x : k4s) if (x == q) { dup = true; break; }
                if (!dup) k4s.push_back(q);
            }
        }
    }
    // score quads by newly covered edges, greedy; prefer regular (low-aspect) cells
    auto quadEdges = [](const std::array<int,4>& q) {
        std::vector<std::pair<int,int>> es;
        for (int i = 0; i < 4; ++i) for (int j = i + 1; j < 4; ++j)
            es.push_back({std::min(q[i], q[j]), std::max(q[i], q[j])});
        return es;
    };
    auto edgeLen = [&](const std::pair<int,int>& e) {
        const auto& a = pos(e.first); const auto& b = pos(e.second);
        float dx = a[0]-b[0], dy = a[1]-b[1], dz = a[2]-b[2];
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    };
    std::vector<float> k4aspect(k4s.size(), 1.0f);
    std::vector<int> k4order(k4s.size(), 0);
    for (size_t i = 0; i < k4s.size(); ++i) {
        float mn = 1e30f, mx = 0.0f;
        for (auto& e : quadEdges(k4s[i])) {
            float l = edgeLen(e);
            mn = std::min(mn, l); mx = std::max(mx, l);
        }
        k4aspect[i] = mn > 1e-6f ? mx / mn : 1e30f;
        k4order[i] = (int)i;
    }
    std::sort(k4order.begin(), k4order.end(), [&](int a, int b) { return k4aspect[a] < k4aspect[b]; });
    std::vector<std::set<std::pair<int,int>>> emittedEdgeSets;
    std::vector<char> used(k4s.size(), 0);
    for (;;) {
        int best = -1, bestScore = 0;
        for (int oi = 0; oi < (int)k4order.size(); ++oi) {
            int i = k4order[oi];
            if (used[i]) continue;
            int score = 0;
            for (auto& e : quadEdges(k4s[i])) if (!covered.count(e)) ++score;
            if (score > bestScore) { bestScore = score; best = i; }
        }
        if (bestScore == 0) break;
        used[best] = 1;
        // reject overlaps: a candidate sharing 3+ edges with a single emitted face is a bogus twin cell
        bool overlaps = false;
        {
            auto es = quadEdges(k4s[best]);
            for (const auto& emitted : emittedEdgeSets) {
                int shared = 0;
                for (auto& e : es) if (emitted.count(e)) ++shared;
                if (shared >= 3) { overlaps = true; break; }
            }
        }
        if (overlaps) continue;
        std::set<std::pair<int,int>> thisEdges;
        for (auto& e : quadEdges(k4s[best])) { covered.insert(e); thisEdges.insert(e); }
        emittedEdgeSets.push_back(std::move(thisEdges));
        // order corners cyclically around centroid
        std::array<int,4> q = k4s[best];
        std::array<float,3> c{};
        for (int n : q) { c[0] += pos(n)[0]; c[1] += pos(n)[1]; c[2] += pos(n)[2]; }
        c[0] /= 4; c[1] /= 4; c[2] /= 4;
        std::array<float,3> e0{pos(q[1])[0]-pos(q[0])[0], pos(q[1])[1]-pos(q[0])[1], pos(q[1])[2]-pos(q[0])[2]};
        std::array<float,3> e1{pos(q[2])[0]-pos(q[0])[0], pos(q[2])[1]-pos(q[0])[1], pos(q[2])[2]-pos(q[0])[2]};
        std::array<float,3> nrm{e0[1]*e1[2]-e0[2]*e1[1], e0[2]*e1[0]-e0[0]*e1[2], e0[0]*e1[1]-e0[1]*e1[0]};
        float nl = std::sqrt(nrm[0]*nrm[0]+nrm[1]*nrm[1]+nrm[2]*nrm[2]);
        if (nl < 1e-8f) nrm = {0,0,1}; else { nrm[0]/=nl; nrm[1]/=nl; nrm[2]/=nl; }
        std::array<float,3> u{pos(q[0])[0]-c[0], pos(q[0])[1]-c[1], pos(q[0])[2]-c[2]};
        float ul = std::sqrt(u[0]*u[0]+u[1]*u[1]+u[2]*u[2]); if (ul > 1e-8f) { u[0]/=ul; u[1]/=ul; u[2]/=ul; }
        std::array<float,3> v{nrm[1]*u[2]-nrm[2]*u[1], nrm[2]*u[0]-nrm[0]*u[2], nrm[0]*u[1]-nrm[1]*u[0]};
        std::array<std::pair<float,int>,4> order;
        for (int i = 0; i < 4; ++i) {
            std::array<float,3> d{pos(q[i])[0]-c[0], pos(q[i])[1]-c[1], pos(q[i])[2]-c[2]};
            float ang = std::atan2(d[0]*v[0]+d[1]*v[1]+d[2]*v[2], d[0]*u[0]+d[1]*u[1]+d[2]*u[2]);
            order[i] = {ang, q[i]};
        }
        std::sort(order.begin(), order.end());
        ProxyFace f;
        for (auto& o : order) f.corners.push_back(o.second);
        faces.push_back(std::move(f));
    }

    // 3-cliques -> triangles for remaining edges
    for (;;) {
        bool progress = false;
        for (auto& e : allEdges) {
            if (covered.count(e)) continue;
            int a = e.first, b = e.second;
            int bestC = -1, bestNew = -1;
            for (int c : adj[a]) {
                if (c == b || !adj[b].count(c)) continue;
                std::pair<int,int> e1{std::min(a,c), std::max(a,c)};
                std::pair<int,int> e2{std::min(b,c), std::max(b,c)};
                int nw = (covered.count(e1) ? 0 : 1) + (covered.count(e2) ? 0 : 1);
                if (nw > bestNew) { bestNew = nw; bestC = c; }
            }
            if (bestC < 0) break;
            covered.insert({std::min(a,bestC), std::max(a,bestC)});
            covered.insert({std::min(b,bestC), std::max(b,bestC)});
            covered.insert(e);
            ProxyFace f;
            f.corners = {a, b, bestC};
            faces.push_back(std::move(f));
            progress = true;
        }
        if (!progress) break;
    }
    for (auto& e : edges) if (!covered.count(e)) ++outUncovered;

    // ---- winding consistency ----
    if (!faces.empty()) {
        // adjacency via shared directed edges
        std::map<std::pair<int,int>, std::vector<std::pair<int,int>>> edgeUse; // (min,max) -> [(face, cornerA)]
        for (int fi = 0; fi < (int)faces.size(); ++fi) {
            const ProxyFace& f = faces[fi];
            int k = (int)f.corners.size();
            for (int i = 0; i < k; ++i) {
                int a = f.corners[i], b = f.corners[(i + 1) % k];
                edgeUse[{std::min(a, b), std::max(a, b)}].push_back({fi, a});
            }
        }
        std::vector<char> flipped(faces.size(), 0), visited(faces.size(), 0);
        std::vector<int> comp(faces.size(), -1);
        int nComp = 0;
        // BFS per connected component
        for (int seed = 0; seed < (int)faces.size(); ++seed) {
            if (visited[seed]) continue;
            std::vector<int> stack = {seed};
            visited[seed] = 1;
            comp[seed] = nComp;
            while (!stack.empty()) {
                int fi = stack.back(); stack.pop_back();
                const ProxyFace& f = faces[fi];
                int k = (int)f.corners.size();
                for (int i = 0; i < k; ++i) {
                    int a = f.corners[i], b = f.corners[(i + 1) % k];
                    int effA = flipped[fi] ? b : a;
                    for (auto& use : edgeUse[{std::min(a, b), std::max(a, b)}]) {
                        int fj = use.first;
                        if (fj == fi || visited[fj]) continue;
                        // neighbor's effective traversal must be opposite to ours:
                        // flip it iff its raw traversal equals our effective one
                        flipped[fj] = (use.second == effA);
                        visited[fj] = 1;
                        comp[fj] = nComp;
                        stack.push_back(fj);
                    }
                }
            }
            ++nComp;
        }
        for (int fi = 0; fi < (int)faces.size(); ++fi)
            if (flipped[fi]) std::reverse(faces[fi].corners.begin(), faces[fi].corners.end());

        // orientation per component: area-weighted normals should point away from the body
        for (int c = 0; c < nComp; ++c) {
            double score = 0.0;
            for (int fi = 0; fi < (int)faces.size(); ++fi) {
                if (comp[fi] != c) continue;
                const ProxyFace& f = faces[fi];
                std::array<float,3> ctr{};
                for (int n : f.corners) { ctr[0] += pos(n)[0]; ctr[1] += pos(n)[1]; ctr[2] += pos(n)[2]; }
                size_t cnt = f.corners.size();
                ctr[0] /= cnt; ctr[1] /= cnt; ctr[2] /= cnt;
                // raw Newell normal (magnitude = area) as an area-weighted vote
                std::array<float, 3> n{0, 0, 0};
                for (size_t i = 0; i < f.corners.size(); ++i) {
                    const auto& a = fe.nodes[f.corners[i]].pos;
                    const auto& b = fe.nodes[f.corners[(i + 1) % f.corners.size()]].pos;
                    n[0] += (a[1] - b[1]) * (a[2] + b[2]);
                    n[1] += (a[2] - b[2]) * (a[0] + b[0]);
                    n[2] += (a[0] - b[0]) * (a[1] + b[1]);
                }
                std::array<float,3> out{ctr[0] - outwardRef[0], ctr[1] - outwardRef[1], ctr[2] - outwardRef[2]};
                score += n[0] * out[0] + n[1] * out[1] + n[2] * out[2];
            }
            if (score < 0.0)
                for (int fi = 0; fi < (int)faces.size(); ++fi)
                    if (comp[fi] == c) std::reverse(faces[fi].corners.begin(), faces[fi].corners.end());
        }
    }
    return faces;
}

struct BuiltProxy {
    int meshIndex = 0;
    std::string proxyName;
    std::string dmxGamePath;
    std::string dmxDiskPath;
    int vertCount = 0;
    int faceCount = 0;
};

bool BuildProxyDmx(const FeData& fe, int meshIndex, const std::vector<int>& meshClothNodes,
                   const std::vector<SkeletonBone>& bones,
                   const std::unordered_map<int, std::vector<std::pair<int, float>>>& staticPinWeights,
                   const std::vector<std::string>& rigidParentBoneNames,
                   const std::string& dmxDiskPath,
                   int& outVertCount, int& outFaceCount, std::string& outLog) {
    std::unordered_map<std::string, int> boneIdx;
    for (size_t i = 0; i < bones.size(); ++i) boneIdx[bones[i].name] = (int)i;

    dmxbin::Doc doc;
    std::vector<dmxbin::Element> elems;
    elems.reserve(16 + bones.size() * 2);
    uint32_t guidCounter = 0;
    auto addElem = [&](const char* type, const char* name) -> uint32_t {
        dmxbin::Element e;
        e.type = doc.AddString(type);
        e.name = doc.AddString(name);
        uint32_t g = ++guidCounter;
        for (int i = 0; i < 16; ++i) e.guid[i] = (uint8_t)((g >> ((i & 3) * 8)) ^ (i * 37));
        elems.push_back(std::move(e));
        return (uint32_t)elems.size() - 1;
    };

    uint32_t scene = addElem("DmElement", "Scene");
    uint32_t model = addElem("DmeModel", "cloth_proxy");
    std::vector<uint32_t> jointElems, jointXforms;
    std::vector<int> jointBone;
    {
        std::function<void(int)> dfs = [&](int bi) {
            uint32_t j = addElem("DmeJoint", bones[bi].name.c_str());
            uint32_t t = addElem("DmeTransform", bones[bi].name.c_str());
            jointElems.push_back(j);
            jointXforms.push_back(t);
            jointBone.push_back(bi);
            for (size_t k = 0; k < bones.size(); ++k) if (bones[k].parent == bi) dfs((int)k);
        };
        for (size_t i = 0; i < bones.size(); ++i) if (bones[i].parent == -1) dfs((int)i);
    }
    std::unordered_map<int,int> boneToJoint;
    for (size_t i = 0; i < jointBone.size(); ++i) boneToJoint[jointBone[i]] = (int)i;

    uint32_t dag = addElem("DmeDag", "cloth_proxy");
    uint32_t mesh = addElem("DmeMesh", "cloth_proxy");
    uint32_t vdata = addElem("DmeVertexData", "bind");
    uint32_t fset = addElem("DmeFaceSet", "no_material");
    uint32_t mat = addElem("DmeMaterial", "no_material");
    uint32_t dagXf = addElem("DmeTransform", "cloth_proxy");
    uint32_t tlist = addElem("DmeTransformList", "base");
    uint32_t axis = addElem("DmeAxisSystem", "axisSystem");
    uint32_t modelXf = addElem("DmeTransform", "");

    dmxbin::SetAttr(elems[scene], dmxbin::MakeElement(doc.AddString("skeleton"), model));
    dmxbin::SetAttr(elems[scene], dmxbin::MakeElement(doc.AddString("model"), model));

    std::vector<uint32_t> modelChildren;
    for (size_t i = 0; i < bones.size(); ++i)
        if (bones[i].parent == -1) {
            auto it = boneToJoint.find((int)i);
            if (it != boneToJoint.end()) modelChildren.push_back(jointElems[it->second]);
        }
    modelChildren.push_back(dag);
    dmxbin::SetAttr(elems[model], dmxbin::MakeElementArray(doc.AddString("children"), modelChildren));
    dmxbin::SetAttr(elems[model], dmxbin::MakeElementArray(doc.AddString("baseStates"), {tlist}));
    dmxbin::SetAttr(elems[model], dmxbin::MakeElement(doc.AddString("axisSystem"), axis));
    dmxbin::SetAttr(elems[model], dmxbin::MakeElement(doc.AddString("transform"), modelXf));
    dmxbin::SetAttr(elems[model], dmxbin::MakeElementArray(doc.AddString("jointList"), jointElems));

    for (size_t i = 0; i < jointElems.size(); ++i) {
        const SkeletonBone& b = bones[jointBone[i]];
        dmxbin::SetAttr(elems[jointElems[i]], dmxbin::MakeElement(doc.AddString("transform"), jointXforms[i]));
        std::vector<uint32_t> ch;
        for (size_t k = 0; k < bones.size(); ++k)
            if (bones[k].parent == jointBone[i]) {
                auto it = boneToJoint.find((int)k);
                if (it != boneToJoint.end()) ch.push_back(jointElems[it->second]);
            }
        dmxbin::SetAttr(elems[jointElems[i]], dmxbin::MakeElementArray(doc.AddString("children"), ch));

        dmxbin::Attr pa; pa.name = doc.AddString("position"); pa.type = dmxbin::AT_VECTOR3;
        pa.raw.resize(12); memcpy(pa.raw.data(), b.origin.data(), 12);
        dmxbin::SetAttr(elems[jointXforms[i]], std::move(pa));
        float q[4];
        QuatFromSourceAngles(b.angles[0], b.angles[1], b.angles[2], q);
        dmxbin::Attr oa; oa.name = doc.AddString("orientation"); oa.type = dmxbin::AT_QUATERNION;
        oa.raw.resize(16); memcpy(oa.raw.data(), q, 16);
        dmxbin::SetAttr(elems[jointXforms[i]], std::move(oa));
    }

    dmxbin::SetAttr(elems[dag], dmxbin::MakeElement(doc.AddString("shape"), mesh));
    dmxbin::SetAttr(elems[dag], dmxbin::MakeElement(doc.AddString("transform"), dagXf));
    auto writeIdentXf = [&](uint32_t xf) {
        dmxbin::Attr pa; pa.name = doc.AddString("position"); pa.type = dmxbin::AT_VECTOR3;
        pa.raw.resize(12); memset(pa.raw.data(), 0, 12);
        dmxbin::SetAttr(elems[xf], std::move(pa));
        dmxbin::Attr oa; oa.name = doc.AddString("orientation"); oa.type = dmxbin::AT_QUATERNION;
        oa.raw.resize(16); float q[4] = {0,0,0,1}; memcpy(oa.raw.data(), q, 16);
        dmxbin::SetAttr(elems[xf], std::move(oa));
    };
    writeIdentXf(dagXf);
    writeIdentXf(modelXf);

    dmxbin::SetAttr(elems[mesh], dmxbin::MakeBool(doc.AddString("visible"), true));
    dmxbin::SetAttr(elems[mesh], dmxbin::MakeElement(doc.AddString("bindState"), vdata));
    dmxbin::SetAttr(elems[mesh], dmxbin::MakeElement(doc.AddString("currentState"), vdata));
    dmxbin::SetAttr(elems[mesh], dmxbin::MakeElementArray(doc.AddString("baseStates"), {vdata}));
    dmxbin::SetAttr(elems[mesh], dmxbin::MakeElementArray(doc.AddString("faceSets"), {fset}));

    // vertices
    int nv = (int)meshClothNodes.size();
    std::vector<std::array<float,3>> pos(nv);
    std::vector<float> enable(nv), goal(nv), drag(nv), radius(nv), friction(nv);
    for (int v = 0; v < nv; ++v) {
        const FeNode& n = fe.nodes[meshClothNodes[v]];
        pos[v] = n.pos;
        enable[v] = n.isStatic ? 0.0f : 1.0f;
        double g3 = n.forceAttr;
        if (g3 < 0.0) g3 = 0.0;
        if (g3 > 1.0) g3 = 1.0;
        double g = std::cbrt(g3);
        if (g > 0.999) g = 1.0;
        goal[v] = (float)g;
        double f;
        if (g3 >= 0.9999) f = n.vertAttr;
        else f = ((double)n.vertAttr - g3) / (1.0 - g3);
        if (f < 0.0) f = 0.0;
        if (f > 1.0) f = 1.0;
        drag[v] = (float)(1.0 - std::pow(1.0 - f, 60.0));
        radius[v] = n.isStatic ? 0.0f : n.radius;
        friction[v] = n.isStatic ? 0.0f : n.friction;
    }

    // skins
    int jc = (int)jointElems.size();
    std::vector<float> bw((size_t)nv * jc, 0.0f);
    std::vector<int32_t> bi((size_t)nv * jc, 0);
    for (int v = 0; v < nv; ++v) {
        const FeNode& n = fe.nodes[meshClothNodes[v]];
        std::vector<std::pair<int,float>> infl; // joint idx, weight
        if (!n.isStatic) {
            auto it = fe.fitWeights.find(meshClothNodes[v]);
            if (it != fe.fitWeights.end()) {
                for (auto& pr : it->second) {
                    if (pr.first < 0 || pr.first >= (int)fe.nodes.size()) continue;
                    auto jt = boneIdx.find(fe.nodes[pr.first].name);
                    if (jt == boneIdx.end()) continue;
                    auto ji = boneToJoint.find(jt->second);
                    if (ji == boneToJoint.end()) continue;
                    infl.push_back({ji->second, pr.second});
                }
            }
        }
        float sum = 0.0f;
        for (auto& pr : infl) sum += pr.second;
        if (sum > 1.0f && !infl.empty()) { for (auto& pr : infl) pr.second /= sum; sum = 1.0f; }
        int domJoint = -1;
        int domBoneNode = ResolveBoneNode(fe, meshClothNodes[v]);
        if (domBoneNode >= 0) {
            auto jt = boneIdx.find(fe.nodes[domBoneNode].name);
            if (jt != boneIdx.end()) {
                auto ji = boneToJoint.find(jt->second);
                if (ji != boneToJoint.end()) domJoint = ji->second;
            }
        }
        if (domJoint < 0 && !infl.empty()) domJoint = infl[0].first;
        if (n.isStatic) {
            infl.clear();
            // pin dominant bone plus any static-bone pins assigned to this vertex
            std::vector<std::pair<int,float>> pins;
            auto pinIt = staticPinWeights.find(meshClothNodes[v]);
            if (pinIt != staticPinWeights.end()) {
                for (auto& pr : pinIt->second) {
                    if (pr.first < 0 || pr.first >= (int)fe.nodes.size()) continue;
                    auto jt = boneIdx.find(fe.nodes[pr.first].name);
                    if (jt == boneIdx.end()) continue;
                    auto ji = boneToJoint.find(jt->second);
                    if (ji == boneToJoint.end()) continue;
                    pins.push_back({ji->second, pr.second});
                }
            }
            float pinSum = 0.0f;
            for (auto& pr : pins) pinSum += pr.second;
            if (pinSum > 0.9f) { for (auto& pr : pins) pr.second *= 0.9f / pinSum; pinSum = 0.9f; }
            if (domJoint >= 0) infl.push_back({domJoint, 1.0f - pinSum});
            for (auto& pr : pins) infl.push_back(pr);
        } else {
            float rem = 1.0f;
            for (auto& pr : infl) rem -= pr.second;
            if (rem < 0.0f) rem = 0.0f;
            if (domJoint >= 0 && (infl.empty() || rem > 0.0f)) {
                bool merged = false;
                for (auto& pr : infl) if (pr.first == domJoint) { pr.second += rem; merged = true; break; }
                if (!merged) infl.push_back({domJoint, rem});
            }
        }
        if (infl.empty() && domJoint < 0 && jc > 0) infl.push_back({0, 1.0f});
        int slot = 0;
        for (auto& pr : infl) {
            if (slot >= jc) break;
            bw[(size_t)v * jc + slot] = pr.second;
            bi[(size_t)v * jc + slot] = pr.first;
            ++slot;
        }
    }

    // faces
    std::set<std::pair<int,int>> meshEdges;
    std::unordered_map<int,int> nodeToVert;
    for (int v = 0; v < nv; ++v) nodeToVert[meshClothNodes[v]] = v;
    for (auto& e : fe.structuralEdges) {
        if (!fe.nodes[e.first].cloth || !fe.nodes[e.second].cloth) continue;
        if (fe.nodes[e.first].mesh != meshIndex) continue;
        meshEdges.insert(e);
    }
    int uncovered = 0;
    std::array<float, 3> outwardRef{0, 0, 0};
    {
        std::vector<std::array<float, 3>> bw = ComputeBoneWorldPositions(bones);
        std::vector<std::array<float, 3>> refs;
        for (const std::string& n : rigidParentBoneNames) {
            auto it = boneIdx.find(n);
            if (it != boneIdx.end()) refs.push_back(bw[it->second]);
        }
        if (refs.empty()) refs = bw;
        for (auto& r : refs) { outwardRef[0] += r[0]; outwardRef[1] += r[1]; outwardRef[2] += r[2]; }
        if (!refs.empty()) { outwardRef[0] /= refs.size(); outwardRef[1] /= refs.size(); outwardRef[2] /= refs.size(); }
    }
    std::vector<ProxyFace> faces = RecoverFaces(meshEdges, fe, outwardRef, uncovered);
    if (uncovered > 0) {
        outLog += "Warning: " + std::to_string(uncovered) + " structural rod(s) could not be covered by faces. ";
    }

    std::vector<int32_t> posIdx, facesArr, nrmIdx, uvIdx;
    std::vector<std::array<float,3>> nrm;
    std::vector<int32_t> mapIdx;
    for (auto& f : faces) {
        std::array<float,3> fn = FaceNormalOf(f, fe);
        for (int corner : f.corners) {
            int v = nodeToVert[corner];
            facesArr.push_back((int32_t)posIdx.size());
            posIdx.push_back(v);
            mapIdx.push_back(v);
            nrmIdx.push_back((int32_t)nrm.size());
            nrm.push_back(fn);
            uvIdx.push_back(0);
        }
        facesArr.push_back(-1);
    }
    if (faces.empty()) {
        for (int v = 0; v < nv; ++v) { posIdx.push_back(v); mapIdx.push_back(v); }
    }
    int nsv = (int)posIdx.size();
    if ((int)uvIdx.size() < nsv) uvIdx.resize(nsv, 0);

    std::vector<std::string> fmt = {"position$0","normal$0","texcoord$0","blendweights$0","blendindices$0",
                                    "cloth_enable$0","cloth_goal_strength_v2$0","cloth_drag_v2$0","cloth_collision_radius$0"};
    if (fe.hasFriction) fmt.push_back("cloth_friction$0");
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeStringArray(doc.AddString("vertexFormat"), fmt));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeBool(doc.AddString("flipVCoordinates"), true));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeInt(doc.AddString("jointCount"), jc));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeVec3Array(doc.AddString("position$0"), pos));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("position$0Indices"), posIdx));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeVec2Array(doc.AddString("texcoord$0"), {{0.0f, 0.0f}}));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("texcoord$0Indices"), uvIdx));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeVec3Array(doc.AddString("normal$0"), nrm.empty() ? std::vector<std::array<float,3>>{{0,0,1}} : nrm));
    std::vector<int32_t> nrmIdxFull = nrmIdx.empty() ? std::vector<int32_t>(nsv, 0) : nrmIdx;
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("normal$0Indices"), nrmIdxFull));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeFloatArray(doc.AddString("blendweights$0"), bw));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("blendindices$0"), bi));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeFloatArray(doc.AddString("cloth_enable$0"), enable));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("cloth_enable$0Indices"), mapIdx));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeFloatArray(doc.AddString("cloth_goal_strength_v2$0"), goal));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("cloth_goal_strength_v2$0Indices"), mapIdx));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeFloatArray(doc.AddString("cloth_drag_v2$0"), drag));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("cloth_drag_v2$0Indices"), mapIdx));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeFloatArray(doc.AddString("cloth_collision_radius$0"), radius));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("cloth_collision_radius$0Indices"), mapIdx));
    if (fe.hasFriction) {
        dmxbin::SetAttr(elems[vdata], dmxbin::MakeFloatArray(doc.AddString("cloth_friction$0"), friction));
        dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("cloth_friction$0Indices"), mapIdx));
    }

    dmxbin::SetAttr(elems[fset], dmxbin::MakeElement(doc.AddString("material"), mat));
    dmxbin::SetAttr(elems[fset], dmxbin::MakeIntArray(doc.AddString("faces"), facesArr));
    {
        dmxbin::Attr mn; mn.name = doc.AddString("mtlName"); mn.type = dmxbin::AT_STRING;
        uint32_t si = doc.AddString("no_material");
        mn.raw.resize(4); memcpy(mn.raw.data(), &si, 4);
        dmxbin::SetAttr(elems[mat], std::move(mn));
    }
    {
        std::vector<uint32_t> tl = jointXforms;
        tl.push_back(dagXf);
        dmxbin::SetAttr(elems[tlist], dmxbin::MakeElementArray(doc.AddString("transforms"), tl));
    }
    dmxbin::SetAttr(elems[axis], dmxbin::MakeInt(doc.AddString("upAxis"), 3));
    dmxbin::SetAttr(elems[axis], dmxbin::MakeInt(doc.AddString("forwardParity"), 1));
    dmxbin::SetAttr(elems[axis], dmxbin::MakeInt(doc.AddString("coordSys"), 0));

    doc.elems = std::move(elems);
    doc.pre = {0,0,0,0,0};

    std::string err;
    if (!dmxbin::WriteFile(dmxDiskPath, doc, err)) {
        outLog += "Failed to write proxy dmx: " + err + " ";
        return false;
    }
    outVertCount = nv;
    outFaceCount = (int)faces.size();
    return true;
}

std::string BuildClothProxyMeshFileBlock(const std::string& name, const std::string& filename,
                                         const std::string& entryIndent, const std::string& fieldIndent) {
    std::string b;
    b += entryIndent + "{\n";
    b += fieldIndent + "_class = \"ClothProxyMeshFile\"\n";
    b += fieldIndent + "name = \"" + name + "\"\n";
    b += fieldIndent + "filename = \"" + filename + "\"\n";
    b += fieldIndent + "import_scale = 1.0\n";
    b += fieldIndent + "back_solve_joints = true\n";
    b += fieldIndent + "back_solve_joints_drive_meshes = false\n";
    b += fieldIndent + "flex_cloth_borders = false\n";
    b += fieldIndent + "add_bones_to_render_mesh = false\n";
    b += fieldIndent + "back_solve_influence_threshold = 0.05\n";
    b += fieldIndent + "cloth_friction_bias = 0.0\n";
    b += fieldIndent + "cloth_friction_scale = 1.0\n";
    b += fieldIndent + "lock_friction_0 = false\n";
    b += fieldIndent + "lock_friction_1 = false\n";
    b += fieldIndent + "cloth_goal_strength_bias = 0.0\n";
    b += fieldIndent + "cloth_goal_strength_scale = 1.0\n";
    b += fieldIndent + "lock_goal_strength_0 = false\n";
    b += fieldIndent + "lock_goal_strength_1 = false\n";
    b += fieldIndent + "cloth_drag_scale = 1.0\n";
    b += fieldIndent + "cloth_mass_scale = 1.0\n";
    b += fieldIndent + "cloth_gravity_scale = 1.0\n";
    b += fieldIndent + "cloth_collision_radius_scale = 1.0\n";
    b += fieldIndent + "cloth_ground_collision_scale = 1.0\n";
    b += fieldIndent + "cloth_ground_friction_scale = 1.0\n";
    b += fieldIndent + "cloth_use_rods_scale = 1.0\n";
    b += fieldIndent + "cloth_make_rods_scale = 1.0\n";
    b += fieldIndent + "cloth_anchor_free_rotate_scale = 1.0\n";
    b += fieldIndent + "cloth_volumetric_scale = 1.0\n";
    b += fieldIndent + "cloth_suspenders_scale = 1.0\n";
    b += fieldIndent + "cloth_bend_stiffness_scale = 1.0\n";
    b += fieldIndent + "cloth_stray_radius_inv_scale = 1.0\n";
    b += fieldIndent + "cloth_stray_radius_scale = 1.0\n";
    b += fieldIndent + "cloth_stray_radius_stretchiness_scale = 1.0\n";
    b += fieldIndent + "import_filter = \n";
    b += fieldIndent + "{\n";
    b += fieldIndent + "\texclude_by_default = false\n";
    b += fieldIndent + "\texception_list = [  ]\n";
    b += fieldIndent + "}\n";
    b += entryIndent + "}";
    return b;
}

bool FeParamsNeedClothParams(const FeData& fe) {
    return fe.expAirDrag != 0.0 || fe.velAirDrag != 0.0 || fe.gravityScale != 1.0 ||
           fe.internalPressure != 0.0 || fe.windage != 0.0 || fe.windDrag != 0.0 ||
           fe.surfaceStretch != 0.0 || fe.rodSmoothRate != 0.0 || fe.rodSmoothIters != 0 ||
           fe.localForce != 1.0 || fe.localRotation != 0.0 || fe.localDrag1 != 0.0 ||
           fe.addWorldCollisionRadius != 2.0 || fe.extraIter != 0 || fe.extraGoalIter != 0 ||
           fe.extraPressureIter != 0;
}

std::string BuildClothParamsBlock(const FeData& fe, const std::string& entryIndent, const std::string& fieldIndent) {
    std::string b;
    b += entryIndent + "{\n";
    b += fieldIndent + "_class = \"ClothParams\"\n";
    auto f = [&](const char* key, double v) { b += fieldIndent + key + " = " + FormatKVFloat((float)v) + "\n"; };
    auto i = [&](const char* key, int64_t v) { b += fieldIndent + key + " = " + std::to_string(v) + "\n"; };
    f("default_stretch", fe.surfaceStretch);
    f("additional_shear_stretch", 0.0);
    i("extra_iterations", fe.extraIter);
    i("extra_goal_iterations", fe.extraGoalIter);
    i("extra_pressure_iterations", fe.extraPressureIter);
    f("goal_strength_bias", 0.0);
    f("default_gravity_scale", fe.gravityScale);
    f("default_vel_air_drag", fe.velAirDrag);
    f("default_exp_air_drag", fe.expAirDrag);
    f("velocity_smooth_rate", fe.rodSmoothRate);
    f("internal_pressure", fe.internalPressure);
    f("windage", fe.windage);
    f("wind_drag", fe.windDrag);
    i("velocity_smooth_iterations", fe.rodSmoothIters);
    f("default_ground_friction", 0.0);
    f("default_world_collision_penetration", 0.0);
    f("add_world_collision_radius", fe.addWorldCollisionRadius);
    f("local_force", fe.localForce);
    f("local_rotation", fe.localRotation);
    f("add_curvature", 0.76);
    f("quad_bend_tolerance", 0.05);
    f("local_drag1", fe.localDrag1);
    b += fieldIndent + "follow_the_lead = false\n";
    b += fieldIndent + "use_per_node_local_force_and_rotation = false\n";
    b += fieldIndent + "uninertial_rods = false\n";
    b += fieldIndent + "explicit_masses = false\n";
    b += fieldIndent + "unitless_damping = true\n";
    b += fieldIndent + "force_world_collision_on_all_nodes = false\n";
    b += fieldIndent + "new_style = true\n";
    b += fieldIndent + "can_collide_with_world_hulls = false\n";
    b += fieldIndent + "can_collide_with_world_meshes = false\n";
    b += fieldIndent + "can_collide_with_world_capsule_and_spheres = false\n";
    b += fieldIndent + "add_stiffness_rods = true\n";
    b += fieldIndent + "rigid_edge_hinges = false\n";
    b += fieldIndent + "add_bend_only_rods = false\n";
    b += fieldIndent + "immovable = false\n";
    b += entryIndent + "}";
    return b;
}

} // namespace

bool ParsePhysBlock(const std::string& physBlockText, ParsedClothData& out, std::string& outLog) {
    out.shapes.clear();
    out.ctrlNames.clear();
    outLog.clear();

    size_t ctrlPos = physBlockText.find("m_CtrlName");
    if (ctrlPos == std::string::npos) {
        outLog = "m_CtrlName not found in PHYS block.";
        return false;
    }
    size_t arrOpen = physBlockText.find('[', ctrlPos);
    if (arrOpen == std::string::npos) {
        outLog = "m_CtrlName array not found.";
        return false;
    }
    size_t arrStart, arrEnd;
    if (!FindBalancedBlock(physBlockText, arrOpen, '[', ']', arrStart, arrEnd)) {
        outLog = "Failed to parse m_CtrlName array.";
        return false;
    }
    out.ctrlNames = ParseStringArray(physBlockText, arrStart + 1, arrEnd - 1);

    auto parseSphereRigids = [&](const std::string& key, bool tapered) -> int {
        size_t pos = physBlockText.find(key);
        if (pos == std::string::npos) return 0;
        size_t open = physBlockText.find('[', pos);
        if (open == std::string::npos) return 0;
        size_t start, end;
        if (!FindBalancedBlock(physBlockText, open, '[', ']', start, end)) return 0;
        std::vector<std::string> blocks = SplitTopLevelBlocks(physBlockText, start + 1, end - 1);
        int count = 0;
        for (const std::string& b : blocks) {
            int nNode = -1, mask = 15;
            ExtractKeyInt(b, 0, "nNode", nNode);
            ExtractKeyInt(b, 0, "nCollisionMask", mask);
            if (nNode < 0 || static_cast<size_t>(nNode) >= out.ctrlNames.size()) continue;
            const std::string& parent = out.ctrlNames[nNode];
            if (parent.empty() || parent.size() >= 6 && parent.compare(0, 6, "$cloth") == 0) continue;

            size_t vPos = b.find("vSphere");
            if (vPos == std::string::npos) continue;
            size_t vOpen = b.find('[', vPos);
            if (vOpen == std::string::npos) continue;
            size_t vStart, vEnd;
            if (!FindBalancedBlock(b, vOpen, '[', ']', vStart, vEnd)) continue;

            if (tapered) {
                std::vector<std::array<float, 4>> points;
                size_t innerPos = vStart + 1;
                while (innerPos < vEnd - 1) {
                    size_t innerOpen = b.find('[', innerPos);
                    if (innerOpen == std::string::npos || innerOpen >= vEnd) break;
                    size_t innerStart, innerEnd;
                    if (!FindBalancedBlock(b, innerOpen, '[', ']', innerStart, innerEnd) || innerEnd > vEnd) break;
                    std::vector<float> a = ParseFloatArray(b, innerStart + 1, innerEnd - 1);
                    if (a.size() != 4) break;
                    points.push_back({ a[0], a[1], a[2], a[3] });
                    innerPos = innerEnd;
                }
                if (points.size() != 2) continue;
                auto cap = std::make_unique<ClothCapsule>();
                cap->parentBone = parent;
                cap->collisionMask = mask;
                cap->point0 = { points[0][0], points[0][1], points[0][2] };
                cap->radius0 = points[0][3];
                cap->point1 = { points[1][0], points[1][1], points[1][2] };
                cap->radius1 = points[1][3];
                out.shapes.push_back(std::move(cap));
            } else {
                std::vector<float> v = ParseFloatArray(b, vStart + 1, vEnd - 1);
                if (v.size() != 4) continue;
                auto sph = std::make_unique<ClothSphere>();
                sph->parentBone = parent;
                sph->collisionMask = mask;
                sph->center = { v[0], v[1], v[2] };
                sph->radius = v[3];
                out.shapes.push_back(std::move(sph));
            }
            ++count;
        }
        return count;
    };

    int sphereCount = parseSphereRigids("m_SphereRigids", false);
    int capsuleCount = parseSphereRigids("m_TaperedCapsuleRigids", true);

    size_t boxPos = physBlockText.find("m_BoxRigids");
    if (boxPos != std::string::npos) {
        size_t boxOpen = physBlockText.find('[', boxPos);
        if (boxOpen != std::string::npos) {
            size_t boxStart, boxEnd;
            if (FindBalancedBlock(physBlockText, boxOpen, '[', ']', boxStart, boxEnd)) {
                std::vector<std::string> boxBlocks = SplitTopLevelBlocks(physBlockText, boxStart + 1, boxEnd - 1);
                if (!boxBlocks.empty()) {
                    outLog += "Box rigids present but not yet supported (" + std::to_string(boxBlocks.size()) + " entries skipped). ";
                }
            }
        }
    }

    size_t sdfPos = physBlockText.find("m_SDFRigids");
    if (sdfPos == std::string::npos) sdfPos = physBlockText.find("m_SdfRigids");
    if (sdfPos != std::string::npos) {
        size_t sdfOpen = physBlockText.find('[', sdfPos);
        if (sdfOpen != std::string::npos) {
            size_t sdfStart, sdfEnd;
            if (FindBalancedBlock(physBlockText, sdfOpen, '[', ']', sdfStart, sdfEnd)) {
                std::vector<std::string> sdfBlocks = SplitTopLevelBlocks(physBlockText, sdfStart + 1, sdfEnd - 1);
                if (!sdfBlocks.empty()) {
                    outLog += "SDF rigids present but not yet supported (" + std::to_string(sdfBlocks.size()) + " entries skipped). ";
                }
            }
        }
    }

    std::unordered_map<std::string, int> nameCounts;
    for (auto& shape : out.shapes) {
        std::string base = shape->parentBone + "_cloth" + (dynamic_cast<ClothSphere*>(shape.get()) ? "Sphere" : "Capsule");
        int idx = ++nameCounts[base];
        shape->name = (idx == 1) ? base : base + "_" + std::to_string(idx);
    }

    outLog += "Parsed " + std::to_string(out.ctrlNames.size()) + " control names, " +
               std::to_string(sphereCount) + " spheres, " +
               std::to_string(capsuleCount) + " capsules.";
    return true;
}

namespace {

bool BuildJiggleProxyDmx(const JiggleChainData& jig, int meshIndex,
                         const std::vector<SkeletonBone>& bones,
                         const std::vector<std::string>& rigidParentBoneNames,
                         const std::string& dmxDiskPath,
                         std::unordered_map<int, std::string>& outNodeVertName,
                         int& outVertCount, int& outFaceCount, std::string& outLog);

std::string BuildClothVertexMapBlock(const JiggleChainData::VtxMap& map,
                                     const std::vector<std::pair<std::string, float>>& nodes,
                                     const std::string& entryIndent, const std::string& fieldIndent);

std::string BuildClothEffectStiffenBlock(const JiggleChainData::Effect& fx, const std::string& mapName,
                                         const std::string& entryIndent, const std::string& fieldIndent);

} // namespace

bool ApplyClothSimRestore(const std::string& vmdlPath, const std::string& physBlockText, std::string& outLog) {
    outLog.clear();

    std::string vmdl;
    if (!ReadFile(vmdlPath, vmdl)) {
        outLog = "Failed to read .vmdl: " + vmdlPath;
        return false;
    }

    if (vmdl.find("ClothProxyMeshFile") != std::string::npos) {
        outLog = "ClothProxyMeshFile already present; skipping.";
        return true;
    }

    ParsedClothData data;
    std::string parseLog;
    bool shapesOk = ParsePhysBlock(physBlockText, data, parseLog);
    (void)shapesOk;

    FeData fe;
    std::string feLog;
    bool feOk = ParseFeModel(physBlockText, fe, feLog);

    std::map<int, std::vector<int>> clothMeshes;
    if (feOk) {
        for (int i = 0; i < (int)fe.nodes.size(); ++i)
            if (fe.nodes[i].cloth) clothMeshes[fe.nodes[i].mesh].push_back(i);
        for (auto& kv : clothMeshes)
            std::sort(kv.second.begin(), kv.second.end(), [&](int a, int b) { return fe.nodes[a].point < fe.nodes[b].point; });
    }

    JiggleChainData jig;
    std::string jigParseLog;
    bool jigOk = ParseJiggleChainData(physBlockText, jig, jigParseLog);
    bool hasJiggle = jigOk && !jig.groups.empty();

    bool hasSoftbody = vmdl.find("_class = \"Softbody\"") != std::string::npos;

    std::vector<BuiltProxy> proxies;
    std::vector<std::string> vertexMapBlocks;
    std::string proxyLog;
    if (!clothMeshes.empty() || hasJiggle) {
        std::vector<SkeletonBone> bones;
        std::string skelLog;
        if (!ParseVmdlSkeleton(vmdl, bones, skelLog)) {
            proxyLog = skelLog + " Cloth proxy mesh skipped.";
        } else {
            // Pins: static bone nodes from the original FeModel that would otherwise
            // vanish from the rebuilt ctrl set (they were referenced by the original
            // proxy only through small secondary skin weights).
            std::unordered_map<int, std::vector<std::pair<int, float>>> staticPinWeights;
            {
                std::unordered_map<std::string, int> boneIdx;
                for (size_t i = 0; i < bones.size(); ++i) boneIdx[bones[i].name] = (int)i;
                std::set<std::string> referenced;
                for (int i = 0; i < (int)fe.nodes.size(); ++i) {
                    if (!fe.nodes[i].cloth || !fe.nodes[i].isStatic) continue;
                    int dom = ResolveBoneNode(fe, i);
                    if (dom >= 0) referenced.insert(fe.nodes[dom].name);
                }
                for (const auto& shape : data.shapes) referenced.insert(shape->parentBone);
                for (int i = 0; i < (int)fe.nodes.size(); ++i) {
                    if (fe.nodes[i].cloth || !fe.nodes[i].isStatic) continue;
                    if (referenced.count(fe.nodes[i].name)) continue;
                    if (!boneIdx.count(fe.nodes[i].name)) continue;
                    int best = -1;
                    float bestD = 1e30f;
                    for (int v = 0; v < (int)fe.nodes.size(); ++v) {
                        if (!fe.nodes[v].cloth || !fe.nodes[v].isStatic) continue;
                        float dx = fe.nodes[v].pos[0] - fe.nodes[i].pos[0];
                        float dy = fe.nodes[v].pos[1] - fe.nodes[i].pos[1];
                        float dz = fe.nodes[v].pos[2] - fe.nodes[i].pos[2];
                        float d = dx * dx + dy * dy + dz * dz;
                        if (d < bestD) { bestD = d; best = v; }
                    }
                    if (best >= 0) staticPinWeights[best].push_back({i, 0.05f});
                }
            }
            {
                size_t pinCount = 0;
                for (auto& kv : staticPinWeights) pinCount += kv.second.size();
                proxyLog += "static pins: " + std::to_string(pinCount) + ". ";
            }

                std::vector<std::string> rigidParentBoneNames;
                for (const auto& shape : data.shapes) rigidParentBoneNames.push_back(shape->parentBone);

            fs::path vmdlFs(vmdlPath);
            std::string stem = vmdlFs.stem().string();
            std::string vmdlDir = vmdlFs.parent_path().generic_string();
            std::string gameDir;
            {
                std::string dirLower = vmdlDir;
                std::transform(dirLower.begin(), dirLower.end(), dirLower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                size_t mp = dirLower.rfind("/models/");
                if (mp != std::string::npos) gameDir = vmdlDir.substr(mp + 1);
                else if (dirLower.rfind("models/", 0) == 0) gameDir = vmdlDir;
            }
            int meshOrdinal = 0;
            for (auto& kv : clothMeshes) {
                BuiltProxy bp;
                bp.meshIndex = kv.first;
                bp.proxyName = stem + "_cloth_proxy";
                if (clothMeshes.size() > 1) bp.proxyName += "_m" + std::to_string(kv.first);
                std::string fileName = bp.proxyName + ".dmx";
                bp.dmxDiskPath = vmdlDir + "/" + fileName;
                bp.dmxGamePath = gameDir.empty() ? fileName : gameDir + "/" + fileName;
                int vc = 0, fc = 0;
                std::string buildLog;
                if (!BuildProxyDmx(fe, kv.first, kv.second, bones, staticPinWeights, rigidParentBoneNames, bp.dmxDiskPath, vc, fc, buildLog)) {
                    proxyLog += "Proxy mesh build failed: " + buildLog;
                    continue;
                }
                bp.vertCount = vc;
                bp.faceCount = fc;
                proxyLog += "Wrote proxy mesh " + bp.dmxDiskPath + " (" + std::to_string(vc) + " verts, " + std::to_string(fc) + " faces). " + buildLog;
                proxies.push_back(bp);
                ++meshOrdinal;
            }
            (void)meshOrdinal;

            if (hasJiggle) {
                BuiltProxy bp;
                bp.meshIndex = clothMeshes.empty() ? 0 : (clothMeshes.rbegin()->first + 1);
                bp.proxyName = stem + "_jiggle_proxy";
                std::string fileName = bp.proxyName + ".dmx";
                bp.dmxDiskPath = vmdlDir + "/" + fileName;
                bp.dmxGamePath = gameDir.empty() ? fileName : gameDir + "/" + fileName;
                std::unordered_map<int, std::string> nodeVertName;
                int vc = 0, fc = 0;
                std::string buildLog;
                if (!BuildJiggleProxyDmx(jig, bp.meshIndex, bones, rigidParentBoneNames, bp.dmxDiskPath, nodeVertName, vc, fc, buildLog)) {
                    proxyLog += "Jiggle proxy build failed: " + buildLog;
                } else {
                    bp.vertCount = vc;
                    bp.faceCount = fc;
                    proxyLog += "Wrote jiggle proxy mesh " + bp.dmxDiskPath + " (" + std::to_string(vc) + " verts, " + std::to_string(fc) + " faces). " + buildLog;
                    proxies.push_back(bp);

                    std::set<std::string> allowedBones;
                    {
                        std::unordered_map<std::string, int> boneIdx;
                        for (size_t i = 0; i < bones.size(); ++i) boneIdx[bones[i].name] = (int)i;
                        for (const auto& g : jig.groups)
                            if (boneIdx.count(g.boneName)) allowedBones.insert(g.boneName);
                        for (const auto& n : rigidParentBoneNames) allowedBones.insert(n);
                    }
                    int totalSkipped = 0;
                    for (const auto& map : jig.vertexMaps) {
                        std::vector<std::pair<std::string, float>> nodes;
                        std::set<std::string> seen;
                        for (auto& nw : map.nodeWeights) {
                            std::string name;
                            auto vt = nodeVertName.find(nw.first);
                            if (vt != nodeVertName.end()) {
                                name = vt->second;
                            } else if (feOk && nw.first >= 0 && nw.first < (int)fe.nodes.size() &&
                                       allowedBones.count(fe.nodes[nw.first].name)) {
                                name = fe.nodes[nw.first].name;
                            } else {
                                ++totalSkipped;
                                continue;
                            }
                            if (seen.count(name)) continue;
                            seen.insert(name);
                            nodes.push_back({ name, nw.second });
                        }
                        if (!nodes.empty())
                            vertexMapBlocks.push_back(BuildClothVertexMapBlock(map, nodes, "\t\t\t\t", "\t\t\t\t\t"));
                    }
                    if (totalSkipped > 0)
                        proxyLog += "Vertex maps: " + std::to_string(totalSkipped) + " node reference(s) skipped (not a cloth node). ";

                    int skippedFx = 0;
                    for (const auto& fx : jig.effects) {
                        if (fx.type != 3) { ++skippedFx; continue; }
                        std::string mapName;
                        for (const auto& m : jig.vertexMaps)
                            if (m.nameHash == fx.vertexMapHash) { mapName = m.name; break; }
                        if (mapName.empty()) { ++skippedFx; continue; }
                        vertexMapBlocks.push_back(BuildClothEffectStiffenBlock(fx, mapName, "\t\t\t\t", "\t\t\t\t\t"));
                    }
                    if (skippedFx > 0)
                        proxyLog += "Effects: " + std::to_string(skippedFx) + " skipped (unsupported type or unresolved vertex map). ";
                }
            }
        }
    }

    std::vector<std::string> proxyBlocks;
    for (const BuiltProxy& bp : proxies)
        proxyBlocks.push_back(BuildClothProxyMeshFileBlock(bp.proxyName, bp.dmxGamePath, "\t\t\t\t", "\t\t\t\t\t"));
    for (const std::string& vmb : vertexMapBlocks)
        proxyBlocks.push_back(vmb);
    std::string clothParamsBlock;
    if (feOk && FeParamsNeedClothParams(fe) && !proxies.empty())
        clothParamsBlock = BuildClothParamsBlock(fe, "\t\t\t\t", "\t\t\t\t\t");

    std::string patched;
    if (!hasSoftbody) {
        if (data.shapes.empty() && proxies.empty()) {
            outLog = "No cloth collision shapes or cloth meshes found in PHYS block.";
            return true;
        }
        size_t arrStart, arrEnd;
        if (!FindRootChildrenArray(vmdl, arrStart, arrEnd)) {
            outLog = "Could not find rootNode children array.";
            return false;
        }
        std::string softbody = BuildSoftbodyBlock(data, proxyBlocks, clothParamsBlock);
        size_t insertPos = arrEnd - 1;
        while (insertPos > arrStart + 1 && std::isspace(static_cast<unsigned char>(vmdl[insertPos - 1]))) --insertPos;
        bool needsComma = (insertPos == arrStart + 1 || vmdl[insertPos - 1] != ',');
        std::string insertion = "\n" + softbody + ",";
        if (needsComma) insertion = "," + insertion;
        patched = vmdl.substr(0, insertPos) + insertion + vmdl.substr(arrEnd - 1);
    } else {
        if (proxies.empty()) {
            outLog = "Softbody block already present; " + parseLog + " " + proxyLog;
            return true;
        }
        size_t sbPos = vmdl.find("_class = \"Softbody\"");
        size_t childrenPos = vmdl.find("children =", sbPos);
        if (childrenPos == std::string::npos) {
            outLog = "Softbody block has no children array; cannot insert proxy mesh.";
            return false;
        }
        size_t arrOpen = vmdl.find('[', childrenPos);
        size_t arrStart, arrEnd;
        if (arrOpen == std::string::npos || !FindBalancedBlock(vmdl, arrOpen, '[', ']', arrStart, arrEnd)) {
            outLog = "Could not parse Softbody children array.";
            return false;
        }
        std::string insertion = "\n";
        for (size_t i = 0; i < proxyBlocks.size(); ++i) {
            insertion += proxyBlocks[i] + ",\n";
        }
        if (!clothParamsBlock.empty()) insertion += clothParamsBlock + ",\n";
        patched = vmdl.substr(0, arrStart + 1) + insertion + vmdl.substr(arrStart + 1);
    }

    if (!IsKV3Balanced(patched)) {
        outLog = "Patched .vmdl is not KV3-balanced; aborting.";
        return false;
    }

    if (!WriteFile(vmdlPath, patched)) {
        outLog = "Failed to write patched .vmdl.";
        return false;
    }

    outLog = parseLog + " " + proxyLog;
    if (!data.shapes.empty()) outLog += "Wrote " + std::to_string(data.shapes.size()) + " cloth shapes. ";
    if (!proxies.empty()) outLog += "Added " + std::to_string(proxies.size()) + " cloth proxy mesh reference(s) to " + vmdlPath + ".";
    return true;
}

} // namespace cloth_sim_restore
// ---------- stage 3: legacy jiggle chain reconstruction ($cc particle pairs) ----------
//
// Older compiled models (pre-m_JiggleBones era, e.g. faceless_void_arcana_base.vmdl_c)
// encode ModelDoc jiggle chains directly in the FeModel as cloth particles:
// every jiggled bone B gets two particles "$ccB_0" / "$ccB_1" at +/-length/2 along the
// bone axis (m_InitPose), linked by rigid rods (flMinDist == flMaxDist) to the
// child/grandchild particles, while B itself becomes a position-driven back-solve node.
// The current resourcecompiler cannot emit that encoding from JiggleBone nodes anymore
// (it writes m_JiggleBones instead, losing rods / goal strengths / per-particle
// collision / vertex maps), so stage 3 rebuilds the simulation through the cloth path:
// a generated cloth proxy mesh whose vertices are the $cc particles (2 per bone),
// connected by ribbon quads between consecutive chain bones. The compiler then
// re-creates the rods, goal springs, collision radii and back-solved bones natively.
// Vertex maps (m_VertexMaps / m_VertexMapValues) are restored as ClothVertexMap
// children of the Softbody; together with the AE_CL_CLOTH_EFFECT animation events
// (which Source2Viewer decompiles into the .vmdl) this lets the compiler regenerate
// m_Effects (<map>_stiffen[_N] / <map>_unstiffen naming convention).

namespace cloth_sim_restore {

bool ParseJiggleChainData(const std::string& physBlockText, JiggleChainData& out, std::string& outLog) {
    out = JiggleChainData();
    kv3t::Value root;
    if (!kv3t::Parse(physBlockText, root)) {
        outLog = "Failed to parse PHYS KV3 text.";
        return false;
    }
    const kv3t::Value* fe = root.find("m_pFeModel");
    if (!fe) {
        outLog = "No m_pFeModel in PHYS block.";
        return false;
    }
    const kv3t::Value* cn = fe->find("m_CtrlName");
    if (!cn || !cn->isArray()) {
        outLog = "No m_CtrlName in m_pFeModel.";
        return false;
    }
    std::vector<std::string> names = cn->asStringArray();
    int64_t nStatic = fe->find("m_nStaticNodes") ? fe->find("m_nStaticNodes")->asInt() : 0;

    std::vector<double> invmass = fe->find("m_NodeInvMasses") ? fe->find("m_NodeInvMasses")->asDoubleArray() : std::vector<double>{};
    std::vector<double> radii = fe->find("m_NodeCollisionRadii") ? fe->find("m_NodeCollisionRadii")->asDoubleArray() : std::vector<double>{};
    std::vector<double> friction = fe->find("m_DynNodeFriction") ? fe->find("m_DynNodeFriction")->asDoubleArray() : std::vector<double>{};

    std::vector<std::array<float, 3>> pos(names.size(), { 0.0f, 0.0f, 0.0f });
    if (const kv3t::Value* ip = fe->find("m_InitPose")) {
        for (size_t i = 0; i < ip->arr.size() && i < names.size(); ++i) {
            std::vector<double> v = ip->arr[i].asDoubleArray();
            if (v.size() >= 3) pos[i] = { (float)v[0], (float)v[1], (float)v[2] };
        }
    }
    std::vector<double> forceAttr(names.size(), 0.0), vertAttr(names.size(), 0.0);
    if (const kv3t::Value* ni = fe->find("m_NodeIntegrator")) {
        for (size_t i = 0; i < ni->arr.size() && i < names.size(); ++i) {
            if (const kv3t::Value* a = ni->arr[i].find("flAnimationForceAttraction")) forceAttr[i] = a->asDouble();
            if (const kv3t::Value* a = ni->arr[i].find("flAnimationVertexAttraction")) vertAttr[i] = a->asDouble();
        }
    }

    std::unordered_map<std::string, bool> isName;
    for (const std::string& n : names) isName[n] = true;

    auto dynVal = [&](const std::vector<double>& arr, int node) -> double {
        int64_t d = (int64_t)node - nStatic;
        return (d >= 0 && d < (int64_t)arr.size()) ? arr[d] : 0.0;
    };
    auto fillEndpoint = [&](JiggleChainData::Group& g, bool first, int node) {
        bool isStatic = ((size_t)node < invmass.size() && invmass[node] == 0.0) || node < nStatic;
        if (first) {
            g.n0 = node;
            g.static0 = isStatic;
            g.radius0 = (float)dynVal(radii, node);
            g.fric0 = (float)dynVal(friction, node);
            g.forceAttr0 = (float)forceAttr[node];
            g.vertAttr0 = (float)vertAttr[node];
            g.pos0 = pos[node];
        } else {
            g.n1 = node;
            g.static1 = isStatic;
            g.radius1 = (float)dynVal(radii, node);
            g.fric1 = (float)dynVal(friction, node);
            g.forceAttr1 = (float)forceAttr[node];
            g.vertAttr1 = (float)vertAttr[node];
            g.pos1 = pos[node];
        }
    };

    {
        std::unordered_map<std::string, size_t> groupIdx;
        for (size_t i = 0; i < names.size(); ++i) {
            const std::string& n = names[i];
            if (n.rfind("$cc", 0) != 0) continue;
            size_t us = n.rfind('_');
            if (us == std::string::npos || us + 2 != n.size()) continue;
            char ep = n[us + 1];
            if (ep != '0' && ep != '1') continue;
            std::string bone = n.substr(3, us - 3);
            if (bone.empty() || !isName.count(bone)) continue;
            auto it = groupIdx.find(bone);
            if (it == groupIdx.end()) {
                groupIdx[bone] = out.groups.size();
                JiggleChainData::Group g;
                g.boneName = bone;
                out.groups.push_back(g);
                it = groupIdx.find(bone);
            }
            fillEndpoint(out.groups[it->second], ep == '0', (int)i);
        }
    }

    // vertex maps: named per-node weight sets (used by cloth effects)
    const kv3t::Value* vm = fe->find("m_VertexMaps");
    const kv3t::Value* vv = fe->find("m_VertexMapValues");
    if (vm && vm->isArray() && vv && vv->isArray()) {
        std::vector<int64_t> values = vv->asIntArray();
        for (const auto& e : vm->arr) {
            JiggleChainData::VtxMap map;
            if (const kv3t::Value* s = e.find("sName")) map.name = s->scalar;
            if (map.name.empty()) continue;
            if (const kv3t::Value* h = e.find("nNameHash")) map.nameHash = (uint32_t)h->asInt();
            if (const kv3t::Value* c = e.find("nColor")) map.color = (uint32_t)c->asInt();
            if (const kv3t::Value* v = e.find("flVolumetricSolveStrength")) map.volumetricSolve = (float)v->asDouble();
            int64_t base = e.find("nVertexBase") ? e.find("nVertexBase")->asInt() : 0;
            int64_t count = e.find("nVertexCount") ? e.find("nVertexCount")->asInt() : 0;
            int64_t offset = e.find("nMapOffset") ? e.find("nMapOffset")->asInt() : 0;
            for (int64_t i = 0; i < count; ++i) {
                int64_t vi = offset + i;
                if (vi < 0 || vi >= (int64_t)values.size()) break;
                if (values[vi] <= 0) continue;
                map.nodeWeights.push_back({ (int)(base + i), (float)values[vi] / 255.0f });
            }
            out.vertexMaps.push_back(std::move(map));
        }
    }

    // cloth effects (e.g. stiffen/unstiffen hooks triggered by AE_CL_CLOTH_EFFECT anim events)
    if (const kv3t::Value* ef = fe->find("m_Effects")) {
        if (ef->isArray()) {
            for (const auto& e : ef->arr) {
                JiggleChainData::Effect fx;
                if (const kv3t::Value* s = e.find("sName")) fx.name = s->scalar;
                if (fx.name.empty()) continue;
                if (const kv3t::Value* t = e.find("nType")) fx.type = t->asInt();
                if (const kv3t::Value* p = e.find("m_Params")) {
                    if (const kv3t::Value* s = p->find("Stiffness")) fx.stiffness = (float)s->asDouble();
                    if (const kv3t::Value* b = p->find("BoneOverlay")) fx.boneOverlay = (float)b->asDouble();
                    if (const kv3t::Value* v = p->find("VertexMap")) fx.vertexMapHash = (uint32_t)v->asInt();
                }
                out.effects.push_back(fx);
            }
        }
    }

    outLog = "Parsed " + std::to_string(out.groups.size()) + " $cc jiggle groups, " +
             std::to_string(out.vertexMaps.size()) + " vertex maps, " +
             std::to_string(out.effects.size()) + " effects.";
    return !out.groups.empty();
}

namespace {

bool BuildJiggleProxyDmx(const JiggleChainData& jig, int meshIndex,
                         const std::vector<SkeletonBone>& bones,
                         const std::vector<std::string>& rigidParentBoneNames,
                         const std::string& dmxDiskPath,
                         std::unordered_map<int, std::string>& outNodeVertName,
                         int& outVertCount, int& outFaceCount, std::string& outLog) {
    std::unordered_map<std::string, int> boneIdx;
    for (size_t i = 0; i < bones.size(); ++i) boneIdx[bones[i].name] = (int)i;

    // verts: 2 per group, in group order; point index = vert index
    struct Vert {
        int node = -1;      // original FeModel node index (for vertex map naming)
        int bone = -1;      // vmdl skeleton bone index
        bool isStatic = false;
        std::array<float, 3> pos{};
        float radius = 0.0f;
        float friction = 0.0f;
        float forceAttr = 0.0f;
        float vertAttr = 0.0f;
    };
    std::vector<Vert> verts;
    std::vector<int> groupVertBase(jig.groups.size(), -1);
    std::vector<int> nodeToVert(jig.groups.size() * 2, -1);
    {
        std::unordered_map<int, int> nodeToGroupVert;
        for (size_t gi = 0; gi < jig.groups.size(); ++gi) {
            const JiggleChainData::Group& g = jig.groups[gi];
            auto bt = boneIdx.find(g.boneName);
            if (bt == boneIdx.end()) {
                outLog += "Jiggle bone '" + g.boneName + "' not found in vmdl skeleton; skipped. ";
                continue;
            }
            groupVertBase[gi] = (int)verts.size();
            Vert v0, v1;
            v0.node = g.n0; v0.bone = bt->second; v0.isStatic = g.static0;
            v0.pos = g.pos0; v0.radius = g.radius0; v0.friction = g.fric0;
            v0.forceAttr = g.forceAttr0; v0.vertAttr = g.vertAttr0;
            v1.node = g.n1; v1.bone = bt->second; v1.isStatic = g.static1;
            v1.pos = g.pos1; v1.radius = g.radius1; v1.friction = g.fric1;
            v1.forceAttr = g.forceAttr1; v1.vertAttr = g.vertAttr1;
            verts.push_back(v0);
            verts.push_back(v1);
            nodeToGroupVert[g.n0] = (int)verts.size() - 2;
            nodeToGroupVert[g.n1] = (int)verts.size() - 1;
        }
        for (auto& kv : nodeToGroupVert)
            outNodeVertName[kv.first] = "$cloth_m" + std::to_string(meshIndex) + "p" + std::to_string(kv.second);
        (void)nodeToVert;
    }
    if (verts.empty()) {
        outLog = "No jiggle groups matched the vmdl skeleton.";
        return false;
    }

    // ribbon quads between consecutive chain bones (via skeleton parenting)
    std::unordered_map<std::string, int> groupOfBone;
    for (size_t gi = 0; gi < jig.groups.size(); ++gi)
        if (groupVertBase[gi] >= 0) groupOfBone[jig.groups[gi].boneName] = (int)gi;

    std::vector<std::array<float, 3>> boneWorld = ComputeBoneWorldPositions(bones);
    std::array<float, 3> outwardRef{ 0, 0, 0 };
    {
        std::vector<std::array<float, 3>> refs;
        for (const std::string& n : rigidParentBoneNames) {
            auto it = boneIdx.find(n);
            if (it != boneIdx.end()) refs.push_back(boneWorld[it->second]);
        }
        if (refs.empty()) refs = boneWorld;
        for (auto& r : refs) { outwardRef[0] += r[0]; outwardRef[1] += r[1]; outwardRef[2] += r[2]; }
        if (!refs.empty()) { outwardRef[0] /= refs.size(); outwardRef[1] /= refs.size(); outwardRef[2] /= refs.size(); }
    }

    std::vector<std::array<int, 4>> quads;
    for (size_t gi = 0; gi < jig.groups.size(); ++gi) {
        int vb = groupVertBase[gi];
        if (vb < 0) continue;
        const std::string& boneName = jig.groups[gi].boneName;
        int bi = boneIdx[boneName];
        int parentBone = bones[bi].parent;
        while (parentBone >= 0) {
            auto pt = groupOfBone.find(bones[parentBone].name);
            if (pt != groupOfBone.end()) {
                int pb = groupVertBase[pt->second];
                if (pb >= 0) {
                    std::array<int, 4> q = { pb, pb + 1, vb + 1, vb };
                    // orient outward (Newell normal should point away from the body)
                    std::array<float, 3> n{ 0, 0, 0 }, c{ 0, 0, 0 };
                    for (int k = 0; k < 4; ++k) {
                        const auto& a = verts[q[k]].pos;
                        const auto& b = verts[q[(k + 1) % 4]].pos;
                        n[0] += (a[1] - b[1]) * (a[2] + b[2]);
                        n[1] += (a[2] - b[2]) * (a[0] + b[0]);
                        n[2] += (a[0] - b[0]) * (a[1] + b[1]);
                        c[0] += a[0] * 0.25f; c[1] += a[1] * 0.25f; c[2] += a[2] * 0.25f;
                    }
                    float dot = n[0] * (c[0] - outwardRef[0]) + n[1] * (c[1] - outwardRef[1]) + n[2] * (c[2] - outwardRef[2]);
                    if (dot < 0.0f) q = { q[1], q[0], q[3], q[2] };
                    quads.push_back(q);
                }
                break;
            }
            parentBone = bones[parentBone].parent;
        }
    }

    dmxbin::Doc doc;
    std::vector<dmxbin::Element> elems;
    elems.reserve(16 + bones.size() * 2);
    uint32_t guidCounter = 0;
    auto addElem = [&](const char* type, const char* name) -> uint32_t {
        dmxbin::Element e;
        e.type = doc.AddString(type);
        e.name = doc.AddString(name);
        uint32_t g = ++guidCounter;
        for (int i = 0; i < 16; ++i) e.guid[i] = (uint8_t)((g >> ((i & 3) * 8)) ^ (i * 37));
        elems.push_back(std::move(e));
        return (uint32_t)elems.size() - 1;
    };

    uint32_t scene = addElem("DmElement", "Scene");
    uint32_t model = addElem("DmeModel", "cloth_proxy");
    std::vector<uint32_t> jointElems, jointXforms;
    std::vector<int> jointBone;
    {
        std::function<void(int)> dfs = [&](int bi) {
            uint32_t j = addElem("DmeJoint", bones[bi].name.c_str());
            uint32_t t = addElem("DmeTransform", bones[bi].name.c_str());
            jointElems.push_back(j);
            jointXforms.push_back(t);
            jointBone.push_back(bi);
            for (size_t k = 0; k < bones.size(); ++k) if (bones[k].parent == bi) dfs((int)k);
        };
        for (size_t i = 0; i < bones.size(); ++i) if (bones[i].parent == -1) dfs((int)i);
    }
    std::unordered_map<int, int> boneToJoint;
    for (size_t i = 0; i < jointBone.size(); ++i) boneToJoint[jointBone[i]] = (int)i;

    uint32_t dag = addElem("DmeDag", "cloth_proxy");
    uint32_t mesh = addElem("DmeMesh", "cloth_proxy");
    uint32_t vdata = addElem("DmeVertexData", "bind");
    uint32_t fset = addElem("DmeFaceSet", "no_material");
    uint32_t mat = addElem("DmeMaterial", "no_material");
    uint32_t dagXf = addElem("DmeTransform", "cloth_proxy");
    uint32_t tlist = addElem("DmeTransformList", "base");
    uint32_t axis = addElem("DmeAxisSystem", "axisSystem");
    uint32_t modelXf = addElem("DmeTransform", "");

    dmxbin::SetAttr(elems[scene], dmxbin::MakeElement(doc.AddString("skeleton"), model));
    dmxbin::SetAttr(elems[scene], dmxbin::MakeElement(doc.AddString("model"), model));

    std::vector<uint32_t> modelChildren;
    for (size_t i = 0; i < bones.size(); ++i)
        if (bones[i].parent == -1) {
            auto it = boneToJoint.find((int)i);
            if (it != boneToJoint.end()) modelChildren.push_back(jointElems[it->second]);
        }
    modelChildren.push_back(dag);
    dmxbin::SetAttr(elems[model], dmxbin::MakeElementArray(doc.AddString("children"), modelChildren));
    dmxbin::SetAttr(elems[model], dmxbin::MakeElementArray(doc.AddString("baseStates"), { tlist }));
    dmxbin::SetAttr(elems[model], dmxbin::MakeElement(doc.AddString("axisSystem"), axis));
    dmxbin::SetAttr(elems[model], dmxbin::MakeElement(doc.AddString("transform"), modelXf));
    dmxbin::SetAttr(elems[model], dmxbin::MakeElementArray(doc.AddString("jointList"), jointElems));

    for (size_t i = 0; i < jointElems.size(); ++i) {
        const SkeletonBone& b = bones[jointBone[i]];
        dmxbin::SetAttr(elems[jointElems[i]], dmxbin::MakeElement(doc.AddString("transform"), jointXforms[i]));
        std::vector<uint32_t> ch;
        for (size_t k = 0; k < bones.size(); ++k)
            if (bones[k].parent == jointBone[i]) {
                auto it = boneToJoint.find((int)k);
                if (it != boneToJoint.end()) ch.push_back(jointElems[it->second]);
            }
        dmxbin::SetAttr(elems[jointElems[i]], dmxbin::MakeElementArray(doc.AddString("children"), ch));

        dmxbin::Attr pa; pa.name = doc.AddString("position"); pa.type = dmxbin::AT_VECTOR3;
        pa.raw.resize(12); memcpy(pa.raw.data(), b.origin.data(), 12);
        dmxbin::SetAttr(elems[jointXforms[i]], std::move(pa));
        float q[4];
        QuatFromSourceAngles(b.angles[0], b.angles[1], b.angles[2], q);
        dmxbin::Attr oa; oa.name = doc.AddString("orientation"); oa.type = dmxbin::AT_QUATERNION;
        oa.raw.resize(16); memcpy(oa.raw.data(), q, 16);
        dmxbin::SetAttr(elems[jointXforms[i]], std::move(oa));
    }

    dmxbin::SetAttr(elems[dag], dmxbin::MakeElement(doc.AddString("shape"), mesh));
    dmxbin::SetAttr(elems[dag], dmxbin::MakeElement(doc.AddString("transform"), dagXf));
    auto writeIdentXf = [&](uint32_t xf) {
        dmxbin::Attr pa; pa.name = doc.AddString("position"); pa.type = dmxbin::AT_VECTOR3;
        pa.raw.resize(12); memset(pa.raw.data(), 0, 12);
        dmxbin::SetAttr(elems[xf], std::move(pa));
        dmxbin::Attr oa; oa.name = doc.AddString("orientation"); oa.type = dmxbin::AT_QUATERNION;
        oa.raw.resize(16); float q[4] = { 0,0,0,1 }; memcpy(oa.raw.data(), q, 16);
        dmxbin::SetAttr(elems[xf], std::move(oa));
    };
    writeIdentXf(dagXf);
    writeIdentXf(modelXf);

    dmxbin::SetAttr(elems[mesh], dmxbin::MakeBool(doc.AddString("visible"), true));
    dmxbin::SetAttr(elems[mesh], dmxbin::MakeElement(doc.AddString("bindState"), vdata));
    dmxbin::SetAttr(elems[mesh], dmxbin::MakeElement(doc.AddString("currentState"), vdata));
    dmxbin::SetAttr(elems[mesh], dmxbin::MakeElementArray(doc.AddString("baseStates"), { vdata }));
    dmxbin::SetAttr(elems[mesh], dmxbin::MakeElementArray(doc.AddString("faceSets"), { fset }));

    int nv = (int)verts.size();
    int jc = (int)jointElems.size();
    std::vector<std::array<float, 3>> pos(nv);
    std::vector<float> enable(nv), goal(nv), drag(nv), radius(nv), friction(nv);
    std::vector<float> bw((size_t)nv * jc, 0.0f);
    std::vector<int32_t> bi((size_t)nv * jc, 0);
    bool hasFriction = false;
    for (int v = 0; v < nv; ++v) {
        const Vert& t = verts[v];
        pos[v] = t.pos;
        enable[v] = t.isStatic ? 0.0f : 1.0f;
        double g3 = t.forceAttr;
        if (g3 < 0.0) g3 = 0.0;
        if (g3 > 1.0) g3 = 1.0;
        double g = std::cbrt(g3);
        if (g > 0.999) g = 0.999;
        goal[v] = (float)g;
        double f;
        if (g3 >= 0.9999) f = t.vertAttr;
        else f = ((double)t.vertAttr - g3) / (1.0 - g3);
        if (f < 0.0) f = 0.0;
        if (f > 1.0) f = 1.0;
        drag[v] = (float)(1.0 - std::pow(1.0 - f, 60.0));
        radius[v] = t.isStatic ? 0.0f : t.radius;
        friction[v] = t.isStatic ? 0.0f : t.friction;
        if (friction[v] != 0.0f) hasFriction = true;
        auto ji = boneToJoint.find(t.bone);
        if (ji != boneToJoint.end()) {
            bw[(size_t)v * jc] = 1.0f;
            bi[(size_t)v * jc] = ji->second;
        }
    }

    std::vector<int32_t> posIdx, facesArr, nrmIdx, uvIdx, mapIdx;
    std::vector<std::array<float, 3>> nrm;
    for (auto& q : quads) {
        std::array<float, 3> fn{ 0, 0, 0 };
        for (int k = 0; k < 4; ++k) {
            const auto& a = verts[q[k]].pos;
            const auto& b = verts[q[(k + 1) % 4]].pos;
            fn[0] += (a[1] - b[1]) * (a[2] + b[2]);
            fn[1] += (a[2] - b[2]) * (a[0] + b[0]);
            fn[2] += (a[0] - b[0]) * (a[1] + b[1]);
        }
        float l = std::sqrt(fn[0] * fn[0] + fn[1] * fn[1] + fn[2] * fn[2]);
        if (l < 1e-12f) fn = { 0, 0, 1 }; else { fn[0] /= l; fn[1] /= l; fn[2] /= l; }
        for (int k = 0; k < 4; ++k) {
            facesArr.push_back((int32_t)posIdx.size());
            posIdx.push_back(q[k]);
            mapIdx.push_back(q[k]);
            nrmIdx.push_back((int32_t)nrm.size());
            uvIdx.push_back(0);
        }
        nrm.push_back(fn);
        facesArr.push_back(-1);
    }
    if (facesArr.empty()) {
        for (int v = 0; v < nv; ++v) { posIdx.push_back(v); mapIdx.push_back(v); }
    }
    int nsv = (int)posIdx.size();
    if ((int)uvIdx.size() < nsv) uvIdx.resize(nsv, 0);

    std::vector<std::string> fmt = { "position$0","normal$0","texcoord$0","blendweights$0","blendindices$0",
                                     "cloth_enable$0","cloth_goal_strength_v2$0","cloth_drag_v2$0","cloth_collision_radius$0" };
    if (hasFriction) fmt.push_back("cloth_friction$0");
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeStringArray(doc.AddString("vertexFormat"), fmt));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeBool(doc.AddString("flipVCoordinates"), true));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeInt(doc.AddString("jointCount"), jc));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeVec3Array(doc.AddString("position$0"), pos));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("position$0Indices"), posIdx));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeVec2Array(doc.AddString("texcoord$0"), {{0.0f, 0.0f}}));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("texcoord$0Indices"), uvIdx));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeVec3Array(doc.AddString("normal$0"), nrm.empty() ? std::vector<std::array<float, 3>>{{0,0,1}} : nrm));
    std::vector<int32_t> nrmIdxFull = nrmIdx.empty() ? std::vector<int32_t>(nsv, 0) : nrmIdx;
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("normal$0Indices"), nrmIdxFull));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeFloatArray(doc.AddString("blendweights$0"), bw));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("blendindices$0"), bi));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeFloatArray(doc.AddString("cloth_enable$0"), enable));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("cloth_enable$0Indices"), mapIdx));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeFloatArray(doc.AddString("cloth_goal_strength_v2$0"), goal));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("cloth_goal_strength_v2$0Indices"), mapIdx));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeFloatArray(doc.AddString("cloth_drag_v2$0"), drag));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("cloth_drag_v2$0Indices"), mapIdx));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeFloatArray(doc.AddString("cloth_collision_radius$0"), radius));
    dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("cloth_collision_radius$0Indices"), mapIdx));
    if (hasFriction) {
        dmxbin::SetAttr(elems[vdata], dmxbin::MakeFloatArray(doc.AddString("cloth_friction$0"), friction));
        dmxbin::SetAttr(elems[vdata], dmxbin::MakeIntArray(doc.AddString("cloth_friction$0Indices"), mapIdx));
    }

    dmxbin::SetAttr(elems[fset], dmxbin::MakeElement(doc.AddString("material"), mat));
    dmxbin::SetAttr(elems[fset], dmxbin::MakeIntArray(doc.AddString("faces"), facesArr));
    {
        dmxbin::Attr mn; mn.name = doc.AddString("mtlName"); mn.type = dmxbin::AT_STRING;
        uint32_t si = doc.AddString("no_material");
        mn.raw.resize(4); memcpy(mn.raw.data(), &si, 4);
        dmxbin::SetAttr(elems[mat], std::move(mn));
    }
    {
        std::vector<uint32_t> tl = jointXforms;
        tl.push_back(dagXf);
        dmxbin::SetAttr(elems[tlist], dmxbin::MakeElementArray(doc.AddString("transforms"), tl));
    }
    dmxbin::SetAttr(elems[axis], dmxbin::MakeInt(doc.AddString("upAxis"), 3));
    dmxbin::SetAttr(elems[axis], dmxbin::MakeInt(doc.AddString("forwardParity"), 1));
    dmxbin::SetAttr(elems[axis], dmxbin::MakeInt(doc.AddString("coordSys"), 0));

    doc.elems = std::move(elems);
    doc.pre = { 0,0,0,0,0 };

    std::string err;
    if (!dmxbin::WriteFile(dmxDiskPath, doc, err)) {
        outLog += "Failed to write jiggle proxy dmx: " + err + " ";
        return false;
    }
    outVertCount = nv;
    outFaceCount = (int)quads.size();
    return true;
}

std::string BuildClothVertexMapBlock(const JiggleChainData::VtxMap& map,
                                     const std::vector<std::pair<std::string, float>>& nodes,
                                     const std::string& entryIndent, const std::string& fieldIndent) {
    std::string b = entryIndent + "{\n";
    b += fieldIndent + "_class = \"ClothVertexMap\"\n";
    b += fieldIndent + "name = \"" + map.name + "\"\n";
    b += fieldIndent + "weight = 1.0\n";
    b += fieldIndent + "aliases = \"\"\n";
    uint32_t r = map.color & 0xFF, g = (map.color >> 8) & 0xFF, bl = (map.color >> 16) & 0xFF;
    b += fieldIndent + "color = [ " + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(bl) + " ]\n";
    b += fieldIndent + "volumetric_solve = " + FormatKVFloat(map.volumetricSolve) + "\n";
    b += fieldIndent + "scale_source_node = \"\"\n";
    b += fieldIndent + "data = \n";
    b += fieldIndent + "{\n";
    b += fieldIndent + "\tnodes = \n";
    b += fieldIndent + "\t{\n";
    std::string nodeIndent = fieldIndent + "\t\t";
    for (const auto& nd : nodes) {
        b += nodeIndent + "\"" + nd.first + "\" = \n";
        b += nodeIndent + "{\n";
        b += nodeIndent + "\tweight = " + FormatKVFloat(nd.second) + "\n";
        b += nodeIndent + "}\n";
    }
    b += fieldIndent + "\t}\n";
    b += fieldIndent + "}\n";
    b += entryIndent + "}";
    return b;
}

std::string BuildClothEffectStiffenBlock(const JiggleChainData::Effect& fx, const std::string& mapName,
                                         const std::string& entryIndent, const std::string& fieldIndent) {
    std::string b = entryIndent + "{\n";
    b += fieldIndent + "_class = \"ClothEffectStiffen\"\n";
    b += fieldIndent + "name = \"" + fx.name + "\"\n";
    b += fieldIndent + "origin = [ 0.0, 0.0, 0.0 ]\n";
    b += fieldIndent + "angles = [ 0.0, 0.0, 0.0 ]\n";
    b += fieldIndent + "vertex_map = \"" + mapName + "\"\n";
    b += fieldIndent + "cloth_effect_version = 0\n";
    b += fieldIndent + "Stiffness = " + FormatKVFloat(fx.stiffness) + "\n";
    b += fieldIndent + "BoneOverlay = " + FormatKVFloat(fx.boneOverlay) + "\n";
    b += entryIndent + "}";
    return b;
}

} // namespace

} // namespace cloth_sim_restore
