#include "DmxBinary.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>

namespace dmxbin {

namespace {

inline uint32_t ru32(const uint8_t* p) {
    uint32_t v; std::memcpy(&v, p, 4); return v;
}
inline int32_t ri32(const uint8_t* p) {
    int32_t v; std::memcpy(&v, p, 4); return v;
}
inline float rf32(const uint8_t* p) {
    float v; std::memcpy(&v, p, 4); return v;
}
inline void wu32(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back(static_cast<uint8_t>(v & 0xFF));
    o.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    o.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    o.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}
inline void wi32(std::vector<uint8_t>& o, int32_t v) { wu32(o, static_cast<uint32_t>(v)); }
inline void wf32(std::vector<uint8_t>& o, float f) { uint32_t v; std::memcpy(&v, &f, 4); wu32(o, v); }

const char kComment[] = "<!-- dmx encoding binary 9 format model 22 -->\n";

} // namespace

int ScalarSize(uint8_t t) {
    switch (t) {
        case AT_ELEMENT: case AT_INT: case AT_FLOAT: case AT_STRING: case AT_TIME: case AT_COLOR: return 4;
        case AT_BOOL: case AT_UINT8: return 1;
        case AT_VECTOR2: case AT_INT64: return 8;
        case AT_VECTOR3: case AT_ANGLE: return 12;
        case AT_VECTOR4: case AT_QUATERNION: return 16;
        case AT_MATRIX: return 64;
        default: return -1;
    }
}

int ArrayElemSize(uint8_t arrayType) {
    if (!IsArray(arrayType)) return -1;
    uint8_t s = ArrayScalar(arrayType);
    if (s == AT_STRING || s == AT_VOID) return -1;
    return ScalarSize(s);
}

size_t ValueSize(const uint8_t* p, const uint8_t* end, uint8_t type) {
    if (p > end) return 0;
    if (type == AT_VOID) {
        if (p + 4 > end) return 0;
        int32_t n = ri32(p);
        if (n < 0) return 0;
        size_t total = 4ull + static_cast<size_t>(n);
        if (p + total > end) return 0;
        return total;
    }
    if (!IsArray(type)) {
        int ss = ScalarSize(type);
        if (ss < 0) return 0;
        if (p + ss > end) return 0;
        return static_cast<size_t>(ss);
    }
    if (p + 4 > end) return 0;
    uint32_t cnt = ru32(p);
    const uint8_t* q = p + 4;
    uint8_t s = ArrayScalar(type);
    if (s == AT_STRING) {
        for (uint32_t i = 0; i < cnt; ++i) {
            const uint8_t* z = q;
            while (z < end && *z != 0) ++z;
            if (z >= end) return 0;
            q = z + 1;
        }
        return static_cast<size_t>(q - p);
    }
    if (s == AT_VOID) {
        for (uint32_t i = 0; i < cnt; ++i) {
            if (q + 4 > end) return 0;
            int32_t n = ri32(q); q += 4;
            if (n < 0) return 0;
            if (q + n > end) return 0;
            q += n;
        }
        return static_cast<size_t>(q - p);
    }
    int es = ScalarSize(s);
    if (es < 0) return 0;
    size_t total = 4ull + static_cast<size_t>(cnt) * static_cast<size_t>(es);
    if (p + total > end) return 0;
    return total;
}

bool Read(const uint8_t* data, size_t size, Doc& out, std::string& err) {
    out = Doc{};
    const size_t clen = sizeof(kComment) - 1;
    if (size < clen || std::memcmp(data, kComment, clen) != 0) {
        err = "not a binary 9 / model 22 dmx (bad header)";
        return false;
    }
    const uint8_t* p = data + clen;
    const uint8_t* end = data + size;
    if (p + 5 + 4 + 1 > end) { err = "truncated header"; return false; }
    std::memcpy(out.pre.data(), p, 5); p += 5;
    uint32_t nStrings = ru32(p); p += 4;
    out.strings.reserve(nStrings);
    for (uint32_t i = 0; i < nStrings; ++i) {
        const uint8_t* z = p;
        while (z < end && *z != 0) ++z;
        if (z >= end) { err = "truncated string table"; return false; }
        out.strings.emplace_back(reinterpret_cast<const char*>(p), static_cast<size_t>(z - p));
        p = z + 1;
    }
    if (p + 4 > end) { err = "truncated element count"; return false; }
    uint32_t nElem = ru32(p); p += 4;
    out.elems.resize(nElem);
    for (uint32_t i = 0; i < nElem; ++i) {
        if (p + 24 > end) { err = "truncated element header"; return false; }
        Element& e = out.elems[i];
        e.type = ru32(p); e.name = ru32(p + 4);
        std::memcpy(e.guid.data(), p + 8, 16);
        p += 24;
    }
    for (uint32_t i = 0; i < nElem; ++i) {
        if (p + 4 > end) { err = "truncated attr count"; return false; }
        uint32_t nAttr = ru32(p); p += 4;
        Element& e = out.elems[i];
        e.attrs.reserve(nAttr);
        for (uint32_t a = 0; a < nAttr; ++a) {
            if (p + 5 > end) { err = "truncated attr header"; return false; }
            Attr attr;
            attr.name = ru32(p); p += 4;
            attr.type = *p; p += 1;
            size_t vs = ValueSize(p, end, attr.type);
            if (vs == 0) { err = "bad value size (type " + std::to_string(attr.type) + ")"; return false; }
            attr.raw.assign(p, p + vs);
            p += vs;
            e.attrs.push_back(std::move(attr));
        }
    }
    if (p != end) { err = "trailing bytes after parse"; return false; }
    return true;
}

bool ReadFile(const std::string& path, Doc& out, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "cannot open: " + path; return false; }
    f.seekg(0, std::ios::end);
    std::streamoff n = f.tellg();
    if (n <= 0) { err = "empty file: " + path; return false; }
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(n));
    f.read(reinterpret_cast<char*>(buf.data()), n);
    return Read(buf.data(), buf.size(), out, err);
}

void Write(const Doc& doc, std::vector<uint8_t>& out) {
    out.clear();
    out.insert(out.end(), kComment, kComment + (sizeof(kComment) - 1));
    out.insert(out.end(), doc.pre.begin(), doc.pre.end());
    wu32(out, static_cast<uint32_t>(doc.strings.size()));
    for (const auto& s : doc.strings) {
        out.insert(out.end(), s.begin(), s.end());
        out.push_back(0);
    }
    wu32(out, static_cast<uint32_t>(doc.elems.size()));
    for (const auto& e : doc.elems) {
        wu32(out, e.type);
        wu32(out, e.name);
        out.insert(out.end(), e.guid.begin(), e.guid.end());
    }
    for (const auto& e : doc.elems) {
        wu32(out, static_cast<uint32_t>(e.attrs.size()));
        for (const auto& a : e.attrs) {
            wu32(out, a.name);
            out.push_back(a.type);
            out.insert(out.end(), a.raw.begin(), a.raw.end());
        }
    }
}

bool WriteFile(const std::string& path, const Doc& doc, std::string& err) {
    std::vector<uint8_t> buf;
    Write(doc, buf);
    std::ofstream f(path, std::ios::binary);
    if (!f) { err = "cannot write: " + path; return false; }
    f.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    return static_cast<bool>(f);
}

uint32_t Doc::FindString(const std::string& s) const {
    for (uint32_t i = 0; i < strings.size(); ++i)
        if (strings[i] == s) return i;
    return kNullElement;
}
uint32_t Doc::AddString(const std::string& s) {
    uint32_t i = FindString(s);
    if (i != kNullElement) return i;
    strings.push_back(s);
    return static_cast<uint32_t>(strings.size() - 1);
}
uint32_t Doc::FindStringCI(const std::string& s) const {
    for (uint32_t i = 0; i < strings.size(); ++i) {
        const std::string& t = strings[i];
        if (t.size() == s.size()) {
            bool eq = true;
            for (size_t k = 0; k < s.size(); ++k)
                if (std::tolower(static_cast<unsigned char>(t[k])) != std::tolower(static_cast<unsigned char>(s[k]))) { eq = false; break; }
            if (eq) return i;
        }
    }
    return kNullElement;
}
const Attr* Doc::FindAttr(const Element& e, uint32_t nameIdx) const {
    for (const auto& a : e.attrs) if (a.name == nameIdx) return &a;
    return nullptr;
}
Attr* Doc::FindAttr(Element& e, uint32_t nameIdx) {
    for (auto& a : e.attrs) if (a.name == nameIdx) return &a;
    return nullptr;
}

void SetAttr(Element& e, Attr a) {
    for (auto& x : e.attrs) if (x.name == a.name) { x = std::move(a); return; }
    e.attrs.push_back(std::move(a));
}

namespace {
bool need(const Attr& a, size_t n) { return a.raw.size() >= n; }
}

bool DecodeElement(const Attr& a, uint32_t& outIdx) {
    if (a.type != AT_ELEMENT || !need(a,4)) return false;
    outIdx = ru32(a.raw.data()); return true;
}
bool DecodeElementArray(const Attr& a, std::vector<uint32_t>& out) {
    if (a.type != AT_ELEMENT_ARRAY || !need(a,4)) return false;
    uint32_t c = ru32(a.raw.data()); if (!need(a, 4ull + 4ull*c)) return false;
    out.resize(c); for (uint32_t i=0;i<c;++i) out[i]=ru32(a.raw.data()+4+4*i); return true;
}
bool DecodeIntArray(const Attr& a, std::vector<int32_t>& out) {
    if (a.type != AT_INT_ARRAY || !need(a,4)) return false;
    uint32_t c=ru32(a.raw.data()); if(!need(a,4ull+4ull*c)) return false;
    out.resize(c); for(uint32_t i=0;i<c;++i) out[i]=ri32(a.raw.data()+4+4*i); return true;
}
bool DecodeUIntArray(const Attr& a, std::vector<uint32_t>& out) {
    if (a.type != AT_INT_ARRAY || !need(a,4)) return false;
    uint32_t c=ru32(a.raw.data()); if(!need(a,4ull+4ull*c)) return false;
    out.resize(c); for(uint32_t i=0;i<c;++i) out[i]=ru32(a.raw.data()+4+4*i); return true;
}
bool DecodeFloatArray(const Attr& a, std::vector<float>& out) {
    if (a.type != AT_FLOAT_ARRAY || !need(a,4)) return false;
    uint32_t c=ru32(a.raw.data()); if(!need(a,4ull+4ull*c)) return false;
    out.resize(c); for(uint32_t i=0;i<c;++i) out[i]=rf32(a.raw.data()+4+4*i); return true;
}
bool DecodeVec2Array(const Attr& a, std::vector<std::array<float,2>>& out) {
    if (a.type != AT_VECTOR2_ARRAY || !need(a,4)) return false;
    uint32_t c=ru32(a.raw.data()); if(!need(a,4ull+8ull*c)) return false;
    out.resize(c); for(uint32_t i=0;i<c;++i){out[i][0]=rf32(a.raw.data()+4+8*i);out[i][1]=rf32(a.raw.data()+4+8*i+4);} return true;
}
bool DecodeVec3Array(const Attr& a, std::vector<std::array<float,3>>& out) {
    if (a.type != AT_VECTOR3_ARRAY || !need(a,4)) return false;
    uint32_t c=ru32(a.raw.data()); if(!need(a,4ull+12ull*c)) return false;
    out.resize(c); for(uint32_t i=0;i<c;++i){out[i][0]=rf32(a.raw.data()+4+12*i);out[i][1]=rf32(a.raw.data()+4+12*i+4);out[i][2]=rf32(a.raw.data()+4+12*i+8);} return true;
}
bool DecodeStringArray(const Attr& a, std::vector<std::string>& out) {
    if (a.type != AT_STRING_ARRAY || !need(a,4)) return false;
    uint32_t c=ru32(a.raw.data()); const uint8_t* q=a.raw.data()+4; const uint8_t* end=a.raw.data()+a.raw.size();
    out.clear(); out.reserve(c);
    for(uint32_t i=0;i<c;++i){
        const uint8_t* z=q; while(z<end && *z) ++z; if(z>=end) return false;
        out.emplace_back(reinterpret_cast<const char*>(q), z-q); q=z+1;
    }
    return q==end;
}
bool DecodeBool(const Attr& a, bool& out){ if(a.type!=AT_BOOL||!need(a,1)) return false; out=a.raw[0]!=0; return true; }
bool DecodeFloat(const Attr& a, float& out){ if(a.type!=AT_FLOAT||!need(a,4)) return false; out=rf32(a.raw.data()); return true; }
bool DecodeInt(const Attr& a, int32_t& out){ if(a.type!=AT_INT||!need(a,4)) return false; out=ri32(a.raw.data()); return true; }

Attr MakeBool(uint32_t n, bool v){ Attr a{n,AT_BOOL,{}}; a.raw.push_back(v?1:0); return a; }
Attr MakeInt(uint32_t n, int32_t v){ Attr a{n,AT_INT,{}}; wi32(a.raw,v); return a; }
Attr MakeFloat(uint32_t n, float v){ Attr a{n,AT_FLOAT,{}}; wf32(a.raw,v); return a; }
Attr MakeElement(uint32_t n, uint32_t idx){ Attr a{n,AT_ELEMENT,{}}; wu32(a.raw,idx); return a; }
Attr MakeElementArray(uint32_t n, const std::vector<uint32_t>& v){ Attr a{n,AT_ELEMENT_ARRAY,{}}; wu32(a.raw,(uint32_t)v.size()); for(auto x:v) wu32(a.raw,x); return a; }
Attr MakeIntArray(uint32_t n, const std::vector<int32_t>& v){ Attr a{n,AT_INT_ARRAY,{}}; wu32(a.raw,(uint32_t)v.size()); for(auto x:v) wi32(a.raw,x); return a; }
Attr MakeUIntArray(uint32_t n, const std::vector<uint32_t>& v){ Attr a{n,AT_INT_ARRAY,{}}; wu32(a.raw,(uint32_t)v.size()); for(auto x:v) wu32(a.raw,x); return a; }
Attr MakeFloatArray(uint32_t n, const std::vector<float>& v){ Attr a{n,AT_FLOAT_ARRAY,{}}; wu32(a.raw,(uint32_t)v.size()); for(auto x:v) wf32(a.raw,x); return a; }
Attr MakeVec2Array(uint32_t n, const std::vector<std::array<float,2>>& v){ Attr a{n,AT_VECTOR2_ARRAY,{}}; wu32(a.raw,(uint32_t)v.size()); for(auto&x:v){wf32(a.raw,x[0]);wf32(a.raw,x[1]);} return a; }
Attr MakeVec3Array(uint32_t n, const std::vector<std::array<float,3>>& v){ Attr a{n,AT_VECTOR3_ARRAY,{}}; wu32(a.raw,(uint32_t)v.size()); for(auto&x:v){wf32(a.raw,x[0]);wf32(a.raw,x[1]);wf32(a.raw,x[2]);} return a; }
Attr MakeStringArray(uint32_t n, const std::vector<std::string>& v){ Attr a{n,AT_STRING_ARRAY,{}}; wu32(a.raw,(uint32_t)v.size()); for(auto&s:v){a.raw.insert(a.raw.end(),s.begin(),s.end());a.raw.push_back(0);} return a; }

} // namespace dmxbin
