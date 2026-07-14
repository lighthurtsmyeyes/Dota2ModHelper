#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dmxbin {

enum AttrType : uint8_t {
    AT_ELEMENT = 1, AT_INT = 2, AT_FLOAT = 3, AT_BOOL = 4, AT_STRING = 5,
    AT_VOID = 6, AT_TIME = 7, AT_COLOR = 8, AT_VECTOR2 = 9, AT_VECTOR3 = 10,
    AT_VECTOR4 = 11, AT_ANGLE = 12, AT_QUATERNION = 13, AT_MATRIX = 14,
    AT_INT64 = 15, AT_UINT8 = 16,
    AT_ELEMENT_ARRAY = 33, AT_INT_ARRAY = 34, AT_FLOAT_ARRAY = 35,
    AT_BOOL_ARRAY = 36, AT_STRING_ARRAY = 37, AT_VOID_ARRAY = 38,
    AT_TIME_ARRAY = 39, AT_COLOR_ARRAY = 40, AT_VECTOR2_ARRAY = 41,
    AT_VECTOR3_ARRAY = 42, AT_VECTOR4_ARRAY = 43, AT_ANGLE_ARRAY = 44,
    AT_QUATERNION_ARRAY = 45, AT_MATRIX_ARRAY = 46, AT_INT64_ARRAY = 47,
    AT_UINT8_ARRAY = 48,
};

constexpr uint32_t kNullElement = 0xFFFFFFFFu;

int ScalarSize(uint8_t t);
inline bool IsArray(uint8_t t) { return t >= 33 && t <= 48; }
inline uint8_t ArrayScalar(uint8_t t) { return static_cast<uint8_t>(t - 32); }
int ArrayElemSize(uint8_t arrayType);
size_t ValueSize(const uint8_t* p, const uint8_t* end, uint8_t type);

struct Attr {
    uint32_t name = 0;
    uint8_t type = 0;
    std::vector<uint8_t> raw;
};

struct Element {
    uint32_t type = 0;
    uint32_t name = 0;
    std::array<uint8_t, 16> guid{};
    std::vector<Attr> attrs;
};

struct Doc {
    std::array<uint8_t, 5> pre{};
    uint8_t mid = 0;
    std::vector<std::string> strings;
    std::vector<Element> elems;

    uint32_t FindString(const std::string& s) const;
    uint32_t AddString(const std::string& s);
    const Attr* FindAttr(const Element& e, uint32_t nameIdx) const;
    Attr* FindAttr(Element& e, uint32_t nameIdx);
    uint32_t FindStringCI(const std::string& s) const;
};

bool Read(const uint8_t* data, size_t size, Doc& out, std::string& err);
bool ReadFile(const std::string& path, Doc& out, std::string& err);
void Write(const Doc& doc, std::vector<uint8_t>& out);
bool WriteFile(const std::string& path, const Doc& doc, std::string& err);

// Typed decoders (read from Attr::raw). Return false on mismatch.
bool DecodeElement(const Attr& a, uint32_t& outIdx);
bool DecodeElementArray(const Attr& a, std::vector<uint32_t>& out);
bool DecodeIntArray(const Attr& a, std::vector<int32_t>& out);
bool DecodeUIntArray(const Attr& a, std::vector<uint32_t>& out);
bool DecodeFloatArray(const Attr& a, std::vector<float>& out);
bool DecodeVec2Array(const Attr& a, std::vector<std::array<float,2>>& out);
bool DecodeVec3Array(const Attr& a, std::vector<std::array<float,3>>& out);
bool DecodeStringArray(const Attr& a, std::vector<std::string>& out);
bool DecodeBool(const Attr& a, bool& out);
bool DecodeFloat(const Attr& a, float& out);
bool DecodeInt(const Attr& a, int32_t& out);

// Typed builders (produce Attr with given nameIdx).
Attr MakeBool(uint32_t nameIdx, bool v);
Attr MakeInt(uint32_t nameIdx, int32_t v);
Attr MakeFloat(uint32_t nameIdx, float v);
Attr MakeElement(uint32_t nameIdx, uint32_t elemIdx);
Attr MakeElementArray(uint32_t nameIdx, const std::vector<uint32_t>& v);
Attr MakeIntArray(uint32_t nameIdx, const std::vector<int32_t>& v);
Attr MakeUIntArray(uint32_t nameIdx, const std::vector<uint32_t>& v);
Attr MakeFloatArray(uint32_t nameIdx, const std::vector<float>& v);
Attr MakeVec2Array(uint32_t nameIdx, const std::vector<std::array<float,2>>& v);
Attr MakeVec3Array(uint32_t nameIdx, const std::vector<std::array<float,3>>& v);
Attr MakeStringArray(uint32_t nameIdx, const std::vector<std::string>& v);

void SetAttr(Element& e, Attr a);

} // namespace dmxbin
