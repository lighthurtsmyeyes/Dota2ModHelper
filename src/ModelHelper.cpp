#include "ModelHelper.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace {

std::string ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool WriteFile(const std::string& path, const std::string& data) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file << data;
    return file.good();
}

size_t LineStart(const std::string& text, size_t pos) {
    if (pos >= text.size()) return 0;
    while (pos > 0 && text[pos - 1] != '\n') --pos;
    return pos;
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

size_t FindOpeningBraceBefore(const std::string& text, size_t pos) {
    if (pos == 0 || pos > text.size()) return std::string::npos;
    bool inString = false;
    for (size_t i = pos - 1; i != std::string::npos; --i) {
        char c = text[i];
        if (inString) {
            if (c == '\\') {
                if (i == 0) break;
                --i;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == '{') {
            return i;
        }
    }
    return std::string::npos;
}

bool ExtractKeyString(const std::string& text, size_t pos, const char* key, std::string& outValue) {
    std::string keyEq = std::string(key) + " = ";
    size_t p = text.find(keyEq, pos);
    if (p == std::string::npos) {
        keyEq = std::string(key) + "=";
        p = text.find(keyEq, pos);
    }
    if (p == std::string::npos) return false;
    size_t quote = text.find('"', p + keyEq.size());
    if (quote == std::string::npos) return false;
    size_t endQuote = text.find('"', quote + 1);
    if (endQuote == std::string::npos) return false;
    outValue = text.substr(quote + 1, endQuote - quote - 1);
    return true;
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

std::string ExtractOwnName(const std::string& block) {
    std::string name;
    ExtractKeyString(block, 0, "name", name);
    return name;
}

std::string ExtractChildrenSection(const std::string& block) {
    size_t pos = block.find("children =");
    if (pos == std::string::npos) return {};

    size_t arrOpen = block.find('[', pos);
    if (arrOpen == std::string::npos) return {};

    size_t arrStart, arrEnd;
    if (!FindBalancedBlock(block, arrOpen, '[', ']', arrStart, arrEnd)) return {};

    size_t sectionStart = LineStart(block, pos);
    return block.substr(sectionStart, arrEnd - sectionStart);
}

std::vector<std::string> ExtractActivityModifiers(const std::string& childrenSection) {
    std::vector<std::string> result;
    if (childrenSection.empty()) return result;

    size_t arrOpen = childrenSection.find('[');
    if (arrOpen == std::string::npos) return result;

    size_t arrStart, arrEnd;
    if (!FindBalancedBlock(childrenSection, arrOpen, '[', ']', arrStart, arrEnd)) return result;

    std::vector<std::string> blocks = SplitTopLevelBlocks(childrenSection, arrStart + 1, arrEnd - 1);
    for (const auto& child : blocks) {
        std::string cls;
        if (ExtractKeyString(child, 0, "_class", cls) && cls == "ActivityModifier") {
            std::string modifierName;
            if (ExtractKeyString(child, 0, "activity_name", modifierName)) {
                result.push_back(std::move(modifierName));
            }
        }
    }
    return result;
}

struct ParsedAnimFile {
    size_t startPos = 0;
    size_t endPos = 0;
    std::string raw;
    std::string name;
    std::string activityName;
    std::vector<std::string> modifiers;
    std::string childrenSection;
};

std::string ExtractActivityName(const std::string& block);

void CollectAnimFilesRecursive(const std::string& vmdlText, size_t start, size_t end,
                               std::vector<ParsedAnimFile>& out) {
    std::vector<std::string> blocks = SplitTopLevelBlocks(vmdlText, start, end);
    size_t searchPos = start;

    for (const auto& block : blocks) {
        size_t blockStart = vmdlText.find(block, searchPos);
        if (blockStart == std::string::npos) continue;
        size_t blockEnd = blockStart + block.size();

        std::string cls;
        ExtractKeyString(block, 0, "_class", cls);

        if (cls == "AnimFile") {
            ParsedAnimFile anim;
            anim.startPos = blockStart;
            anim.endPos = blockEnd;
            anim.raw = block;
            anim.name = ExtractOwnName(block);
            anim.activityName = ExtractActivityName(block);
            anim.childrenSection = ExtractChildrenSection(block);
            anim.modifiers = ExtractActivityModifiers(anim.childrenSection);
            std::sort(anim.modifiers.begin(), anim.modifiers.end());
            out.push_back(std::move(anim));
        }
        else if (cls == "Folder") {
            size_t childrenPos = block.find("children =");
            if (childrenPos != std::string::npos) {
                size_t arrOpen = block.find('[', childrenPos);
                if (arrOpen != std::string::npos) {
                    size_t arrStart, arrEnd;
                    if (FindBalancedBlock(block, arrOpen, '[', ']', arrStart, arrEnd)) {
                        CollectAnimFilesRecursive(vmdlText, blockStart + arrStart, blockStart + arrEnd, out);
                    }
                }
            }
        }

        searchPos = blockEnd;
    }
}

std::vector<ParsedAnimFile> ExtractAnimFiles(const std::string& vmdlText) {
    std::vector<ParsedAnimFile> result;

    size_t animListPos = vmdlText.find("_class = \"AnimationList\"");
    if (animListPos == std::string::npos) return result;

    size_t listBrace = FindOpeningBraceBefore(vmdlText, animListPos);
    if (listBrace == std::string::npos) return result;

    size_t listStart, listEnd;
    if (!FindBalancedBlock(vmdlText, listBrace, '{', '}', listStart, listEnd)) return result;

    size_t childrenPos = vmdlText.find("children =", listStart);
    if (childrenPos == std::string::npos || childrenPos >= listEnd) return result;

    size_t arrOpen = vmdlText.find('[', childrenPos);
    if (arrOpen == std::string::npos || arrOpen >= listEnd) return result;

    size_t arrStart, arrEnd;
    if (!FindBalancedBlock(vmdlText, arrOpen, '[', ']', arrStart, arrEnd)) return result;

    CollectAnimFilesRecursive(vmdlText, arrStart + 1, arrEnd - 1, result);
    return result;
}

bool IsSubset(const std::vector<std::string>& subset, const std::vector<std::string>& superset) {
    if (subset.empty()) return true;
    auto it = superset.begin();
    for (const std::string& s : subset) {
        it = std::find(it, superset.end(), s);
        if (it == superset.end()) return false;
        ++it;
    }
    return true;
}

std::vector<std::string> SubtractModifiers(const std::vector<std::string>& from,
                                           const std::vector<std::string>& toRemove) {
    std::vector<std::string> result = from;
    for (const std::string& s : toRemove) {
        auto it = std::find(result.begin(), result.end(), s);
        if (it != result.end()) result.erase(it);
    }
    return result;
}

bool HasChildrenSection(const std::string& block) {
    return block.find("children =") != std::string::npos;
}

bool ReplaceNameInBlock(std::string& block, const std::string& newName) {
    std::string keyEq = "name = ";
    size_t p = block.find(keyEq);
    if (p == std::string::npos) {
        keyEq = "name=";
        p = block.find(keyEq);
    }
    if (p == std::string::npos) return false;

    size_t q1 = block.find('"', p + keyEq.size());
    if (q1 == std::string::npos) return false;
    size_t q2 = block.find('"', q1 + 1);
    if (q2 == std::string::npos) return false;

    block.replace(q1 + 1, q2 - q1 - 1, newName);
    return true;
}

bool RemoveChildrenSection(std::string& block) {
    size_t pos = block.find("children =");
    if (pos == std::string::npos) return true;

    size_t arrOpen = block.find('[', pos);
    if (arrOpen == std::string::npos) return false;

    size_t arrStart, arrEnd;
    if (!FindBalancedBlock(block, arrOpen, '[', ']', arrStart, arrEnd)) return false;

    size_t sectionStart = LineStart(block, pos);
    size_t sectionEnd = arrEnd;
    while (sectionEnd < block.size() && (block[sectionEnd] == '\n' || block[sectionEnd] == '\r')) ++sectionEnd;

    block.erase(sectionStart, sectionEnd - sectionStart);
    return true;
}

bool ReplaceChildrenSection(std::string& block, const std::string& newChildrenSection) {
    size_t pos = block.find("children =");
    if (pos == std::string::npos) return false;

    size_t arrOpen = block.find('[', pos);
    if (arrOpen == std::string::npos) return false;

    size_t arrStart, arrEnd;
    if (!FindBalancedBlock(block, arrOpen, '[', ']', arrStart, arrEnd)) return false;

    size_t sectionStart = LineStart(block, pos);
    size_t sectionEnd = arrEnd;
    while (sectionEnd < block.size() && (block[sectionEnd] == '\n' || block[sectionEnd] == '\r')) ++sectionEnd;

    block.replace(sectionStart, sectionEnd - sectionStart, newChildrenSection + "\n");
    return true;
}

bool InsertChildrenSection(std::string& block, const std::string& newChildrenSection) {
    size_t insertPos = block.size();
    for (const char* key : {"activity_weight", "activity_name"}) {
        size_t p = block.find(key);
        if (p != std::string::npos) {
            size_t lineEnd = block.find('\n', p);
            if (lineEnd != std::string::npos) {
                insertPos = lineEnd + 1;
                break;
            }
        }
    }

    if (insertPos == block.size()) {
        size_t closingBrace = block.rfind('}');
        if (closingBrace != std::string::npos) {
            insertPos = closingBrace;
        }
    }

    block.insert(insertPos, newChildrenSection + "\n");
    return true;
}

std::string ExtractActivityName(const std::string& block) {
    std::string temp = block;
    RemoveChildrenSection(temp);
    std::string activityName;
    ExtractKeyString(temp, 0, "activity_name", activityName);
    return activityName;
}

std::vector<std::string> ExtractChildBlocks(const std::string& childrenSection) {
    std::vector<std::string> result;
    if (childrenSection.empty()) return result;

    size_t arrOpen = childrenSection.find('[');
    if (arrOpen == std::string::npos) return result;

    size_t arrStart, arrEnd;
    if (!FindBalancedBlock(childrenSection, arrOpen, '[', ']', arrStart, arrEnd)) return result;

    return SplitTopLevelBlocks(childrenSection, arrStart + 1, arrEnd - 1);
}

std::string BuildMergedChildrenSection(const std::string& sourceChildrenSection, const std::string& targetChildrenSection) {
    std::vector<std::string> sourceBlocks = ExtractChildBlocks(sourceChildrenSection);
    std::vector<std::string> targetBlocks = ExtractChildBlocks(targetChildrenSection);

    std::vector<std::string> resultBlocks;
    for (const auto& block : sourceBlocks) {
        std::string cls;
        ExtractKeyString(block, 0, "_class", cls);
        if (cls != "ActivityModifier") {
            resultBlocks.push_back(block);
        }
    }
    for (const auto& block : targetBlocks) {
        std::string cls;
        ExtractKeyString(block, 0, "_class", cls);
        if (cls == "ActivityModifier") {
            resultBlocks.push_back(block);
        }
    }

    if (resultBlocks.empty()) {
        return {};
    }

    const std::string& sectionTemplate = !sourceChildrenSection.empty() ? sourceChildrenSection : targetChildrenSection;
    size_t arrOpen = sectionTemplate.find('[');
    size_t arrStart, arrEnd;
    FindBalancedBlock(sectionTemplate, arrOpen, '[', ']', arrStart, arrEnd);

    std::string result = sectionTemplate.substr(0, arrOpen + 1) + "\n";
    for (size_t i = 0; i < resultBlocks.size(); ++i) {
        result += resultBlocks[i];
        if (i + 1 < resultBlocks.size()) {
            result += ",\n";
        } else {
            result += "\n";
        }
    }
    size_t closingLineStart = LineStart(sectionTemplate, arrEnd - 1);
    result += sectionTemplate.substr(closingLineStart);
    return result;
}

std::string BuildReplacementBlock(const ParsedAnimFile& source, const ParsedAnimFile& target) {
    std::string result = source.raw;

    ReplaceNameInBlock(result, target.name);

    std::string mergedChildren = BuildMergedChildrenSection(source.childrenSection, target.childrenSection);
    if (mergedChildren.empty()) {
        RemoveChildrenSection(result);
    } else if (HasChildrenSection(result)) {
        ReplaceChildrenSection(result, mergedChildren);
    } else {
        InsertChildrenSection(result, mergedChildren);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Merge Meshes
// ---------------------------------------------------------------------------

bool IsIdentChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

// Finds a key at the top nesting level of [begin, end) and returns the span of
// its value (quoted string including quotes, balanced { } / [ ] block, or a
// scalar up to end-of-line). Ranges must be "inner" ranges so that top-level
// keys sit at relative depth 0.
bool FindTopLevelKeyValue(const std::string& text, size_t begin, size_t end, const char* key,
                          size_t& valueStart, size_t& valueEnd) {
    size_t keyLen = std::strlen(key);
    int depth = 0;
    bool inString = false;
    size_t i = begin;
    while (i < end) {
        char c = text[i];
        if (inString) {
            if (c == '\\' && i + 1 < end) ++i;
            else if (c == '"') inString = false;
            ++i;
            continue;
        }
        if (c == '"') { inString = true; ++i; continue; }
        if (c == '{' || c == '[') { ++depth; ++i; continue; }
        if (c == '}' || c == ']') { --depth; ++i; continue; }
        if (depth == 0 && (i == begin || !IsIdentChar(text[i - 1])) &&
            i + keyLen <= end && text.compare(i, keyLen, key) == 0 &&
            (i + keyLen >= end || !IsIdentChar(text[i + keyLen]))) {
            size_t j = i + keyLen;
            while (j < end && (text[j] == ' ' || text[j] == '\t')) ++j;
            if (j >= end || text[j] != '=') { ++i; continue; }
            ++j;
            while (j < end && (text[j] == ' ' || text[j] == '\t' || text[j] == '\n' || text[j] == '\r')) ++j;
            if (j >= end) return false;
            if (text[j] == '"') {
                size_t q = j + 1;
                while (q < end) {
                    if (text[q] == '\\' && q + 1 < end) { q += 2; continue; }
                    if (text[q] == '"') break;
                    ++q;
                }
                if (q >= end) return false;
                valueStart = j;
                valueEnd = q + 1;
                return true;
            }
            if (text[j] == '{' || text[j] == '[') {
                size_t bStart, bEnd;
                char open = text[j];
                char close = (open == '{') ? '}' : ']';
                if (!FindBalancedBlock(text, j, open, close, bStart, bEnd) || bEnd > end) return false;
                valueStart = bStart;
                valueEnd = bEnd;
                return true;
            }
            size_t v = j;
            while (v < end && text[v] != '\n' && text[v] != '\r') ++v;
            size_t vend = v;
            while (vend > j && (text[vend - 1] == ' ' || text[vend - 1] == '\t' || text[vend - 1] == ',')) --vend;
            valueStart = j;
            valueEnd = vend;
            return true;
        }
        ++i;
    }
    return false;
}

std::string Unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::vector<std::pair<size_t, size_t>> SplitTopLevelBlockSpans(const std::string& text, size_t begin, size_t end) {
    std::vector<std::pair<size_t, size_t>> out;
    int depth = 0;
    bool inString = false;
    size_t blockStart = std::string::npos;
    for (size_t i = begin; i < end; ++i) {
        char c = text[i];
        if (inString) {
            if (c == '\\' && i + 1 < end) ++i;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') { inString = true; continue; }
        if (c == '{') {
            if (depth == 0) blockStart = i;
            ++depth;
        }
        else if (c == '}') {
            --depth;
            if (depth == 0 && blockStart != std::string::npos) {
                out.push_back({ blockStart, i + 1 });
                blockStart = std::string::npos;
            }
        }
    }
    return out;
}

std::string LineIndent(const std::string& text, size_t pos) {
    size_t ls = LineStart(text, pos);
    size_t i = ls;
    while (i < text.size() && (text[i] == '\t' || text[i] == ' ')) ++i;
    return text.substr(ls, i - ls);
}

std::string DetectIndentUnit(const std::string& text) {
    size_t ri = text.find("rootNode");
    if (ri != std::string::npos) {
        std::string ind = LineIndent(text, ri);
        if (!ind.empty()) return ind;
    }
    return "\t";
}

int IndentDepth(const std::string& indent, const std::string& unit) {
    if (unit.empty()) return 0;
    int depth = 0;
    size_t pos = 0;
    while (pos + unit.size() <= indent.size() && indent.compare(pos, unit.size(), unit) == 0) {
        ++depth;
        pos += unit.size();
    }
    if (pos < indent.size()) ++depth; // partial/space indent counts as one level
    return depth;
}

std::string RepeatIndent(const std::string& unit, int depth) {
    std::string s;
    for (int i = 0; i < depth; ++i) s += unit;
    return s;
}

struct SectionInfo {
    bool found = false;
    size_t open = 0;
    size_t close = 0; // index of closing '}'
    bool hasChildren = false;
    size_t childrenOpen = 0;  // index of '['
    size_t childrenClose = 0; // index of ']'
    std::vector<std::pair<size_t, size_t>> items; // [open, afterClose) of each child block
};

bool FindRootChildrenArray(const std::string& text, size_t& arrOpen, size_t& arrClose) {
    // Note: the kv3 header comment contains GUID braces, so anchor on "rootNode".
    size_t rn = text.find("rootNode");
    if (rn == std::string::npos) return false;
    size_t brace = text.find('{', rn);
    if (brace == std::string::npos) return false;
    size_t bs, be;
    if (!FindBalancedBlock(text, brace, '{', '}', bs, be)) return false;
    size_t cs, ce;
    if (!FindTopLevelKeyValue(text, bs + 1, be - 1, "children", cs, ce)) return false;
    if (cs >= text.size() || text[cs] != '[') return false;
    arrOpen = cs;
    arrClose = ce - 1;
    return true;
}

bool FindSection(const std::string& text, const char* className, SectionInfo& out) {
    size_t arrOpen, arrClose;
    if (!FindRootChildrenArray(text, arrOpen, arrClose)) return false;
    auto spans = SplitTopLevelBlockSpans(text, arrOpen + 1, arrClose);
    for (const auto& sp : spans) {
        size_t vs, ve;
        if (!FindTopLevelKeyValue(text, sp.first + 1, sp.second - 1, "_class", vs, ve)) continue;
        if (Unquote(text.substr(vs, ve - vs)) != className) continue;
        out.found = true;
        out.open = sp.first;
        out.close = sp.second - 1;
        size_t cs, ce;
        if (FindTopLevelKeyValue(text, sp.first + 1, sp.second - 1, "children", cs, ce) &&
            cs < text.size() && text[cs] == '[') {
            out.hasChildren = true;
            out.childrenOpen = cs;
            out.childrenClose = ce - 1;
            out.items = SplitTopLevelBlockSpans(text, cs + 1, ce - 1);
        }
        return true;
    }
    return false;
}

bool ReplaceValueInBlock(std::string& block, const char* key, const std::string& newValue) {
    size_t open = block.find('{');
    if (open == std::string::npos) return false;
    size_t close = block.rfind('}');
    if (close == std::string::npos || close <= open) return false;
    size_t vs, ve;
    if (!FindTopLevelKeyValue(block, open + 1, close, key, vs, ve)) return false;
    block.replace(vs, ve - vs, newValue);
    return true;
}

struct SourceBone {
    std::string name;
    size_t open = 0;
    size_t close = 0; // index of '}'
    bool hasChildren = false;
    size_t childrenOpen = 0;
    size_t childrenClose = 0; // index of ']'
    std::vector<SourceBone> children;
};

SourceBone ParseSourceBone(const std::string& text, size_t open, size_t close) {
    SourceBone bone;
    bone.open = open;
    bone.close = close;
    size_t vs, ve;
    if (FindTopLevelKeyValue(text, open + 1, close, "name", vs, ve)) {
        bone.name = Unquote(text.substr(vs, ve - vs));
    }
    size_t cs, ce;
    if (FindTopLevelKeyValue(text, open + 1, close, "children", cs, ce) &&
        cs < text.size() && text[cs] == '[') {
        bone.hasChildren = true;
        bone.childrenOpen = cs;
        bone.childrenClose = ce - 1;
        for (const auto& sp : SplitTopLevelBlockSpans(text, cs + 1, ce - 1)) {
            bone.children.push_back(ParseSourceBone(text, sp.first, sp.second - 1));
        }
    }
    return bone;
}

void CollectSourceBones(const SourceBone& bone, std::unordered_map<std::string, const SourceBone*>& out) {
    if (!bone.name.empty()) out[bone.name] = &bone;
    for (const auto& child : bone.children) CollectSourceBones(child, out);
}

struct TargetBone {
    std::string name;
    std::string origin;
    std::string angles;
    std::string doNotDiscard;
    std::vector<TargetBone> children;
};

TargetBone ParseTargetBone(const std::string& text, size_t open, size_t close) {
    TargetBone bone;
    size_t vs, ve;
    if (FindTopLevelKeyValue(text, open + 1, close, "name", vs, ve)) {
        bone.name = Unquote(text.substr(vs, ve - vs));
    }
    if (FindTopLevelKeyValue(text, open + 1, close, "origin", vs, ve)) {
        bone.origin = text.substr(vs, ve - vs);
    }
    if (FindTopLevelKeyValue(text, open + 1, close, "angles", vs, ve)) {
        bone.angles = text.substr(vs, ve - vs);
    }
    if (FindTopLevelKeyValue(text, open + 1, close, "do_not_discard", vs, ve)) {
        bone.doNotDiscard = text.substr(vs, ve - vs);
    }
    size_t cs, ce;
    if (FindTopLevelKeyValue(text, open + 1, close, "children", cs, ce) &&
        cs < text.size() && text[cs] == '[') {
        for (const auto& sp : SplitTopLevelBlockSpans(text, cs + 1, ce - 1)) {
            bone.children.push_back(ParseTargetBone(text, sp.first, sp.second - 1));
        }
    }
    return bone;
}

int CountTargetBones(const TargetBone& bone) {
    int count = 1;
    for (const auto& child : bone.children) count += CountTargetBones(child);
    return count;
}

struct AttachmentData {
    std::string name;
    std::string parentBone;
    std::string relativeOrigin;
    std::string relativeAngles;
    std::string weight;
    std::string ignoreRotation;
};

AttachmentData ParseAttachment(const std::string& text, size_t open, size_t close) {
    AttachmentData data;
    size_t vs, ve;
    if (FindTopLevelKeyValue(text, open + 1, close, "name", vs, ve)) {
        data.name = Unquote(text.substr(vs, ve - vs));
    }
    if (FindTopLevelKeyValue(text, open + 1, close, "parent_bone", vs, ve)) {
        data.parentBone = Unquote(text.substr(vs, ve - vs));
    }
    if (FindTopLevelKeyValue(text, open + 1, close, "relative_origin", vs, ve)) {
        data.relativeOrigin = text.substr(vs, ve - vs);
    }
    if (FindTopLevelKeyValue(text, open + 1, close, "relative_angles", vs, ve)) {
        data.relativeAngles = text.substr(vs, ve - vs);
    }
    if (FindTopLevelKeyValue(text, open + 1, close, "weight", vs, ve)) {
        data.weight = text.substr(vs, ve - vs);
    }
    if (FindTopLevelKeyValue(text, open + 1, close, "ignore_rotation", vs, ve)) {
        data.ignoreRotation = text.substr(vs, ve - vs);
    }
    return data;
}

std::string EmitTargetBone(const TargetBone& bone, const std::string& unit, int depth) {
    std::string ind = RepeatIndent(unit, depth);
    std::string ind1 = RepeatIndent(unit, depth + 1);
    std::string out;
    out += ind + "{\n";
    out += ind1 + "_class = \"Bone\"\n";
    out += ind1 + "name = \"" + bone.name + "\"\n";
    if (!bone.children.empty()) {
        out += ind1 + "children = \n";
        out += ind1 + "[\n";
        for (const auto& child : bone.children) {
            out += EmitTargetBone(child, unit, depth + 2);
            out += ",\n";
        }
        out += ind1 + "]\n";
    }
    out += ind1 + "origin = " + (bone.origin.empty() ? std::string("[ 0.0, 0.0, 0.0 ]") : bone.origin) + "\n";
    out += ind1 + "angles = " + (bone.angles.empty() ? std::string("[ 0.0, 0.0, 0.0 ]") : bone.angles) + "\n";
    out += ind1 + "do_not_discard = " + (bone.doNotDiscard.empty() ? std::string("true") : bone.doNotDiscard) + "\n";
    out += ind + "}";
    return out;
}

std::string EmitAttachment(const AttachmentData& data, const std::string& unit, int depth) {
    std::string ind = RepeatIndent(unit, depth);
    std::string ind1 = RepeatIndent(unit, depth + 1);
    std::string out;
    out += ind + "{\n";
    out += ind1 + "_class = \"Attachment\"\n";
    out += ind1 + "name = \"" + data.name + "\"\n";
    out += ind1 + "parent_bone = \"" + data.parentBone + "\"\n";
    out += ind1 + "relative_origin = " + (data.relativeOrigin.empty() ? std::string("[ 0.0, 0.0, 0.0 ]") : data.relativeOrigin) + "\n";
    out += ind1 + "relative_angles = " + (data.relativeAngles.empty() ? std::string("[ 0.0, 0.0, 0.0 ]") : data.relativeAngles) + "\n";
    out += ind1 + "weight = " + (data.weight.empty() ? std::string("1.0") : data.weight) + "\n";
    out += ind1 + "ignore_rotation = " + (data.ignoreRotation.empty() ? std::string("false") : data.ignoreRotation) + "\n";
    out += ind + "}";
    return out;
}

struct TextEdit {
    size_t pos = 0;
    size_t removeLen = 0;
    std::string insert;
};

// Appends fully-formatted items (no trailing comma) into an existing [ ] array.
void AppendItemsToArray(const std::string& text, size_t arrOpen, size_t arrClose,
                        const std::vector<std::string>& items, std::vector<TextEdit>& edits) {
    if (items.empty()) return;
    std::string payload;
    for (const auto& item : items) payload += item + ",\n";

    bool empty = true;
    for (size_t i = arrOpen + 1; i < arrClose; ++i) {
        if (text[i] != ' ' && text[i] != '\t' && text[i] != '\n' && text[i] != '\r') { empty = false; break; }
    }
    if (empty) {
        std::string lineIndent = LineIndent(text, arrOpen);
        edits.push_back({ arrOpen, arrClose - arrOpen + 1, "[\n" + payload + lineIndent + "]" });
    }
    else {
        edits.push_back({ LineStart(text, arrClose), 0, payload });
    }
}

std::string ToLowerAscii(std::string s) {
    for (auto& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Derives "<content root>" and "models/..." game directory from a .vmdl disk path.
bool DeriveContentRootAndGameDir(const std::string& vmdlPath, fs::path& contentRoot, std::string& gameDir) {
    std::error_code ec;
    fs::path abs = fs::absolute(fs::path(vmdlPath), ec);
    if (ec) abs = fs::path(vmdlPath);
    std::string p = abs.lexically_normal().string();
    std::string low = ToLowerAscii(p);
    size_t pos = std::string::npos;
    for (const char* pat : { "\\models\\", "/models/" }) {
        size_t q = low.rfind(pat);
        if (q != std::string::npos && (pos == std::string::npos || q > pos)) pos = q;
    }
    if (pos == std::string::npos) return false;
    contentRoot = fs::path(p.substr(0, pos));
    size_t dirStart = pos + 1; // skip the separator before "models"
    size_t lastSep = p.find_last_of("\\/");
    if (lastSep == std::string::npos || lastSep <= dirStart) return false;
    gameDir = p.substr(dirStart, lastSep - dirStart);
    std::replace(gameDir.begin(), gameDir.end(), '\\', '/');
    return !gameDir.empty();
}

fs::path LocateTargetDmx(const std::string& ref, const fs::path& targetVmdl, const fs::path& searchRoot) {
    std::error_code ec;
    std::string fileName = fs::path(ref).filename().string();
    std::vector<fs::path> candidates = {
        searchRoot / ref,
        targetVmdl.parent_path() / ref,
        targetVmdl.parent_path() / fileName,
        searchRoot / fileName
    };
    for (const auto& c : candidates) {
        if (fs::exists(c, ec) && fs::is_regular_file(c, ec)) return c.lexically_normal();
    }
    if (fs::exists(searchRoot, ec) && fs::is_directory(searchRoot, ec)) {
        std::string wanted = ToLowerAscii(fileName);
        for (const auto& entry : fs::recursive_directory_iterator(searchRoot, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            if (ToLowerAscii(entry.path().filename().string()) == wanted) return entry.path().lexically_normal();
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// Transfer Animations
// ---------------------------------------------------------------------------

constexpr const char* kMismatchSuffix = "_MISSMATCH";

struct QuotedToken {
    size_t start = 0; // index of opening quote
    size_t end = 0;   // index past closing quote
    std::string text; // unquoted content
};

std::vector<QuotedToken> ExtractQuotedTokens(const std::string& text, size_t begin, size_t end) {
    std::vector<QuotedToken> tokens;
    size_t i = begin;
    while (i < end) {
        if (text[i] != '"') { ++i; continue; }
        size_t q = i + 1;
        std::string content;
        bool closed = false;
        while (q < end) {
            if (text[q] == '\\' && q + 1 < end) { content += text[q]; content += text[q + 1]; q += 2; continue; }
            if (text[q] == '"') { closed = true; break; }
            content += text[q];
            ++q;
        }
        if (!closed) break;
        tokens.push_back({ i, q + 1, std::move(content) });
        i = q + 1;
    }
    return tokens;
}

std::string LeadingIndent(const std::string& block) {
    size_t i = 0;
    while (i < block.size() && (block[i] == '\t' || block[i] == ' ')) ++i;
    return block.substr(0, i);
}

std::string ReindentBlock(const std::string& block, const std::string& oldIndent, const std::string& newIndent) {
    if (oldIndent == newIndent || oldIndent.empty()) return block;
    std::string out;
    out.reserve(block.size());
    size_t pos = 0;
    while (pos < block.size()) {
        size_t lineEnd = block.find('\n', pos);
        if (lineEnd == std::string::npos) lineEnd = block.size();
        else ++lineEnd;
        size_t lineLen = lineEnd - pos;
        if (lineLen >= oldIndent.size() && block.compare(pos, oldIndent.size(), oldIndent) == 0) {
            out += newIndent;
            out.append(block, pos + oldIndent.size(), lineLen - oldIndent.size());
        }
        else {
            out.append(block, pos, lineLen);
        }
        pos = lineEnd;
    }
    return out;
}

bool CopyAnimFileToSourceDir(const fs::path& srcFile, const fs::path& destDir, const std::string& gameDir,
                             const std::function<void(const std::string&)>& logLine,
                             std::string& outRef, bool& outCopied) {
    std::error_code ec;
    outCopied = false;
    std::string fileName = srcFile.filename().string();
    fs::path dest = destDir / fileName;
    if (fs::exists(dest, ec)) {
        auto srcSize = fs::file_size(srcFile, ec);
        auto dstSize = fs::file_size(dest, ec);
        if (!ec && srcSize == dstSize) {
            logLine("Reusing existing .dmx: " + dest.string());
        }
        else {
            std::string stem = fs::path(fileName).stem().string();
            std::string ext = fs::path(fileName).extension().string();
            for (int i = 1; i < 100; ++i) {
                fs::path candidate = destDir / (stem + "_t" + std::to_string(i) + ext);
                if (!fs::exists(candidate, ec)) {
                    dest = candidate;
                    fileName = candidate.filename().string();
                    break;
                }
            }
            logLine("Destination .dmx exists with different content, using unique name: " + fileName);
        }
    }
    if (!fs::exists(dest, ec)) {
        fs::copy_file(srcFile, dest, ec);
        if (ec) {
            logLine("WARNING: failed to copy .dmx '" + srcFile.string() + "': " + ec.message());
            return false;
        }
        outCopied = true;
        logLine("Copied .dmx: " + srcFile.string() + " -> " + dest.string());
    }
    outRef = gameDir + "/" + fileName;
    return true;
}

// Rewrites a single animation-file reference (source_filename / additional_anim_files
// entry) inside a transferred block: locates the file in the target tree, copies it
// into the source game dir and replaces the quoted reference with the new game path.
void CopyAndRewriteRef(std::string& block, size_t tokenStart, size_t tokenEnd, const std::string& ref,
                       const fs::path& targetVmdl, const fs::path& searchRoot,
                       const fs::path& destDir, const std::string& gameDir,
                       const std::function<void(const std::string&)>& logLine,
                       int& filesCopied) {
    fs::path dmxSrc = LocateTargetDmx(ref, targetVmdl, searchRoot);
    if (dmxSrc.empty()) {
        logLine("WARNING: animation .dmx not found (" + ref + "), reference kept as-is.");
        return;
    }
    std::string newRef;
    bool copied = false;
    if (!CopyAnimFileToSourceDir(dmxSrc, destDir, gameDir, logLine, newRef, copied)) return;
    if (copied) ++filesCopied;
    block.replace(tokenStart, tokenEnd - tokenStart, "\"" + newRef + "\"");
}

// Returns true if the quoted token at tokenStart (index of the opening quote) is the
// value of a `name = "..."` key (exactly "name", not "activity_name"/"weight_list_name").
bool IsNameKeyValue(const std::string& text, size_t tokenStart, size_t scopeBegin) {
    size_t i = tokenStart;
    while (i > scopeBegin && (text[i - 1] == ' ' || text[i - 1] == '\t')) --i;
    if (i == scopeBegin || text[i - 1] != '=') return false;
    --i;
    while (i > scopeBegin && (text[i - 1] == ' ' || text[i - 1] == '\t')) --i;
    size_t idEnd = i;
    while (i > scopeBegin && IsIdentChar(text[i - 1])) --i;
    return text.compare(i, idEnd - i, "name") == 0;
}

// Returns true if the quoted token at tokenStart is a bare string array entry
// (preceded only by whitespace and '[' or ','), e.g. anim_order / blend_anim_list.
bool IsArrayStringEntry(const std::string& text, size_t tokenStart, size_t scopeBegin) {
    size_t i = tokenStart;
    while (i > scopeBegin && (text[i - 1] == ' ' || text[i - 1] == '\t' ||
                              text[i - 1] == '\n' || text[i - 1] == '\r')) --i;
    if (i == scopeBegin) return false;
    return text[i - 1] == ',' || text[i - 1] == '[';
}

// Renames every animation-name reference to oldName inside the AnimationList
// section: AnimFile `name` fields, AnimOrder entries, blend list entries.
// `activity_name` values (incl. ActivityModifier names) are intentionally left alone.
bool RenameAnimationInAnimationList(std::string& vmdlText, const std::string& oldName, const std::string& newName) {
    size_t animListPos = vmdlText.find("_class = \"AnimationList\"");
    if (animListPos == std::string::npos) return false;
    size_t listBrace = FindOpeningBraceBefore(vmdlText, animListPos);
    if (listBrace == std::string::npos) return false;
    size_t listStart, listEnd;
    if (!FindBalancedBlock(vmdlText, listBrace, '{', '}', listStart, listEnd)) return false;

    std::string section = vmdlText.substr(listStart, listEnd - listStart);
    std::string replacement = "\"" + newName + "\"";
    bool replaced = false;
    size_t i = 0;
    while (i < section.size()) {
        if (section[i] != '"') { ++i; continue; }
        size_t q = i + 1;
        bool closed = false;
        while (q < section.size()) {
            if (section[q] == '\\' && q + 1 < section.size()) { q += 2; continue; }
            if (section[q] == '"') { closed = true; break; }
            ++q;
        }
        if (!closed) break;
        size_t tokenLen = q - i - 1;
        if (tokenLen == oldName.size() && section.compare(i + 1, tokenLen, oldName) == 0 &&
            (IsNameKeyValue(section, i, 0) || IsArrayStringEntry(section, i, 0))) {
            section.replace(i, tokenLen + 2, replacement);
            i += replacement.size();
            replaced = true;
            continue;
        }
        i = q + 1;
    }
    if (!replaced) return false;
    vmdlText.replace(listStart, listEnd - listStart, section);
    return true;
}

} // namespace

namespace model_helper {

ActivityModifierResult ApplyActivityModifiers(const std::string& vmdlPath,
                                              const std::vector<std::string>& userModifiers) {
    ActivityModifierResult result;

    if (vmdlPath.empty()) {
        result.message = "VMDL path is empty.";
        return result;
    }

    std::vector<std::string> sortedUserModifiers = userModifiers;
    sortedUserModifiers.erase(
        std::remove_if(sortedUserModifiers.begin(), sortedUserModifiers.end(),
                       [](const std::string& s) { return s.empty(); }),
        sortedUserModifiers.end());
    if (sortedUserModifiers.empty()) {
        result.message = "No activity modifiers specified.";
        return result;
    }
    std::sort(sortedUserModifiers.begin(), sortedUserModifiers.end());

    std::string vmdlText = ReadFile(vmdlPath);
    if (vmdlText.empty()) {
        result.message = "Failed to read VMDL file: " + vmdlPath;
        return result;
    }

    std::vector<ParsedAnimFile> animations = ExtractAnimFiles(vmdlText);
    if (animations.empty()) {
        result.message = "No AnimFile blocks found in the VMDL.";
        return result;
    }

    using Key = std::pair<std::string, std::vector<std::string>>;
    std::unordered_map<std::string, size_t> targetMap;
    for (size_t i = 0; i < animations.size(); ++i) {
        std::string key = animations[i].activityName + "\0";
        for (const auto& m : animations[i].modifiers) {
            key += m + "\0";
        }
        auto [it, inserted] = targetMap.emplace(key, i);
        (void)it;
        (void)inserted;
    }

    std::vector<std::pair<size_t, size_t>> sourceToTarget;
    std::unordered_set<size_t> usedTargets;

    for (size_t i = 0; i < animations.size(); ++i) {
        const ParsedAnimFile& source = animations[i];
        if (!IsSubset(sortedUserModifiers, source.modifiers)) continue;

        std::vector<std::string> remaining = SubtractModifiers(source.modifiers, sortedUserModifiers);
        std::string key = source.activityName + "\0";
        for (const auto& m : remaining) {
            key += m + "\0";
        }

        auto it = targetMap.find(key);
        if (it == targetMap.end()) continue;
        size_t targetIndex = it->second;
        if (targetIndex == i) continue;
        if (usedTargets.count(targetIndex)) continue;

        usedTargets.insert(targetIndex);
        sourceToTarget.emplace_back(i, targetIndex);
    }

    if (sourceToTarget.empty()) {
        result.message = "No matching source/target animation pairs found.";
        return result;
    }

    std::sort(sourceToTarget.begin(), sourceToTarget.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& [sourceIndex, targetIndex] : sourceToTarget) {
        const ParsedAnimFile& source = animations[sourceIndex];
        const ParsedAnimFile& target = animations[targetIndex];

        std::string newBlock = BuildReplacementBlock(source, target);
        vmdlText.replace(target.startPos, target.endPos - target.startPos, newBlock);
    }

    if (!WriteFile(vmdlPath, vmdlText)) {
        result.message = "Failed to write modified VMDL file: " + vmdlPath;
        return result;
    }

    result.success = true;
    result.matchedSourceAnimations = static_cast<int>(sourceToTarget.size());
    result.replacedTargetAnimations = static_cast<int>(sourceToTarget.size());
    result.message = "Applied activity modifiers to " + std::to_string(result.replacedTargetAnimations) + " animation(s).";
    return result;
}

MergeMeshesResult MergeMeshes(const MergeMeshesOptions& options) {
    MergeMeshesResult result;
    std::ostringstream log;
    auto logLine = [&](const std::string& s) { log << s << "\n"; };

    if (options.sourceVmdlPath.empty() || options.targetVmdlPath.empty()) {
        result.message = "Source or target VMDL path is empty.";
        return result;
    }
    std::error_code ec;
    fs::path sourcePath = fs::path(options.sourceVmdlPath).lexically_normal();
    fs::path targetPath = fs::path(options.targetVmdlPath).lexically_normal();
    if (!fs::exists(sourcePath, ec)) {
        result.message = "Source VMDL not found: " + options.sourceVmdlPath;
        return result;
    }
    if (!fs::exists(targetPath, ec)) {
        result.message = "Target VMDL not found: " + options.targetVmdlPath;
        return result;
    }
    {
        std::string a = ToLowerAscii(fs::absolute(sourcePath, ec).string());
        std::string b = ToLowerAscii(fs::absolute(targetPath, ec).string());
        if (a == b) {
            result.message = "Source and target are the same file.";
            return result;
        }
    }

    fs::path contentRoot;
    std::string gameDir;
    if (!DeriveContentRootAndGameDir(options.sourceVmdlPath, contentRoot, gameDir)) {
        result.message = "Cannot derive content root from the source path (expected a path containing a \"models\" folder).";
        return result;
    }
    logLine("Source content root: " + contentRoot.string());
    logLine("Source game dir: " + gameDir);

    fs::path searchRoot = options.targetSearchRoot.empty() ? targetPath.parent_path() : fs::path(options.targetSearchRoot);
    logLine("Target search root: " + searchRoot.string());

    std::string sourceText = ReadFile(sourcePath.string());
    if (sourceText.empty()) {
        result.message = "Failed to read source VMDL: " + sourcePath.string();
        return result;
    }
    std::string targetText = ReadFile(targetPath.string());
    if (targetText.empty()) {
        result.message = "Failed to read target VMDL: " + targetPath.string();
        return result;
    }

    const std::string unit = DetectIndentUnit(sourceText);

    // ---- Parse target render meshes -------------------------------------
    struct TargetMesh {
        std::string name;
        std::string filename;
    };
    std::vector<TargetMesh> targetMeshes;
    SectionInfo targetMeshList;
    if (FindSection(targetText, "RenderMeshList", targetMeshList)) {
        for (const auto& sp : targetMeshList.items) {
            TargetMesh mesh;
            size_t vs, ve;
            if (FindTopLevelKeyValue(targetText, sp.first + 1, sp.second - 1, "name", vs, ve)) {
                mesh.name = Unquote(targetText.substr(vs, ve - vs));
            }
            if (FindTopLevelKeyValue(targetText, sp.first + 1, sp.second - 1, "filename", vs, ve)) {
                mesh.filename = Unquote(targetText.substr(vs, ve - vs));
            }
            if (!mesh.filename.empty()) {
                if (mesh.name.empty()) mesh.name = fs::path(mesh.filename).stem().string();
                targetMeshes.push_back(std::move(mesh));
            }
        }
    }
    if (targetMeshes.empty()) {
        result.message = "Target model has no render meshes.";
        return result;
    }
    logLine("Target render meshes: " + std::to_string(targetMeshes.size()));

    // ---- Parse target LOD groups (mesh_name -> lod indices) --------------
    bool targetHasLodList = false;
    std::unordered_map<std::string, std::vector<int>> targetMeshLods;
    SectionInfo targetLodList;
    if (FindSection(targetText, "LODGroupList", targetLodList)) {
        targetHasLodList = true;
        for (size_t i = 0; i < targetLodList.items.size(); ++i) {
            const auto& sp = targetLodList.items[i];
            size_t vs, ve;
            if (!FindTopLevelKeyValue(targetText, sp.first + 1, sp.second - 1, "mesh_references", vs, ve)) continue;
            if (vs >= targetText.size() || targetText[vs] != '[') continue;
            for (const auto& refSpan : SplitTopLevelBlockSpans(targetText, vs + 1, ve - 1)) {
                size_t nvs, nve;
                if (FindTopLevelKeyValue(targetText, refSpan.first + 1, refSpan.second - 1, "mesh_name", nvs, nve)) {
                    targetMeshLods[Unquote(targetText.substr(nvs, nve - nvs))].push_back(static_cast<int>(i));
                }
            }
        }
    }
    logLine(targetHasLodList ? "Target has LOD groups." : "Target has no LOD groups (meshes go to all source LODs).");

    // ---- Parse source sections -------------------------------------------
    SectionInfo sourceMeshList;
    FindSection(sourceText, "RenderMeshList", sourceMeshList);
    std::unordered_set<std::string> sourceMeshNames;
    if (sourceMeshList.found) {
        for (const auto& sp : sourceMeshList.items) {
            size_t vs, ve;
            if (FindTopLevelKeyValue(sourceText, sp.first + 1, sp.second - 1, "name", vs, ve)) {
                sourceMeshNames.insert(Unquote(sourceText.substr(vs, ve - vs)));
            }
        }
    }

    SectionInfo sourceLodList;
    FindSection(sourceText, "LODGroupList", sourceLodList);
    struct SourceLodGroup {
        size_t meshRefsOpen = 0;
        size_t meshRefsClose = 0;
        std::string firstEntryBlock;
        std::unordered_set<std::string> meshNames;
    };
    std::vector<SourceLodGroup> sourceLods;
    if (sourceLodList.found) {
        for (const auto& sp : sourceLodList.items) {
            size_t vs, ve;
            if (!FindTopLevelKeyValue(sourceText, sp.first + 1, sp.second - 1, "mesh_references", vs, ve)) continue;
            if (vs >= sourceText.size() || sourceText[vs] != '[') continue;
            SourceLodGroup group;
            group.meshRefsOpen = vs;
            group.meshRefsClose = ve - 1;
            auto refSpans = SplitTopLevelBlockSpans(sourceText, vs + 1, ve - 1);
            for (size_t ri = 0; ri < refSpans.size(); ++ri) {
                if (ri == 0) group.firstEntryBlock = LineIndent(sourceText, refSpans[ri].first) +
                    sourceText.substr(refSpans[ri].first, refSpans[ri].second - refSpans[ri].first);
                size_t nvs, nve;
                if (FindTopLevelKeyValue(sourceText, refSpans[ri].first + 1, refSpans[ri].second - 1, "mesh_name", nvs, nve)) {
                    group.meshNames.insert(Unquote(sourceText.substr(nvs, nve - nvs)));
                }
            }
            sourceLods.push_back(std::move(group));
        }
    }

    SectionInfo sourceAttachmentList;
    FindSection(sourceText, "AttachmentList", sourceAttachmentList);
    std::unordered_set<std::string> sourceAttachmentNames;
    if (sourceAttachmentList.found) {
        for (const auto& sp : sourceAttachmentList.items) {
            size_t vs, ve;
            if (FindTopLevelKeyValue(sourceText, sp.first + 1, sp.second - 1, "name", vs, ve)) {
                sourceAttachmentNames.insert(Unquote(sourceText.substr(vs, ve - vs)));
            }
        }
    }

    // ---- Copy .dmx meshes and build the added-mesh list ------------------
    struct AddedMesh {
        std::string name;
        std::string newRef;
    };
    std::vector<AddedMesh> addedMeshes;
    fs::path destDir = contentRoot / gameDir;
    fs::create_directories(destDir, ec);
    for (const auto& mesh : targetMeshes) {
        if (sourceMeshNames.count(mesh.name)) {
            logLine("Mesh '" + mesh.name + "' already exists in source, skipped.");
            continue;
        }
        fs::path dmxSrc = LocateTargetDmx(mesh.filename, targetPath, searchRoot);
        if (dmxSrc.empty()) {
            logLine("WARNING: .dmx not found for mesh '" + mesh.name + "' (" + mesh.filename + "), mesh skipped.");
            continue;
        }
        std::string fileName = dmxSrc.filename().string();
        fs::path dest = destDir / fileName;
        if (fs::exists(dest, ec)) {
            auto srcSize = fs::file_size(dmxSrc, ec);
            auto dstSize = fs::file_size(dest, ec);
            if (!ec && srcSize == dstSize) {
                logLine("Reusing existing .dmx: " + dest.string());
            }
            else {
                std::string stem = fs::path(fileName).stem().string();
                std::string ext = fs::path(fileName).extension().string();
                for (int i = 1; i < 100; ++i) {
                    fs::path candidate = destDir / (stem + "_m" + std::to_string(i) + ext);
                    if (!fs::exists(candidate, ec)) {
                        dest = candidate;
                        fileName = candidate.filename().string();
                        break;
                    }
                }
                logLine("Destination .dmx exists with different content, using unique name: " + fileName);
            }
        }
        if (!fs::exists(dest, ec)) {
            fs::copy_file(dmxSrc, dest, ec);
            if (ec) {
                logLine("WARNING: failed to copy .dmx for mesh '" + mesh.name + "': " + ec.message());
                continue;
            }
            logLine("Copied .dmx: " + dmxSrc.string() + " -> " + dest.string());
        }
        addedMeshes.push_back({ mesh.name, gameDir + "/" + fileName });
        sourceMeshNames.insert(mesh.name);
    }
    if (addedMeshes.empty()) {
        result.message = "No new meshes to merge (all target meshes are already present or their .dmx files are missing).";
        result.log = log.str();
        return result;
    }
    result.meshesAdded = static_cast<int>(addedMeshes.size());

    // ---- Build text edits -------------------------------------------------
    std::vector<TextEdit> edits;

    // Root children array (needed when sections must be created).
    size_t rootArrOpen = 0, rootArrClose = 0;
    bool haveRoot = FindRootChildrenArray(sourceText, rootArrOpen, rootArrClose);
    int rootChildDepth = 2;
    if (haveRoot) {
        auto rootItems = SplitTopLevelBlockSpans(sourceText, rootArrOpen + 1, rootArrClose);
        if (!rootItems.empty()) {
            rootChildDepth = IndentDepth(LineIndent(sourceText, rootItems.front().first), unit);
        }
        else {
            rootChildDepth = IndentDepth(LineIndent(sourceText, rootArrOpen), unit) + 1;
        }
    }

    // 1) RenderMeshList
    {
        std::vector<std::string> newBlocks;
        for (const auto& mesh : addedMeshes) {
            if (sourceMeshList.found && !sourceMeshList.items.empty()) {
                std::string block = LineIndent(sourceText, sourceMeshList.items.front().first) +
                    sourceText.substr(sourceMeshList.items.front().first,
                    sourceMeshList.items.front().second - sourceMeshList.items.front().first);
                ReplaceValueInBlock(block, "name", "\"" + mesh.name + "\"");
                ReplaceValueInBlock(block, "filename", "\"" + mesh.newRef + "\"");
                newBlocks.push_back(std::move(block));
            }
            else {
                int depth = rootChildDepth + 1;
                std::string ind = RepeatIndent(unit, depth);
                std::string ind1 = RepeatIndent(unit, depth + 1);
                std::string block;
                block += ind + "{\n";
                block += ind1 + "_class = \"RenderMeshFile\"\n";
                block += ind1 + "name = \"" + mesh.name + "\"\n";
                block += ind1 + "filename = \"" + mesh.newRef + "\"\n";
                block += ind + "}";
                newBlocks.push_back(std::move(block));
            }
        }
        if (sourceMeshList.found && sourceMeshList.hasChildren) {
            AppendItemsToArray(sourceText, sourceMeshList.childrenOpen, sourceMeshList.childrenClose, newBlocks, edits);
        }
        else if (haveRoot) {
            std::string ind = RepeatIndent(unit, rootChildDepth);
            std::string ind1 = RepeatIndent(unit, rootChildDepth + 1);
            std::string section;
            section += ind + "{\n";
            section += ind1 + "_class = \"RenderMeshList\"\n";
            section += ind1 + "children = \n";
            section += ind1 + "[\n";
            for (const auto& block : newBlocks) section += block + ",\n";
            section += ind1 + "]\n";
            section += ind + "}";
            AppendItemsToArray(sourceText, rootArrOpen, rootArrClose, { section }, edits);
            logLine("Source had no RenderMeshList, created one.");
        }
    }

    // 2) LOD groups
    if (!sourceLods.empty()) {
        std::vector<std::vector<std::string>> perLodItems(sourceLods.size());
        for (const auto& mesh : addedMeshes) {
            std::vector<int> lodIndices;
            if (targetHasLodList) {
                auto it = targetMeshLods.find(mesh.name);
                if (it != targetMeshLods.end() && !it->second.empty()) lodIndices = it->second;
                else lodIndices.push_back(0);
            }
            else {
                for (size_t i = 0; i < sourceLods.size(); ++i) lodIndices.push_back(static_cast<int>(i));
            }
            for (int idx : lodIndices) {
                size_t lod = static_cast<size_t>(idx);
                if (lod >= sourceLods.size()) lod = sourceLods.size() - 1;
                SourceLodGroup& group = sourceLods[lod];
                if (group.meshNames.count(mesh.name)) continue;
                group.meshNames.insert(mesh.name);
                std::string entry;
                if (!group.firstEntryBlock.empty()) {
                    entry = group.firstEntryBlock;
                    ReplaceValueInBlock(entry, "mesh_name", "\"" + mesh.name + "\"");
                }
                else {
                    std::string ind = LineIndent(sourceText, group.meshRefsOpen) + unit;
                    entry = ind + "{\n" + ind + unit + "mesh_name = \"" + mesh.name + "\"\n" + ind + "}";
                }
                perLodItems[lod].push_back(std::move(entry));
                ++result.lodReferencesAdded;
            }
        }
        for (size_t i = 0; i < sourceLods.size(); ++i) {
            AppendItemsToArray(sourceText, sourceLods[i].meshRefsOpen, sourceLods[i].meshRefsClose, perLodItems[i], edits);
        }
    }
    else {
        logLine("Source has no LOD groups, skipping LOD merge.");
    }

    // 3) Attachments
    {
        std::vector<AttachmentData> toAdd;
        SectionInfo targetAttachmentList;
        if (FindSection(targetText, "AttachmentList", targetAttachmentList)) {
            for (const auto& sp : targetAttachmentList.items) {
                AttachmentData data = ParseAttachment(targetText, sp.first, sp.second - 1);
                if (data.name.empty()) continue;
                if (sourceAttachmentNames.count(data.name)) {
                    logLine("Attachment '" + data.name + "' already exists in source, skipped.");
                    continue;
                }
                sourceAttachmentNames.insert(data.name);
                toAdd.push_back(std::move(data));
            }
        }
        if (!toAdd.empty()) {
            if (sourceAttachmentList.found && sourceAttachmentList.hasChildren) {
                int depth = IndentDepth(LineIndent(sourceText, sourceAttachmentList.childrenOpen), unit) + 1;
                std::vector<std::string> blocks;
                for (const auto& a : toAdd) blocks.push_back(EmitAttachment(a, unit, depth));
                AppendItemsToArray(sourceText, sourceAttachmentList.childrenOpen, sourceAttachmentList.childrenClose, blocks, edits);
            }
            else if (haveRoot) {
                std::string ind = RepeatIndent(unit, rootChildDepth);
                std::string ind1 = RepeatIndent(unit, rootChildDepth + 1);
                std::string section;
                section += ind + "{\n";
                section += ind1 + "_class = \"AttachmentList\"\n";
                section += ind1 + "children = \n";
                section += ind1 + "[\n";
                for (const auto& a : toAdd) section += EmitAttachment(a, unit, rootChildDepth + 2) + ",\n";
                section += ind1 + "]\n";
                section += ind + "}";
                AppendItemsToArray(sourceText, rootArrOpen, rootArrClose, { section }, edits);
                logLine("Source had no AttachmentList, created one.");
            }
            result.attachmentsAdded = static_cast<int>(toAdd.size());
        }
        else {
            logLine("No new attachments to merge.");
        }
    }

    // 4) Skeleton
    {
        std::vector<TargetBone> targetRoots;
        SectionInfo targetSkeleton;
        if (FindSection(targetText, "Skeleton", targetSkeleton)) {
            for (const auto& sp : targetSkeleton.items) {
                targetRoots.push_back(ParseTargetBone(targetText, sp.first, sp.second - 1));
            }
        }
        if (!targetRoots.empty()) {
            SectionInfo sourceSkeleton;
            FindSection(sourceText, "Skeleton", sourceSkeleton);
            std::vector<SourceBone> sourceRoots;
            std::unordered_map<std::string, const SourceBone*> sourceBoneMap;
            if (sourceSkeleton.found) {
                for (const auto& sp : sourceSkeleton.items) {
                    sourceRoots.push_back(ParseSourceBone(sourceText, sp.first, sp.second - 1));
                }
                for (const auto& root : sourceRoots) CollectSourceBones(root, sourceBoneMap);
            }

            if (!sourceSkeleton.found || sourceRoots.empty()) {
                // Insert the whole target skeleton forest.
                int boneDepth = rootChildDepth + 2;
                std::vector<std::string> items;
                for (const auto& root : targetRoots) {
                    items.push_back(EmitTargetBone(root, unit, boneDepth));
                    result.bonesAdded += CountTargetBones(root);
                }
                if (sourceSkeleton.found && sourceSkeleton.hasChildren) {
                    AppendItemsToArray(sourceText, sourceSkeleton.childrenOpen, sourceSkeleton.childrenClose, items, edits);
                }
                else if (haveRoot) {
                    std::string ind = RepeatIndent(unit, rootChildDepth);
                    std::string ind1 = RepeatIndent(unit, rootChildDepth + 1);
                    std::string section;
                    section += ind + "{\n";
                    section += ind1 + "_class = \"Skeleton\"\n";
                    section += ind1 + "children = \n";
                    section += ind1 + "[\n";
                    for (const auto& item : items) section += item + ",\n";
                    section += ind1 + "]\n";
                    section += ind + "}";
                    AppendItemsToArray(sourceText, rootArrOpen, rootArrClose, { section }, edits);
                    logLine("Source had no skeleton, inserted target skeleton.");
                }
            }
            else {
                // Graft target-only bones under the nearest matching source bone.
                struct Graft {
                    const SourceBone* under; // nullptr -> first source root
                    const TargetBone* bone;
                };
                std::vector<Graft> grafts;
                std::function<void(const TargetBone&, const SourceBone*)> planGrafts =
                    [&](const TargetBone& t, const SourceBone* matchedAncestor) {
                        auto it = sourceBoneMap.find(t.name);
                        if (it != sourceBoneMap.end()) {
                            for (const auto& child : t.children) planGrafts(child, it->second);
                        }
                        else {
                            grafts.push_back({ matchedAncestor, &t });
                        }
                    };
                for (const auto& root : targetRoots) planGrafts(root, nullptr);

                // Group grafts by destination bone.
                std::unordered_map<const SourceBone*, std::vector<const TargetBone*>> graftsByBone;
                for (const auto& g : grafts) {
                    const SourceBone* under = g.under ? g.under : &sourceRoots.front();
                    graftsByBone[under].push_back(g.bone);
                }
                for (const auto& [under, bones] : graftsByBone) {
                    int boneDepth = IndentDepth(LineIndent(sourceText, under->open), unit);
                    std::vector<std::string> items;
                    for (const auto* b : bones) {
                        items.push_back(EmitTargetBone(*b, unit, boneDepth + 2));
                        result.bonesAdded += CountTargetBones(*b);
                    }
                    if (under->hasChildren) {
                        AppendItemsToArray(sourceText, under->childrenOpen, under->childrenClose, items, edits);
                    }
                    else {
                        size_t vs, ve;
                        std::string payload;
                        if (FindTopLevelKeyValue(sourceText, under->open + 1, under->close, "name", vs, ve)) {
                            size_t lineEnd = sourceText.find('\n', ve);
                            if (lineEnd != std::string::npos) {
                                std::string ind1 = RepeatIndent(unit, boneDepth + 1);
                                payload += ind1 + "children = \n";
                                payload += ind1 + "[\n";
                                for (const auto& item : items) payload += item + ",\n";
                                payload += ind1 + "]\n";
                                edits.push_back({ lineEnd + 1, 0, payload });
                            }
                        }
                        if (payload.empty()) {
                            logLine("WARNING: failed to graft bones under '" + under->name + "'.");
                        }
                    }
                    logLine("Grafted " + std::to_string(bones.size()) + " bone subtree(s) under '" + under->name + "'.");
                }
                if (grafts.empty()) {
                    logLine("No new bones to merge.");
                }
            }
        }
        else {
            logLine("Target has no skeleton, skipping skeleton merge.");
        }
    }

    if (edits.empty()) {
        result.message = "Nothing to merge (no VMDL changes were produced).";
        result.log = log.str();
        return result;
    }

    // Apply edits back-to-front.
    std::sort(edits.begin(), edits.end(), [](const TextEdit& a, const TextEdit& b) { return a.pos > b.pos; });
    for (const auto& edit : edits) {
        sourceText.replace(edit.pos, edit.removeLen, edit.insert);
    }

    fs::path outputPath = options.outputVmdlPath.empty()
        ? (sourcePath.parent_path() / (sourcePath.stem().string() + "_merged.vmdl"))
        : fs::path(options.outputVmdlPath);
    if (!WriteFile(outputPath.string(), sourceText)) {
        result.message = "Failed to write merged VMDL: " + outputPath.string();
        result.log = log.str();
        return result;
    }
    logLine("Merged VMDL written to: " + outputPath.string());

    result.success = true;
    result.message = "Merged " + std::to_string(result.meshesAdded) + " mesh(es), " +
        std::to_string(result.attachmentsAdded) + " attachment(s), " +
        std::to_string(result.bonesAdded) + " bone(s) -> " + outputPath.string();
    result.log = log.str();
    return result;
}

TransferAnimationsResult TransferAnimations(const TransferAnimationsOptions& options) {
    TransferAnimationsResult result;
    std::ostringstream log;
    auto logLine = [&](const std::string& s) { log << s << "\n"; };

    if (options.sourceVmdlPath.empty() || options.targetVmdlPath.empty()) {
        result.message = "Source or target VMDL path is empty.";
        return result;
    }
    std::error_code ec;
    fs::path sourcePath = fs::path(options.sourceVmdlPath).lexically_normal();
    fs::path targetPath = fs::path(options.targetVmdlPath).lexically_normal();
    if (!fs::exists(sourcePath, ec)) {
        result.message = "Source VMDL not found: " + options.sourceVmdlPath;
        return result;
    }
    if (!fs::exists(targetPath, ec)) {
        result.message = "Target VMDL not found: " + options.targetVmdlPath;
        return result;
    }
    {
        std::string a = ToLowerAscii(fs::absolute(sourcePath, ec).string());
        std::string b = ToLowerAscii(fs::absolute(targetPath, ec).string());
        if (a == b) {
            result.message = "Source and target are the same file.";
            return result;
        }
    }

    fs::path contentRoot;
    std::string gameDir;
    if (!DeriveContentRootAndGameDir(options.sourceVmdlPath, contentRoot, gameDir)) {
        result.message = "Cannot derive content root from the source path (expected a path containing a \"models\" folder).";
        return result;
    }
    logLine("Source content root: " + contentRoot.string());
    logLine("Source game dir: " + gameDir);

    fs::path searchRoot = options.targetSearchRoot.empty() ? targetPath.parent_path() : fs::path(options.targetSearchRoot);
    logLine("Target search root: " + searchRoot.string());

    std::string sourceText = ReadFile(sourcePath.string());
    if (sourceText.empty()) {
        result.message = "Failed to read source VMDL: " + sourcePath.string();
        return result;
    }
    std::string targetText = ReadFile(targetPath.string());
    if (targetText.empty()) {
        result.message = "Failed to read target VMDL: " + targetPath.string();
        return result;
    }

    std::vector<ParsedAnimFile> sourceAnims = ExtractAnimFiles(sourceText);
    if (sourceAnims.empty()) {
        result.message = "No AnimFile blocks found in the source VMDL.";
        return result;
    }
    std::vector<ParsedAnimFile> targetAnims = ExtractAnimFiles(targetText);
    if (targetAnims.empty()) {
        result.message = "No AnimFile blocks found in the target VMDL.";
        return result;
    }
    logLine("Source animations: " + std::to_string(sourceAnims.size()) +
            ", target animations: " + std::to_string(targetAnims.size()));

    // key -> queue of target animation indices (duplicated keys are consumed in order)
    std::unordered_map<std::string, std::vector<size_t>> targetMap;
    for (size_t i = 0; i < targetAnims.size(); ++i) {
        if (targetAnims[i].activityName.empty()) continue;
        std::string key = targetAnims[i].activityName + "\0";
        for (const auto& m : targetAnims[i].modifiers) key += m + "\0";
        targetMap[key].push_back(i);
    }

    fs::path destDir = contentRoot / gameDir;
    fs::create_directories(destDir, ec);

    struct Replacement {
        size_t start = 0;
        size_t end = 0;
        std::string text;
    };
    std::vector<Replacement> replacements;
    std::vector<std::string> mismatchNames;

    const size_t suffixLen = std::strlen(kMismatchSuffix);

    for (const auto& src : sourceAnims) {
        if (src.activityName.empty()) {
            ++result.animationsSkipped;
            continue;
        }
        if (src.name.size() >= suffixLen &&
            src.name.compare(src.name.size() - suffixLen, suffixLen, kMismatchSuffix) == 0) {
            ++result.animationsSkipped;
            logLine("Animation '" + src.name + "' is already marked, skipped.");
            continue;
        }

        std::string key = src.activityName + "\0";
        for (const auto& m : src.modifiers) key += m + "\0";

        size_t targetIndex = std::string::npos;
        auto mapIt = targetMap.find(key);
        if (mapIt != targetMap.end() && !mapIt->second.empty()) {
            targetIndex = mapIt->second.front();
            mapIt->second.erase(mapIt->second.begin());
        }
        if (targetIndex == std::string::npos) {
            mismatchNames.push_back(src.name);
            ++result.animationsMismatched;
            std::string mods;
            for (const auto& m : src.modifiers) mods += (mods.empty() ? "" : ", ") + m;
            logLine("No mirror animation for '" + src.name + "' (activity '" + src.activityName +
                    "'" + (mods.empty() ? std::string() : ", modifiers: " + mods) +
                    "), renamed to '" + src.name + kMismatchSuffix + "'.");
            continue;
        }

        const ParsedAnimFile& tgt = targetAnims[targetIndex];

        std::string newBlock = ReindentBlock(tgt.raw, LeadingIndent(tgt.raw), LeadingIndent(src.raw));
        ReplaceNameInBlock(newBlock, src.name);

        // Copy referenced animation files into the source game dir and rewrite refs.
        size_t blockOpen = newBlock.find('{');
        size_t blockClose = newBlock.rfind('}');
        if (blockOpen != std::string::npos && blockClose != std::string::npos && blockClose > blockOpen) {
            size_t vs, ve;
            if (FindTopLevelKeyValue(newBlock, blockOpen + 1, blockClose, "source_filename", vs, ve) &&
                vs < newBlock.size() && newBlock[vs] == '"') {
                std::string ref = Unquote(newBlock.substr(vs, ve - vs));
                if (!ref.empty()) {
                    CopyAndRewriteRef(newBlock, vs, ve, ref, targetPath, searchRoot, destDir, gameDir,
                                      logLine, result.filesCopied);
                }
            }

            // Re-locate after the source_filename rewrite (spans may have shifted).
            blockOpen = newBlock.find('{');
            blockClose = newBlock.rfind('}');
            if (FindTopLevelKeyValue(newBlock, blockOpen + 1, blockClose, "additional_anim_files", vs, ve)) {
                std::vector<QuotedToken> tokens = ExtractQuotedTokens(newBlock, vs, ve);
                for (auto it = tokens.rbegin(); it != tokens.rend(); ++it) {
                    if (it->text.empty()) continue;
                    CopyAndRewriteRef(newBlock, it->start, it->end, it->text, targetPath, searchRoot,
                                      destDir, gameDir, logLine, result.filesCopied);
                }
            }
        }

        replacements.push_back({ src.startPos, src.endPos, std::move(newBlock) });
        ++result.animationsTransferred;
        logLine("Transferred animation '" + src.name + "' <- '" + tgt.name + "'.");
    }

    if (replacements.empty() && mismatchNames.empty()) {
        result.message = "Nothing to transfer (no transferable animations found).";
        result.log = log.str();
        return result;
    }

    std::sort(replacements.begin(), replacements.end(),
              [](const Replacement& a, const Replacement& b) { return a.start > b.start; });
    for (const auto& r : replacements) {
        sourceText.replace(r.start, r.end - r.start, r.text);
    }

    for (const auto& name : mismatchNames) {
        if (!RenameAnimationInAnimationList(sourceText, name, name + kMismatchSuffix)) {
            logLine("WARNING: failed to rename references for '" + name + "'.");
        }
    }

    if (!WriteFile(sourcePath.string(), sourceText)) {
        result.message = "Failed to write modified VMDL file: " + sourcePath.string();
        result.log = log.str();
        return result;
    }
    logLine("Modified VMDL written to: " + sourcePath.string());

    result.success = true;
    result.message = "Transferred " + std::to_string(result.animationsTransferred) + " animation(s), " +
        std::to_string(result.animationsMismatched) + " mismatch(es) renamed, " +
        std::to_string(result.filesCopied) + " file(s) copied.";
    result.log = log.str();
    return result;
}

} // namespace model_helper
