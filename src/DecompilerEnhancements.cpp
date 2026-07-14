#include "DecompilerEnhancements.h"
#include "SecurityHardening.h"
#include "parser/ModelDecompiler.h"
#include "parser/SkinDataManager.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file << data;
    return file.good();
}

std::string_view Trim(std::string_view s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

bool SkipWhitespace(const std::string& text, size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    return pos < text.size();
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

bool IsKV3Balanced(const std::string& text) {
    int depthBrace = 0;
    int depthBracket = 0;
    int depthParen = 0;
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
        if (depthBrace < 0 || depthBracket < 0 || depthParen < 0) {
            return false;
        }
    }
    return depthBrace == 0 && depthBracket == 0 && depthParen == 0;
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

std::string ExtractIndent(const std::string& text, size_t lineStart) {
    size_t i = lineStart;
    while (i < text.size() && (text[i] == '\t' || text[i] == ' ')) ++i;
    return text.substr(lineStart, i - lineStart);
}

size_t LineStart(const std::string& text, size_t pos) {
    if (pos >= text.size()) return 0;
    while (pos > 0 && text[pos - 1] != '\n') --pos;
    return pos;
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

std::vector<int> ParseIntArray(const std::string& text, size_t start, size_t end) {
    std::vector<int> result;
    size_t pos = start;
    while (pos < end) {
        while (pos < end && (std::isspace(static_cast<unsigned char>(text[pos])) || text[pos] == ',')) ++pos;
        if (pos >= end) break;
        if (text[pos] == ']' || text[pos] == '[' || text[pos] == '{') break;
        size_t numStart = pos;
        if (text[pos] == '-' || text[pos] == '+') ++pos;
        while (pos < end && std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
        if (numStart == pos) break;
        try {
            result.push_back(std::stoi(text.substr(numStart, pos - numStart)));
        }
        catch (...) {
            break;
        }
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
        }
        catch (...) {
            break;
        }
    }
    return result;
}

bool ExtractKeyBool(const std::string& text, size_t pos, const char* key, bool& outValue) {
    std::string keyEq = std::string(key) + " = ";
    size_t p = text.find(keyEq, pos);
    if (p == std::string::npos) {
        keyEq = std::string(key) + "=";
        p = text.find(keyEq, pos);
    }
    if (p == std::string::npos) return false;
    size_t valStart = p + keyEq.size();
    while (valStart < text.size() && std::isspace(static_cast<unsigned char>(text[valStart]))) ++valStart;
    if (valStart + 4 <= text.size() && text.compare(valStart, 4, "true") == 0) {
        outValue = true;
        return true;
    }
    if (valStart + 5 <= text.size() && text.compare(valStart, 5, "false") == 0) {
        outValue = false;
        return true;
    }
    return false;
}

std::string MaybeConvertUtf16ToUtf8(const std::string& input) {
    if (input.size() >= 2 &&
        static_cast<unsigned char>(input[0]) == 0xFF &&
        static_cast<unsigned char>(input[1]) == 0xFE) {
        std::string output;
        output.reserve(input.size() / 2);
        for (size_t i = 2; i + 1 < input.size(); i += 2) {
            wchar_t wc = static_cast<wchar_t>(static_cast<unsigned char>(input[i]) |
                (static_cast<unsigned char>(input[i + 1]) << 8));
            if (wc < 0x80) {
                output.push_back(static_cast<char>(wc));
            }
            else if (wc < 0x800) {
                output.push_back(static_cast<char>(0xC0 | (wc >> 6)));
                output.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
            }
            else {
                output.push_back(static_cast<char>(0xE0 | (wc >> 12)));
                output.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
            }
        }
        return output;
    }
    if (input.size() >= 3 &&
        static_cast<unsigned char>(input[0]) == 0xEF &&
        static_cast<unsigned char>(input[1]) == 0xBB &&
        static_cast<unsigned char>(input[2]) == 0xBF) {
        return input.substr(3);
    }
    return input;
}

std::string NormalizeAnimName(const std::string& name) {
    std::string base = fs::path(name).filename().string();
    static const std::vector<std::string> kAnimExts = {
        ".dmx", ".smd", ".fbx", ".obj", ".gltf", ".glb", ".ma", ".mb", ".max"
    };
    std::string lower = base;
    for (auto& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    for (const auto& ext : kAnimExts) {
        if (lower.size() > ext.size() && lower.compare(lower.size() - ext.size(), ext.size(), ext) == 0) {
            base.resize(base.size() - ext.size());
            break;
        }
    }
    return base;
}

std::string FormatKVFloat(float v) {
    std::string s = std::to_string(v);
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        size_t end = s.size();
        while (end > dot + 2 && s[end - 1] == '0') {
            --end;
        }
        s.resize(end);
    } else {
        s += ".0";
    }
    return s;
}

bool ParseASEQBlock(const std::string& aseqText,
                    std::vector<std::string>& outOrder,
                    std::vector<std::string>& outPoseParams,
                    std::vector<decompiler_enhancements::SequenceInfo>& outSequences) {
    outOrder.clear();
    outPoseParams.clear();
    outSequences.clear();

    size_t nameArrayPos = aseqText.find("m_localSequenceNameArray");
    if (nameArrayPos == std::string::npos) return false;
    size_t bracketOpen = aseqText.find('[', nameArrayPos);
    if (bracketOpen == std::string::npos) return false;
    size_t bracketStart, bracketEnd;
    if (!FindBalancedBlock(aseqText, bracketOpen, '[', ']', bracketStart, bracketEnd)) return false;
    outOrder = ParseStringArray(aseqText, bracketStart + 1, bracketEnd - 1);

    std::vector<std::string> poseParamNames;
    size_t poseParamArrayPos = aseqText.find("m_localPoseParamArray");
    if (poseParamArrayPos != std::string::npos) {
        size_t poseOpen = aseqText.find('[', poseParamArrayPos);
        if (poseOpen != std::string::npos) {
            size_t poseStart, poseEnd;
            if (FindBalancedBlock(aseqText, poseOpen, '[', ']', poseStart, poseEnd)) {
                std::vector<std::string> poseBlocks = SplitTopLevelBlocks(aseqText, poseStart + 1, poseEnd - 1);
                for (const std::string& pb : poseBlocks) {
                    std::string poseName;
                    if (ExtractKeyString(pb, 0, "m_sName", poseName)) {
                        poseParamNames.push_back(std::move(poseName));
                    }
                    else {
                        poseParamNames.emplace_back();
                    }
                }
            }
        }
    }

    std::vector<std::string> weightListNames;
    size_t boneMaskArrayPos = aseqText.find("m_localBoneMaskArray");
    if (boneMaskArrayPos != std::string::npos) {
        size_t maskOpen = aseqText.find('[', boneMaskArrayPos);
        if (maskOpen != std::string::npos) {
            size_t maskStart, maskEnd;
            if (FindBalancedBlock(aseqText, maskOpen, '[', ']', maskStart, maskEnd)) {
                std::vector<std::string> maskBlocks = SplitTopLevelBlocks(aseqText, maskStart + 1, maskEnd - 1);
                for (const std::string& mb : maskBlocks) {
                    std::string maskName;
                    if (ExtractKeyString(mb, 0, "m_sName", maskName)) {
                        weightListNames.push_back(std::move(maskName));
                    }
                    else {
                        weightListNames.emplace_back();
                    }
                }
            }
        }
    }

    size_t seqArrayPos = aseqText.find("m_localS1SeqDescArray");
    if (seqArrayPos == std::string::npos) return false;
    bracketOpen = aseqText.find('[', seqArrayPos);
    if (bracketOpen == std::string::npos) return false;
    if (!FindBalancedBlock(aseqText, bracketOpen, '[', ']', bracketStart, bracketEnd)) return false;

    std::vector<std::string> seqBlocks = SplitTopLevelBlocks(aseqText, bracketStart + 1, bracketEnd - 1);
    for (const std::string& block : seqBlocks) {
        decompiler_enhancements::SequenceInfo info;
        if (!ExtractKeyString(block, 0, "m_sName", info.name)) continue;

        int weightListIndex = -1;
        if (ExtractKeyInt(block, 0, "m_nLocalWeightlist", weightListIndex)) {
            if (weightListIndex >= 0 && static_cast<size_t>(weightListIndex) < weightListNames.size()) {
                info.weightListName = weightListNames[weightListIndex];
            }
        }

        size_t fetchPos = block.find("m_fetch");
        if (fetchPos != std::string::npos) {
            size_t fetchBrace = block.find('{', fetchPos);
            if (fetchBrace != std::string::npos) {
                size_t fetchStart, fetchEnd;
                if (FindBalancedBlock(block, fetchBrace, '{', '}', fetchStart, fetchEnd)) {
                    std::string fetchBlock = block.substr(fetchStart, fetchEnd - fetchStart);

                    bool b0D = false, b1D = false, b2D = false, b2DTri = false;
                    ExtractKeyBool(fetchBlock, 0, "m_b0D", b0D);
                    ExtractKeyBool(fetchBlock, 0, "m_b1D", b1D);
                    ExtractKeyBool(fetchBlock, 0, "m_b2D", b2D);
                    ExtractKeyBool(fetchBlock, 0, "m_b2D_TRI", b2DTri);

                    if (b2DTri) info.blendType = decompiler_enhancements::SequenceInfo::BlendType::Blend2DTri;
                    else if (b2D) info.blendType = decompiler_enhancements::SequenceInfo::BlendType::Blend2D;
                    else if (b1D) info.blendType = decompiler_enhancements::SequenceInfo::BlendType::Blend1D;

                    if (info.blendType != decompiler_enhancements::SequenceInfo::BlendType::None) {
                        size_t refPos = fetchBlock.find("m_localReferenceArray");
                        if (refPos != std::string::npos) {
                            size_t refOpen = fetchBlock.find('[', refPos);
                            if (refOpen != std::string::npos) {
                                size_t refStart, refEnd;
                                if (FindBalancedBlock(fetchBlock, refOpen, '[', ']', refStart, refEnd)) {
                                    info.referenceArray = ParseIntArray(fetchBlock, refStart + 1, refEnd - 1);
                                }
                            }
                        }

                        size_t groupPos = fetchBlock.find("m_nGroupSize");
                        if (groupPos != std::string::npos) {
                            size_t groupOpen = fetchBlock.find('[', groupPos);
                            if (groupOpen != std::string::npos) {
                                size_t groupStart, groupEnd;
                                if (FindBalancedBlock(fetchBlock, groupOpen, '[', ']', groupStart, groupEnd)) {
                                    std::vector<int> group = ParseIntArray(fetchBlock, groupStart + 1, groupEnd - 1);
                                    if (group.size() >= 1) info.groupSize[0] = group[0];
                                    if (group.size() >= 2) info.groupSize[1] = group[1];
                                }
                            }
                        }

                        size_t posePos = fetchBlock.find("m_nLocalPose");
                        if (posePos != std::string::npos) {
                            size_t poseOpen = fetchBlock.find('[', posePos);
                            if (poseOpen != std::string::npos) {
                                size_t poseStart, poseEnd;
                                if (FindBalancedBlock(fetchBlock, poseOpen, '[', ']', poseStart, poseEnd)) {
                                    std::vector<int> poses = ParseIntArray(fetchBlock, poseStart + 1, poseEnd - 1);
                                    if (poses.size() >= 1) info.localPose[0] = poses[0];
                                    if (poses.size() >= 2) info.localPose[1] = poses[1];
                                }
                            }
                        }

                        size_t key0Pos = fetchBlock.find("m_poseKeyArray0");
                        if (key0Pos != std::string::npos) {
                            size_t key0Open = fetchBlock.find('[', key0Pos);
                            if (key0Open != std::string::npos) {
                                size_t key0Start, key0End;
                                if (FindBalancedBlock(fetchBlock, key0Open, '[', ']', key0Start, key0End)) {
                                    info.poseKeyArray0 = ParseFloatArray(fetchBlock, key0Start + 1, key0End - 1);
                                }
                            }
                        }

                        size_t key1Pos = fetchBlock.find("m_poseKeyArray1");
                        if (key1Pos != std::string::npos) {
                            size_t key1Open = fetchBlock.find('[', key1Pos);
                            if (key1Open != std::string::npos) {
                                size_t key1Start, key1End;
                                if (FindBalancedBlock(fetchBlock, key1Open, '[', ']', key1Start, key1End)) {
                                    info.poseKeyArray1 = ParseFloatArray(fetchBlock, key1Start + 1, key1End - 1);
                                }
                            }
                        }
                    }
                }
            }
        }

        size_t actPos = block.find("m_activityArray");
        if (actPos != std::string::npos) {
            size_t actOpen = block.find('[', actPos);
            if (actOpen != std::string::npos) {
                size_t actStart, actEnd;
                if (FindBalancedBlock(block, actOpen, '[', ']', actStart, actEnd)) {
                    std::vector<std::string> actBlocks = SplitTopLevelBlocks(block, actStart + 1, actEnd - 1);
                    for (const std::string& ab : actBlocks) {
                        std::string name;
                        int weight = 1;
                        ExtractKeyString(ab, 0, "m_name", name);
                        size_t wpos = ab.find("m_nWeight");
                        if (wpos != std::string::npos) {
                            size_t eq = ab.find('=', wpos);
                            if (eq != std::string::npos) {
                                std::string num = std::string(Trim(std::string_view(ab).substr(eq + 1)));
                                size_t term = num.find_first_of(",}\n");
                                if (term != std::string::npos) num = num.substr(0, term);
                                try { weight = std::stoi(num); } catch (...) {}
                            }
                        }
                        if (!name.empty()) {
                            info.activities.emplace_back(name, weight);
                        }
                    }
                }
            }
        }
        outSequences.push_back(std::move(info));
    }
    outPoseParams = std::move(poseParamNames);
    return !outOrder.empty();
}

struct AnimBlock {
	std::string raw;
	std::string cls;
	std::string name;
	size_t originalIndex;
};

std::string NormalizeAnimName(const std::string& name);
std::vector<AnimBlock> CollectAllAnimBlocks(const std::string& vmdlText);
std::string ExtractOwnName(const std::string& block, const std::string& cls);
std::string AddActivityModifiersToAnimBlock(const std::string& block,
                                            const std::vector<std::pair<std::string, int>>& modifiers);
std::string ConvertAnimFileToBlend(const std::string& animBlock,
                                   const decompiler_enhancements::SequenceInfo& seq,
                                   const std::vector<std::string>& sequenceNames,
                                   const std::vector<std::string>& poseParamNames,
                                   const std::unordered_map<std::string, std::string>& seqNameToAnimName);
std::string ApplyEnhancementsToAnimBlock(const std::string& block,
                                         const std::string& name,
                                         const std::vector<std::string>& sequenceNames,
                                         const std::vector<std::string>& poseParamNames,
                                         const std::unordered_map<std::string, decompiler_enhancements::SequenceInfo>& sequences,
                                         const std::unordered_map<std::string, std::vector<std::pair<std::string, int>>>& modifiers,
                                         const std::unordered_map<std::string, std::string>& weightLists,
                                         const std::unordered_map<std::string, std::string>& seqNameToAnimName,
                                         int enchantmentFlags);
std::string ApplyEnhancementsToAllAnimFiles(const std::string& vmdlText,
                                            const std::vector<std::string>& sequenceNames,
                                            const std::vector<std::string>& poseParamNames,
                                            const std::unordered_map<std::string, decompiler_enhancements::SequenceInfo>& sequences,
                                            const std::unordered_map<std::string, std::vector<std::pair<std::string, int>>>& modifiers,
                                            const std::unordered_map<std::string, std::string>& weightLists,
                                            const std::unordered_map<std::string, std::string>& seqNameToAnimName,
                                            int enchantmentFlags);


std::vector<AnimBlock> ExtractAnimationBlocks(const std::string& vmdlText,
                                              size_t& outChildrenStart,
                                              size_t& outChildrenEnd,
                                              std::string& outChildrenPrefix,
                                              std::string& outChildrenSuffix) {
    std::vector<AnimBlock> result;
    outChildrenStart = outChildrenEnd = std::string::npos;

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

    outChildrenStart = arrStart;
    outChildrenEnd = arrEnd;
    outChildrenPrefix = vmdlText.substr(listStart, arrStart - listStart + 1);
    outChildrenSuffix = vmdlText.substr(arrEnd, listEnd - arrEnd);

    std::vector<std::string> blocks = SplitTopLevelBlocks(vmdlText, arrStart + 1, arrEnd - 1);
    for (size_t i = 0; i < blocks.size(); ++i) {
        AnimBlock ab;
        ab.raw = blocks[i];
        ab.originalIndex = i;
        ExtractKeyString(blocks[i], 0, "_class", ab.cls);
        ab.name = ExtractOwnName(blocks[i], ab.cls);
        result.push_back(std::move(ab));
    }
    return result;
}

std::vector<AnimBlock> CollectAllAnimBlocks(const std::string& vmdlText) {
    std::vector<AnimBlock> result;
    size_t pos = 0;
    while (true) {
        size_t animPos = vmdlText.find("_class = \"AnimFile\"", pos);
        size_t blend1Pos = vmdlText.find("_class = \"1DBlend\"", pos);
        size_t blend2Pos = vmdlText.find("_class = \"2DBlend\"", pos);
        size_t nextPos = std::min({ animPos, blend1Pos, blend2Pos });
        if (nextPos == std::string::npos) break;
        pos = nextPos;

        size_t brace = FindOpeningBraceBefore(vmdlText, pos);
        if (brace == std::string::npos) {
            ++pos;
            continue;
        }
        size_t blockStart, blockEnd;
        if (!FindBalancedBlock(vmdlText, brace, '{', '}', blockStart, blockEnd)) {
            ++pos;
            continue;
        }
        AnimBlock ab;
        ab.raw = vmdlText.substr(blockStart, blockEnd - blockStart);
        ExtractKeyString(ab.raw, 0, "_class", ab.cls);
        ab.name = ExtractOwnName(ab.raw, ab.cls);
        result.push_back(std::move(ab));
        pos = blockEnd;
    }
    return result;
}

std::string ExtractOwnName(const std::string& block, const std::string& cls) {
    if (cls == "Folder") {
        size_t childrenPos = block.find("children =");
        if (childrenPos != std::string::npos) {
            size_t arrOpen = block.find('[', childrenPos);
            if (arrOpen != std::string::npos) {
                size_t arrStart, arrEnd;
                if (FindBalancedBlock(block, arrOpen, '[', ']', arrStart, arrEnd)) {
                    std::string name;
                    if (ExtractKeyString(block, arrEnd, "name", name)) {
                        return name;
                    }
                }
            }
        }
    }
    std::string name;
    ExtractKeyString(block, 0, "name", name);
    return name;
}

std::string ApplyEnhancementsToAnimBlock(const std::string& block,
                                         const std::string& name,
                                         const std::vector<std::string>& sequenceNames,
                                         const std::vector<std::string>& poseParamNames,
                                         const std::unordered_map<std::string, decompiler_enhancements::SequenceInfo>& sequences,
                                         const std::unordered_map<std::string, std::vector<std::pair<std::string, int>>>& modifiers,
                                         const std::unordered_map<std::string, std::string>& weightLists,
                                         const std::unordered_map<std::string, std::string>& seqNameToAnimName,
                                         int enchantmentFlags) {
    std::string result = block;

    auto findSeq = [&](const std::string& key) {
        auto it = sequences.find(key);
        if (it != sequences.end()) return it;
        return sequences.find(NormalizeAnimName(key));
    };
    auto findWL = [&](const std::string& key) {
        auto it = weightLists.find(key);
        if (it != weightLists.end()) return it;
        return weightLists.find(NormalizeAnimName(key));
    };
    auto findMods = [&](const std::string& key) {
        auto it = modifiers.find(key);
        if (it != modifiers.end()) return it;
        return modifiers.find(NormalizeAnimName(key));
    };

    if ((enchantmentFlags & decompiler_enhancements::EnchantmentFlags::RestoreBlendAnimations) ||
        (enchantmentFlags & decompiler_enhancements::EnchantmentFlags::RestoreWeightLists) ||
        (enchantmentFlags & decompiler_enhancements::EnchantmentFlags::RestoreActivityModifiers)) {
        auto seqIt = findSeq(name);
        if (seqIt != sequences.end()) {
            const auto& seq = seqIt->second;
            if ((enchantmentFlags & decompiler_enhancements::EnchantmentFlags::RestoreBlendAnimations) &&
                seq.blendType != decompiler_enhancements::SequenceInfo::BlendType::None) {
                result = ConvertAnimFileToBlend(result, seq, sequenceNames, poseParamNames, seqNameToAnimName);
            }
        }

        if ((enchantmentFlags & decompiler_enhancements::EnchantmentFlags::RestoreWeightLists)) {
            auto wlIt = findWL(name);
            if (wlIt != weightLists.end() && !wlIt->second.empty() && result.find("weight_list_name") == std::string::npos) {
                size_t namePos = result.find("name = \"");
                if (namePos == std::string::npos) namePos = result.find("name=\"");
                if (namePos != std::string::npos) {
                    size_t firstQuote = result.find('"', namePos);
                    if (firstQuote != std::string::npos) {
                        size_t endQuote = result.find('"', firstQuote + 1);
                        if (endQuote != std::string::npos) {
                            size_t lineStart = LineStart(result, namePos);
                            std::string indent = ExtractIndent(result, lineStart);
                            std::string insertion = "\n" + indent + "weight_list_name = \"" + wlIt->second + "\"";
                            result.insert(endQuote + 1, insertion);
                        }
                    }
                }
            }
        }

        if ((enchantmentFlags & decompiler_enhancements::EnchantmentFlags::RestoreActivityModifiers)) {
            auto modIt = findMods(name);
            if (modIt != modifiers.end() && !modIt->second.empty()) {
                std::vector<std::pair<std::string, int>> mods = modIt->second;
                if (!mods.empty()) mods.erase(mods.begin());
                if (!mods.empty()) {
                    result = AddActivityModifiersToAnimBlock(result, mods);
                }
            }
        }
    }

    return result;
}




bool RemoveFieldLine(std::string& block, const char* key) {
    size_t pos = 0;
    bool removed = false;
    while ((pos = block.find(key, pos)) != std::string::npos) {
        size_t keyEnd = pos + std::strlen(key);
        if (keyEnd < block.size() && (block[keyEnd] == ' ' || block[keyEnd] == '=')) {
            size_t lineStart = LineStart(block, pos);
            size_t lineEnd = block.find('\n', pos);
            if (lineEnd == std::string::npos) lineEnd = block.size();
            else ++lineEnd;
            block.erase(lineStart, lineEnd - lineStart);
            removed = true;
            pos = lineStart;
        }
        else {
            ++pos;
        }
    }
    return removed;
}
std::string ApplyEnhancementsToAllAnimFiles(const std::string& vmdlText,
                                            const std::vector<std::string>& sequenceNames,
                                            const std::vector<std::string>& poseParamNames,
                                            const std::unordered_map<std::string, decompiler_enhancements::SequenceInfo>& sequences,
                                            const std::unordered_map<std::string, std::vector<std::pair<std::string, int>>>& modifiers,
                                            const std::unordered_map<std::string, std::string>& weightLists,
                                            const std::unordered_map<std::string, std::string>& seqNameToAnimName,
                                            int enchantmentFlags) {
    std::string result = vmdlText;
    size_t pos = 0;
    while (true) {
        size_t animPos = result.find("_class = \"AnimFile\"", pos);
        size_t blend1Pos = result.find("_class = \"1DBlend\"", pos);
        size_t blend2Pos = result.find("_class = \"2DBlend\"", pos);
        size_t nextPos = std::min({ animPos, blend1Pos, blend2Pos });
        if (nextPos == std::string::npos) break;
        pos = nextPos;

        size_t brace = FindOpeningBraceBefore(result, pos);
        if (brace == std::string::npos) {
            ++pos;
            continue;
        }
        size_t blockStart, blockEnd;
        if (!FindBalancedBlock(result, brace, '{', '}', blockStart, blockEnd)) {
            ++pos;
            continue;
        }
        std::string block = result.substr(blockStart, blockEnd - blockStart);
        std::string name;
        ExtractKeyString(block, 0, "name", name);
        std::string modified = ApplyEnhancementsToAnimBlock(block, name, sequenceNames, poseParamNames,
                                                            sequences, modifiers, weightLists, seqNameToAnimName,
                                                            enchantmentFlags);
        if (modified != block) {
            result.replace(blockStart, blockEnd - blockStart, modified);
            pos = blockStart + modified.size();
        }
        else {
            pos = blockEnd;
        }
    }
    return result;
}

std::string GetClassLineIndent(const std::string& block) {
    size_t classPos = block.find("_class = ");
    if (classPos == std::string::npos) classPos = block.find("_class=");
    if (classPos == std::string::npos) return "";
    size_t lineStart = LineStart(block, classPos);
    std::string indent;
    for (size_t i = lineStart; i < classPos; ++i) {
        if (block[i] == '\t' || block[i] == ' ') indent.push_back(block[i]);
        else break;
    }
    return indent;
}

std::string GetBlockBraceIndent(const std::string& block) {
    size_t brace = block.find('{');
    if (brace == std::string::npos) return "";
    size_t lineStart = LineStart(block, brace);
    return block.substr(lineStart, brace - lineStart);
}
std::string BuildBlendListField(const std::string& indent,
                                const std::vector<std::string>& names,
                                const std::vector<float>& weights) {
    std::string fieldIndent = indent + "\t";
    std::string entryIndent = fieldIndent + "\t";
    std::string block;
    block += fieldIndent + "blendList = \n";
    block += fieldIndent + "[\n";
    for (size_t i = 0; i < names.size(); ++i) {
        block += entryIndent + "{\n";
        block += entryIndent + "\tname = \"" + names[i] + "\"\n";
        block += entryIndent + "\tweight = " + FormatKVFloat(weights[i]) + "\n";
        block += entryIndent + "},\n";
    }
    block += fieldIndent + "]\n";
    return block;
}

std::string Build2DBlendFields(const std::string& indent,
                               const std::vector<std::string>& rowNames,
                               const std::vector<float>& rowWeights,
                               const std::vector<std::string>& colNames,
                               const std::vector<float>& colWeights,
                               const std::vector<std::vector<std::string>>& blendAnimList) {
    std::string fieldIndent = indent + "\t";
    std::string block;
    block += fieldIndent + "blend_anim_events = false\n";
    block += fieldIndent + "fixed_blend = false\n";
    block += fieldIndent + "row_fixed_blend_val = 0.5\n";
    block += fieldIndent + "col_fixed_blend_val = 0.5\n";
    block += fieldIndent + "row_pose_param_name = \"" + (!rowNames.empty() ? rowNames[0] : "") + "\"\n";
    block += fieldIndent + "col_pose_param_name = \"" + (!colNames.empty() ? colNames[0] : "") + "\"\n";

    auto buildFloatArray = [&](const std::vector<float>& vals) {
        std::string s = "[ ";
        for (size_t i = 0; i < vals.size(); ++i) {
            if (i > 0) s += ", ";
            s += FormatKVFloat(vals[i]);
        }
        s += " ]";
        return s;
    };

    block += fieldIndent + "row_weight_list = " + buildFloatArray(rowWeights) + "\n";
    block += fieldIndent + "col_weight_list = " + buildFloatArray(colWeights) + "\n";

    block += fieldIndent + "blend_anim_list = \n";
    block += fieldIndent + "[\n";
    for (size_t r = 0; r < blendAnimList.size(); ++r) {
        block += fieldIndent + "\t[\n";
        for (size_t c = 0; c < blendAnimList[r].size(); ++c) {
            block += fieldIndent + "\t\t\"" + blendAnimList[r][c] + "\",\n";
        }
        block += fieldIndent + "\t],\n";
    }
    block += fieldIndent + "]\n";
    return block;
}

std::string BuildCommonAnimFields(const std::string& fieldIndent,
                                  const decompiler_enhancements::SequenceInfo& seq) {
    std::string primaryActivity;
    int primaryWeight = 1;
    if (!seq.activities.empty()) {
        primaryActivity = seq.activities[0].first;
        primaryWeight = seq.activities[0].second;
    }
    std::string block;
    block += fieldIndent + "is_default_idle_anim = false\n";
    block += fieldIndent + "activity_name = \"" + primaryActivity + "\"\n";
    block += fieldIndent + "activity_weight = " + std::to_string(primaryWeight) + "\n";
    if (!seq.weightListName.empty()) {
        block += fieldIndent + "weight_list_name = \"" + seq.weightListName + "\"\n";
    }
    block += fieldIndent + "anim_markup_ordered = false\n";

    block += fieldIndent + "disable_compression = false\n";
    block += fieldIndent + "animgraph_additive = false\n";
    block += fieldIndent + "delete_from_compiled_model = false\n";
    return block;
}

std::string ConvertAnimFileToBlend(const std::string& animBlock,
                                   const decompiler_enhancements::SequenceInfo& seq,
                                   const std::vector<std::string>& sequenceNames,
                                   const std::vector<std::string>& poseParamNames,
                                   const std::unordered_map<std::string, std::string>& seqNameToAnimName) {
    using BlendType = decompiler_enhancements::SequenceInfo::BlendType;
    if (seq.blendType == BlendType::None) return animBlock;

    auto resolveAnimName = [&](const std::string& seqName) -> std::string {
        auto it = seqNameToAnimName.find(NormalizeAnimName(seqName));
        if (it != seqNameToAnimName.end()) return it->second;
        return seqName;
    };

    std::string block = animBlock;

    // Replace _class.
    size_t classPos = block.find("_class = \"AnimFile\"");
    if (classPos == std::string::npos) classPos = block.find("_class=\"AnimFile\"");
    if (classPos == std::string::npos) return animBlock;
    size_t valueStart = block.find('"', classPos);
    if (valueStart == std::string::npos) return animBlock;
    size_t valueEnd = block.find('"', valueStart + 1);
    if (valueEnd == std::string::npos) return animBlock;
    std::string newClassValue = (seq.blendType == BlendType::Blend1D) ? "1DBlend" : "2DBlend";
    block.replace(classPos, valueEnd - classPos + 1, "_class = \"" + newClassValue + "\"");

    // Remove AnimFile-only fields.
    RemoveFieldLine(block, "source_filename");
    RemoveFieldLine(block, "import_bone_scales");
    RemoveFieldLine(block, "start_frame");
    RemoveFieldLine(block, "end_frame");
    RemoveFieldLine(block, "framerate");
    RemoveFieldLine(block, "reverse");
    RemoveFieldLine(block, "additional_anim_files");

    // Preserve non-ActivityModifier children (e.g. AnimEvent footsteps). ActivityModifiers
    // are reconstructed from ASEQ by AddActivityModifiersToAnimBlock later.
    {
        size_t childrenPos = block.find("children =");
        if (childrenPos != std::string::npos) {
            size_t arrOpen = block.find('[', childrenPos);
            if (arrOpen != std::string::npos) {
                size_t arrStart, arrEnd;
                if (FindBalancedBlock(block, arrOpen, '[', ']', arrStart, arrEnd)) {
                    size_t lineStart = LineStart(block, childrenPos);
                    std::string arrayIndent = ExtractIndent(block, lineStart);
                    std::vector<std::string> rawChildren = SplitTopLevelBlocks(block, arrStart + 1, arrEnd - 1);
                    std::vector<std::string> preserved;
                    preserved.reserve(rawChildren.size());
                    for (const auto& child : rawChildren) {
                        std::string childClass;
                        ExtractKeyString(child, 0, "_class", childClass);
                        if (childClass != "ActivityModifier") {
                            preserved.push_back(child);
                        }
                    }
                    std::string newArray = "[\n" + arrayIndent + "]";
                    if (!preserved.empty()) {
                        newArray = "[";
                        for (const auto& child : preserved) {
                            newArray += "\n";
                            newArray += child;
                            newArray += ",";
                        }
                        newArray += "\n" + arrayIndent + "]";
                    }
                    block.replace(arrStart, arrEnd - arrStart, newArray);
                }
            }
        }
    }

    // Remove common anim fields so we can re-insert them in the canonical order.
    RemoveFieldLine(block, "is_default_idle_anim");
    RemoveFieldLine(block, "activity_name");
    RemoveFieldLine(block, "activity_weight");
    RemoveFieldLine(block, "weight_list_name");
    RemoveFieldLine(block, "anim_markup_ordered");
    RemoveFieldLine(block, "disable_compression");
    RemoveFieldLine(block, "animgraph_additive");
    RemoveFieldLine(block, "delete_from_compiled_model");

    std::string indent = GetClassLineIndent(block);
    if (!indent.empty() && indent.back() == '\t') {
        indent.pop_back();
    }
    else if (!indent.empty() && indent.back() == ' ') {
        // Assume 4-space indentation used elsewhere; revert to one level less.
        if (indent.size() >= 4) indent.resize(indent.size() - 4);
    }
    std::string fieldIndent = indent + "\t";
    std::string commonFields = BuildCommonAnimFields(fieldIndent, seq);

    std::string blendFields;
    if (seq.blendType == BlendType::Blend1D) {
        std::vector<std::string> names;
        std::vector<float> weights;
        for (size_t i = 0; i < seq.referenceArray.size(); ++i) {
            int refIdx = seq.referenceArray[i];
            std::string name = (refIdx >= 0 && static_cast<size_t>(refIdx) < sequenceNames.size()) ? resolveAnimName(sequenceNames[refIdx]) : "";
            if (name.empty()) continue;
            names.push_back(name);
            float w = (i < seq.poseKeyArray0.size()) ? seq.poseKeyArray0[i] :
                      (i < seq.poseKeyArray1.size()) ? seq.poseKeyArray1[i] : 0.0f;
            weights.push_back(w);
        }
        if (names.empty()) return animBlock;

        std::string poseParam = (seq.localPose[0] >= 0 && static_cast<size_t>(seq.localPose[0]) < poseParamNames.size()) ? poseParamNames[seq.localPose[0]] : "";
        if (poseParam.empty() && seq.localPose[1] >= 0 && static_cast<size_t>(seq.localPose[1]) < poseParamNames.size()) {
            poseParam = poseParamNames[seq.localPose[1]];
        }

        blendFields += fieldIndent + "blend_anim_events = false\n";
        blendFields += fieldIndent + "fixed_blend = false\n";
        blendFields += fieldIndent + "fixed_blend_val = 0.5\n";
        blendFields += fieldIndent + "poseParam = \"" + poseParam + "\"\n";
        blendFields += BuildBlendListField(indent, names, weights);
    }
    else {
        int rows = seq.groupSize[0];
        int cols = seq.groupSize[1];
        if (rows <= 0 || cols <= 0) return animBlock;
        size_t expected = static_cast<size_t>(rows) * static_cast<size_t>(cols);
        if (seq.referenceArray.size() < expected || seq.poseKeyArray0.size() < expected || seq.poseKeyArray1.size() < expected) {
            return animBlock;
        }

        std::vector<std::vector<std::string>> blendAnimList(static_cast<size_t>(rows), std::vector<std::string>(static_cast<size_t>(cols)));
        std::vector<float> rowWeights;
        std::vector<float> colWeights;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                size_t dataIdx = static_cast<size_t>(r * cols + c);
                int refIdx = seq.referenceArray[dataIdx];
                blendAnimList[r][c] = (refIdx >= 0 && static_cast<size_t>(refIdx) < sequenceNames.size()) ? resolveAnimName(sequenceNames[refIdx]) : "";
                if (blendAnimList[r][c].empty()) return animBlock;

                    if (c == 0) rowWeights.push_back(seq.poseKeyArray1[dataIdx]);
                    if (r == 0) colWeights.push_back(seq.poseKeyArray0[dataIdx]);

            }
        }

        std::string rowPoseParam = (seq.localPose[1] >= 0 && static_cast<size_t>(seq.localPose[1]) < poseParamNames.size()) ? poseParamNames[seq.localPose[1]] : "";
        std::string colPoseParam = (seq.localPose[0] >= 0 && static_cast<size_t>(seq.localPose[0]) < poseParamNames.size()) ? poseParamNames[seq.localPose[0]] : "";

        blendFields += Build2DBlendFields(indent, { rowPoseParam }, rowWeights, { colPoseParam }, colWeights, blendAnimList);
    }

    if (blendFields.empty()) return animBlock;

    // Insert common + blend fields before the closing brace.
    size_t closingBrace = block.rfind('}');
    if (closingBrace == std::string::npos) return animBlock;
    size_t insertPos = LineStart(block, closingBrace);
    block.insert(insertPos, commonFields + blendFields);
    return block;
}


std::string BuildActivityModifierBlock(const std::string& baseIndent, const std::string& name, int weight) {
    std::string contentIndent = baseIndent + "\t";
    std::string block;
    block += baseIndent + "{\n";
    block += contentIndent + "_class = \"ActivityModifier\"\n";
    block += contentIndent + "activity_name = \"" + name + "\"\n";
    block += contentIndent + "activity_weight = " + std::to_string(weight) + "\n";
    block += baseIndent + "}";
    return block;
}

std::string AddActivityModifiersToAnimBlock(const std::string& block,
                                            const std::vector<std::pair<std::string, int>>& modifiers) {
    if (modifiers.empty()) return block;

    size_t childrenPos = block.find("children =");
    if (childrenPos == std::string::npos) {
        size_t blockEnd = block.rfind('}');
        if (blockEnd == std::string::npos) return block;
        size_t fieldLine = LineStart(block, blockEnd);
        std::string blockIndent = ExtractIndent(block, fieldLine);
        std::string fieldIndent = blockIndent + "\t";
        std::string childIndent = fieldIndent + "\t";

        std::string childrenBlock = fieldIndent + "children = \n" + fieldIndent + "[\n";
        for (size_t i = 0; i < modifiers.size(); ++i) {
            childrenBlock += BuildActivityModifierBlock(childIndent, modifiers[i].first, modifiers[i].second);
            if (i + 1 < modifiers.size()) childrenBlock += ",";
            childrenBlock += "\n";
        }
        childrenBlock += fieldIndent + "]\n";

        std::string res = block.substr(0, fieldLine);
        res += childrenBlock;
        res += blockIndent + "}\n";
        return res;
    }

    size_t arrOpen = block.find('[', childrenPos);
    if (arrOpen == std::string::npos) return block;
    size_t arrStart, arrEnd;
    if (!FindBalancedBlock(block, arrOpen, '[', ']', arrStart, arrEnd)) return block;

    size_t firstBrace = block.find('{', arrStart + 1);
    std::string baseIndent;
    if (firstBrace != std::string::npos && firstBrace < arrEnd) {
        baseIndent = ExtractIndent(block, LineStart(block, firstBrace));
    } else {
        baseIndent = ExtractIndent(block, LineStart(block, arrStart)) + "\t";
    }

    size_t closingBracketPos = arrEnd - 1;
    size_t closingLineStart = LineStart(block, closingBracketPos);
    bool hasContent = false;
    for (size_t k = arrStart + 1; k < closingBracketPos; ++k) {
        if (!std::isspace(static_cast<unsigned char>(block[k]))) {
            hasContent = true;
            break;
        }
    }

    bool needsLeadingComma = false;
    if (hasContent) {
        size_t trimPos = closingLineStart;
        while (trimPos > arrStart + 1 && std::isspace(static_cast<unsigned char>(block[trimPos - 1]))) {
            --trimPos;
        }
        if (trimPos > arrStart + 1 && block[trimPos - 1] != ',') {
            needsLeadingComma = true;
        }
    }

    std::string insertion;
    for (const auto& mod : modifiers) {
        if (!insertion.empty()) insertion += ",\n";
        else if (needsLeadingComma) insertion += ",\n";
        else if (hasContent) insertion += "\n";
        insertion += BuildActivityModifierBlock(baseIndent, mod.first, mod.second);
    }
    if (!insertion.empty()) insertion += "\n";

    std::string res = block.substr(0, closingLineStart);
    res += insertion;
    res += block.substr(closingLineStart);
    return res;
}

std::string BuildAnimOrderBlock(const std::string& blockIndent,
                                const std::vector<std::string>& order) {
    std::string fieldIndent = blockIndent + "\t";
    std::string entryIndent = fieldIndent + "\t";
    std::string block;
    block += blockIndent + "{\n";
    block += fieldIndent + "_class = \"AnimOrder\"\n";
    block += fieldIndent + "anim_order = \n";
    block += fieldIndent + "[\n";
    for (const auto& name : order) {
        if (name.empty()) continue;
        block += entryIndent + "\"" + name + "\",\n";
    }
    block += fieldIndent + "]\n";
    block += blockIndent + "}";
    return block;
}

std::string BuildChildrenContent(const std::vector<AnimBlock>& blocks,
                                 const std::vector<std::string>& order,
                                 int enchantmentFlags) {
    std::vector<AnimBlock> working = blocks;

    if ((enchantmentFlags & decompiler_enhancements::EnchantmentFlags::FixAnimationOrder) && !order.empty()) {
        std::string blockIndent;
        if (!blocks.empty()) {
            blockIndent = GetBlockBraceIndent(blocks[0].raw);
        }
        if (blockIndent.empty()) blockIndent = "\t\t";

        AnimBlock orderBlock;
        orderBlock.raw = BuildAnimOrderBlock(blockIndent, order);
        orderBlock.cls = "AnimOrder";
        orderBlock.name.clear();
        orderBlock.originalIndex = static_cast<size_t>(-1);
        working.insert(working.begin(), std::move(orderBlock));
    }

    std::string content;
    for (size_t i = 0; i < working.size(); ++i) {
        if (i > 0) content += ",\n";
        else content += "\n";
        content += working[i].raw;
    }
    if (!working.empty()) content += "\n";
    return content;
}

} // namespace

namespace decompiler_enhancements {

bool ApplyEnhancements(const std::string& vmdlPath, const std::string& compiledInputPath,
                       const std::string& aseqBlockText, int enchantmentFlags,
                       std::string& outLog) {
    (void)compiledInputPath;
    outLog.clear();
    if (enchantmentFlags == EnchantmentFlags::None) return true;

    std::string vmdlText = ReadFile(vmdlPath);
    if (vmdlText.empty()) {
        outLog = OBF_CSTR("Failed to read .vmdl file: ") + vmdlPath;
        return false;
    }

    std::vector<std::string> order;
    std::vector<std::string> poseParams;
    std::vector<SequenceInfo> sequences;
    std::string normalizedAseq = MaybeConvertUtf16ToUtf8(aseqBlockText);
    if (!ParseASEQBlock(normalizedAseq, order, poseParams, sequences)) {
        outLog = OBF_CSTR("Failed to parse ASEQ block.");
        return false;
    }

    std::unordered_map<std::string, std::vector<std::pair<std::string, int>>> modifiers;
    std::unordered_map<std::string, std::string> weightLists;
    std::unordered_map<std::string, SequenceInfo> sequenceMap;
    for (const auto& seq : sequences) {
        std::string normalizedSeqName = NormalizeAnimName(seq.name);
        modifiers[normalizedSeqName] = seq.activities;
        if (!seq.weightListName.empty()) {
            weightLists[normalizedSeqName] = seq.weightListName;
        }
        sequenceMap[normalizedSeqName] = seq;
    }

    size_t childrenStart, childrenEnd;
    std::string prefix, suffix;
    std::vector<AnimBlock> blocks = ExtractAnimationBlocks(vmdlText, childrenStart, childrenEnd, prefix, suffix);
    if (blocks.empty()) {
        outLog = OBF_CSTR("No AnimationList children found in .vmdl.");
        return false;
    }

    std::string newChildren = BuildChildrenContent(blocks, order, enchantmentFlags);

    std::string patched = vmdlText.substr(0, childrenStart + 1);
    patched += newChildren;
    patched += vmdlText.substr(childrenEnd - 1);
    vmdlText = std::move(patched);

    std::unordered_map<std::string, std::string> seqNameToAnimName;
    std::vector<AnimBlock> allAnimBlocks = CollectAllAnimBlocks(vmdlText);
    for (const auto& ab : allAnimBlocks) {
        std::string normalized = NormalizeAnimName(ab.name);
        if (!normalized.empty() && seqNameToAnimName.find(normalized) == seqNameToAnimName.end()) {
            seqNameToAnimName[normalized] = ab.name;
        }
    }

    vmdlText = ApplyEnhancementsToAllAnimFiles(vmdlText, order, poseParams, sequenceMap, modifiers, weightLists, seqNameToAnimName, enchantmentFlags);

    if (!IsKV3Balanced(vmdlText)) {
        outLog = OBF_CSTR("Patched .vmdl is not KV3-balanced; aborting write.");
        return false;
    }

    if (!WriteFile(vmdlPath, vmdlText)) {
        outLog = OBF_CSTR("Failed to write patched .vmdl file.");
        return false;
    }

    outLog = OBF_CSTR("Applied enchantments to ") + vmdlPath +
             OBF_CSTR(". Top-level blocks: ") + std::to_string(blocks.size()) +
             OBF_CSTR(", animation blocks processed: ") + std::to_string(allAnimBlocks.size()) +
             OBF_CSTR(", ASEQ order entries: ") + std::to_string(order.size()) +
             OBF_CSTR(", weight lists restored: ") + std::to_string(weightLists.size()) +
             OBF_CSTR(", blend animations restored: ") + std::to_string(sequenceMap.size()) + ". ";
    return true;
}


} // namespace decompiler_enhancements
