#include "DecompilerUI.h"

#include <windows.h>
#include <shobjidl.h>
#include <cstring>
#include <shlobj.h>
#include <commdlg.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <sstream>

#include "imgui.h"
#include "VPKManager.h"
#include "SteamManager.h"
#include "VRF.h"
#include "parser/Utils.h"
#include "parser/ModelDecompiler.h"
#include "DecompilerEnhancements.h"
#include "SecurityHardening.h"
#include "ModelHelper.h"
#include "DmxBinary.h"
#include "GltfMorph.h"
#include "MorphMerge.h"

#include <regex>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;
using decompiler_ui::DecompilerConfig;
using decompiler_ui::DecompilerState;

namespace {

std::atomic_bool g_decompilerRunning{ false };
std::atomic_bool g_modelHelperRunning{ false };

fs::path GetExecutableDirectory() {
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        try {
            return fs::current_path();
        }
        catch (const std::exception&) {
            return fs::path(".");
        }
    }
    return fs::path(buffer).parent_path();
}

fs::path GetProjectDirectory() {
    try {
        fs::path current = fs::current_path();
        fs::path root = current.root_path();
        fs::path check = current;
        while (!check.empty() && check != root) {
            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(check, ec)) {
                if (entry.is_regular_file(ec) && entry.path().extension() == OBF_CSTR(".vcxproj")) {
                    return check;
                }
            }
            fs::path parent = check.parent_path();
            if (parent == check) break;
            check = parent;
        }
    }
    catch (const std::exception&) {}
    try {
        return fs::current_path();
    }
    catch (const std::exception&) {
        return fs::path(".");
    }
}

fs::path ResolveProjectRelativePath(const std::string& path) {
    try {
        fs::path p(path);
        if (p.is_absolute()) return p.lexically_normal();
        return (GetProjectDirectory() / p).lexically_normal();
    }
    catch (const std::exception&) {
        return fs::path(path);
    }
}

fs::path GetTempPathInProject(const std::string& prefix, const std::string& suffix) {
    static std::atomic<uint64_t> counter{ 0 };
    uint64_t id = counter.fetch_add(1);
    fs::path tempDir = GetProjectDirectory() / OBF_CSTR("temp");
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    return (tempDir / (prefix + std::to_string(id) + suffix)).lexically_normal();
}

fs::path GetSettingsPath() {
    return GetExecutableDirectory() / OBF_CSTR("configs") / OBF_CSTR("decompiler_settings.json");
}

void LoadString(char* dst, size_t dstSize, const json& j, const char* key) {
    if (j.contains(key) && j[key].is_string()) {
        std::string value = j[key].get<std::string>();
        if (dstSize > 0) {
            size_t copyLen = std::min(value.size(), dstSize - 1);
            std::memcpy(dst, value.data(), copyLen);
            dst[copyLen] = '\0';
        }
    }
}

void SaveString(json& j, const char* key, const char* value) {
    j[key] = value;
}

std::string ExtractModelNameFromDataBlock(const std::string& dataBlock) {
    size_t pos = dataBlock.find(OBF_CSTR("m_name"));
    if (pos == std::string::npos) return OBF_CSTR("");

    pos = dataBlock.find('"', pos);
    if (pos == std::string::npos) return OBF_CSTR("");

    size_t nameStart = pos + 1;
    size_t nameEnd = dataBlock.find('"', nameStart);
    if (nameEnd == std::string::npos) return OBF_CSTR("");

    return dataBlock.substr(nameStart, nameEnd - nameStart);
}

bool ShouldUseVpkContext(const std::string& inputPath, const std::string& dotaPath) {
    if (inputPath.length() < 3) return false;
    std::string ext = inputPath.substr(inputPath.find_last_of('.') + 1);
    for (auto& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    if (ext != OBF_CSTR("vmdl_c") && ext != OBF_CSTR("vmat_c") && ext != OBF_CSTR("vtex_c") &&
        ext != OBF_CSTR("vsnd_c") && ext != OBF_CSTR("vpcf_c") && ext != OBF_CSTR("vcs_c"))
        return false;
    if (!fs::exists(inputPath)) return false;
    try {
        fs::path absInput = fs::absolute(inputPath);
        fs::path absDota = fs::absolute(dotaPath);
        std::string inputStr = absInput.string();
        std::string dotaStr = absDota.string();
        std::transform(inputStr.begin(), inputStr.end(), inputStr.begin(),
            [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        std::transform(dotaStr.begin(), dotaStr.end(), dotaStr.begin(),
            [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        return inputStr.find(dotaStr) == 0;
    }
    catch (const std::exception&) {
        return false;
    }
}

std::string OpenFolderDialog(HWND hwnd, const char* title) {
    std::string result;

    HRESULT coinit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(coinit) && coinit != RPC_E_CHANGED_MODE) {
        return result;
    }

    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr)) {
        DWORD options;
        if (SUCCEEDED(pfd->GetOptions(&options))) {
            pfd->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        }

        if (title && title[0] != '\0') {
            int titleLen = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
            if (titleLen > 0) {
                std::wstring wtitle(titleLen, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle.data(), titleLen);
                pfd->SetTitle(wtitle.c_str());
            }
        }
        pfd->SetOkButtonLabel(L"Select Folder");

        hr = pfd->Show(hwnd);
        if (SUCCEEDED(hr)) {
            IShellItem* psi = nullptr;
            hr = pfd->GetResult(&psi);
            if (SUCCEEDED(hr)) {
                PWSTR path = nullptr;
                hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &path);
                if (SUCCEEDED(hr) && path) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        result.resize(len - 1);
                        WideCharToMultiByte(CP_UTF8, 0, path, -1, result.data(), len, nullptr, nullptr);
                    }
                    CoTaskMemFree(path);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }

    if (coinit == S_OK) {
        CoUninitialize();
    }
    return result;
}

std::string OpenFileDialog(HWND hwnd, const char* title, const char* filter) {
    char fileName[MAX_PATH] = { 0 };

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrTitle = title;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(fileName);
    }
    return "";
}

std::string QuoteCmdArg(const std::string& arg) {
    std::string out;
    out.reserve(arg.size() + 2);
    out += '"';
    for (size_t i = 0; i < arg.size(); ++i) {
        if (arg[i] == '\\') {
            size_t j = i;
            while (j < arg.size() && arg[j] == '\\') ++j;
            size_t count = j - i;
            bool followedByQuote = (j == arg.size()) || (arg[j] == '"');
            out.append(count * (followedByQuote ? 2 : 1), '\\');
            i = j - 1;
        }
        else if (arg[i] == '"') {
            out += "\\\"";
        }
        else {
            out += arg[i];
        }
    }
    out += '"';
    return out;
}

std::string BuildOutputPath(const std::string& outputDir, const std::string& /*inputPath*/) {
    return ResolveProjectRelativePath(outputDir).string();
}

std::string BuildModelDecompileCommand(const DecompilerConfig& s,
    const std::string& inputFile,
    const std::string& outputFolder,
    bool inputIsVpkWithFilter,
    const std::string& vpkFilter)
{
    std::string cmd;

    cmd += OBF_CSTR("-i ") + QuoteCmdArg(inputFile);
    cmd += OBF_CSTR(" -o ") + QuoteCmdArg(outputFolder);

    if (inputIsVpkWithFilter && !vpkFilter.empty()) {
        cmd += OBF_CSTR(" -f ") + QuoteCmdArg(vpkFilter);
    }

    if (s.recursive) cmd += OBF_CSTR(" --recursive");
    if (s.recursiveVpk) cmd += OBF_CSTR(" --recursive_vpk");
    if (s.vpkCache) cmd += OBF_CSTR(" --vpk_cache");
    if (s.vpkVerify) cmd += OBF_CSTR(" --vpk_verify");

    if (!inputIsVpkWithFilter && s.vpkExtensions[0] != '\0') {
        cmd += OBF_CSTR(" -e ") + QuoteCmdArg(s.vpkExtensions);
    }
    if (!inputIsVpkWithFilter && s.vpkFilepath[0] != '\0') {
        cmd += OBF_CSTR(" -f ") + QuoteCmdArg(s.vpkFilepath);
    }

    if (s.allBlocks) {
        cmd += OBF_CSTR(" -a");
    }
    else if (s.blockName[0] != '\0') {
        cmd += OBF_CSTR(" -b ") + std::string(s.blockName);
    }

    if (s.vpkDecompile) cmd += OBF_CSTR(" -d");

    if (s.textureDecodeFlags == 1) cmd += OBF_CSTR(" --texture_decode_flags none");
    else if (s.textureDecodeFlags == 2) cmd += OBF_CSTR(" --texture_decode_flags ForceLDR");

    if (s.vpkList) cmd += OBF_CSTR(" --vpk_list");
    if (s.vpkDir) cmd += OBF_CSTR(" --vpk_dir");

    if (s.gltfExportFormat == 1) {
        cmd += OBF_CSTR(" --gltf_export_format gltf");
    }
    else if (s.gltfExportFormat == 2) {
        cmd += OBF_CSTR(" --gltf_export_format glb");
    }

    if (s.gltfExportFormat != 0) {
        if (s.gltfExportMaterials) cmd += OBF_CSTR(" --gltf_export_materials");
        if (s.gltfExportAnimations) cmd += OBF_CSTR(" --gltf_export_animations");
        if (s.gltfTexturesAdapt) cmd += OBF_CSTR(" --gltf_textures_adapt");
        if (s.gltfExportExtras) cmd += OBF_CSTR(" --gltf_export_extras");
        if (s.gltfMeshList[0] != '\0') {
            cmd += OBF_CSTR(" --gltf_mesh_list ") + QuoteCmdArg(s.gltfMeshList);
        }
        if (s.gltfAnimationList[0] != '\0') {
            cmd += OBF_CSTR(" --gltf_animation_list ") + QuoteCmdArg(s.gltfAnimationList);
        }
    }

    if (s.toolsAssetInfoShort) cmd += OBF_CSTR(" --tools_asset_info_short");
    if (s.threads > 1) cmd += OBF_CSTR(" --threads ") + std::to_string(s.threads);

    return cmd;
}

std::string BuildFilesCommand(const DecompilerConfig& s,
    const std::string& inputFile,
    const std::string& outputFolder,
    bool inputIsVpkWithFilter,
    const std::string& vpkFilter)
{
    DecompilerConfig fileState = s;
    fileState.allBlocks = false;
    fileState.blockName[0] = '\0';
    return BuildModelDecompileCommand(fileState, inputFile, outputFolder, inputIsVpkWithFilter, vpkFilter);
}

std::string BuildBlocksCommand(const DecompilerConfig& s,
    const std::string& inputFile,
    const std::string& outputFolder)
{
    DecompilerConfig blockState = s;
    blockState.vpkDecompile = true;
    blockState.gltfExportFormat = 0;
    blockState.vpkList = false;
    blockState.vpkDir = false;
    blockState.recursive = false;
    blockState.recursiveVpk = false;
    blockState.vpkExtensions[0] = '\0';
    blockState.vpkFilepath[0] = '\0';
    blockState.gltfExportMaterials = false;
    blockState.gltfExportAnimations = false;
    blockState.gltfTexturesAdapt = false;
    blockState.gltfExportExtras = false;
    blockState.gltfMeshList[0] = '\0';
    blockState.gltfAnimationList[0] = '\0';
    blockState.toolsAssetInfoShort = false;
    blockState.threads = 1;
    return BuildModelDecompileCommand(blockState, inputFile, outputFolder, false, "");
}

bool IsVpkInput(const std::string& path) {
    if (path.length() >= 4) {
        std::string ext = path.substr(path.length() - 4);
        for (auto& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        return ext == OBF_CSTR(".vpk");
    }
    return false;
}

bool IsCompiledSource2Input(const std::string& path) {
    if (path.length() < 3) return false;
    std::string ext = path.substr(path.find_last_of('.') + 1);
    for (auto& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    // Common Source 2 compiled resource extensions.
    static const std::vector<std::string> compiledExts = {
        OBF_CSTR("vmdl_c"), OBF_CSTR("vmat_c"), OBF_CSTR("vtex_c"), OBF_CSTR("vsnd_c"),
        OBF_CSTR("vpcf_c"), OBF_CSTR("vfont_c"), OBF_CSTR("vcss_c"), OBF_CSTR("vjs_c"),
        OBF_CSTR("vxml_c"), OBF_CSTR("vanim_c"), OBF_CSTR("vseq_c"), OBF_CSTR("vmorf_c"),
        OBF_CSTR("vphys_c"), OBF_CSTR("vwnod_c"), OBF_CSTR("vwrld_c"), OBF_CSTR("vsurf_c"),
        OBF_CSTR("vtmv_c"), OBF_CSTR("vfeat_c"), OBF_CSTR("vcomp_c"), OBF_CSTR("vbuf_c"),
        OBF_CSTR("vcs_c"), OBF_CSTR("vrman_c"), OBF_CSTR("vpost_c"), OBF_CSTR("vdlg_c"),
        OBF_CSTR("vsmart_c"), OBF_CSTR("vdpn_c")
    };
    return std::find(compiledExts.begin(), compiledExts.end(), ext) != compiledExts.end();
}

bool IsModelInput(const std::string& path) {
    return skin_parser::Utils::isModelPath(path);
}

void FlattenOutputIfSingleFile(const std::string& outputPath, const std::string& inputPath) {
    if (IsVpkInput(inputPath)) return;

    std::string compiledPath = inputPath;
    if (IsModelInput(inputPath)) {
        fs::path p(inputPath);
        p.replace_extension(OBF_CSTR(".vmdl_c"));
        compiledPath = p.string();
    }
    if (!IsCompiledSource2Input(compiledPath)) return;

    fs::path inputFs(compiledPath);
    std::string relDir = inputFs.parent_path().string();
    std::replace(relDir.begin(), relDir.end(), '\\', '/');
    if (relDir.empty() || relDir == ".") return;

    fs::path expectedSubdir = fs::path(outputPath) / relDir;
    if (!fs::exists(expectedSubdir) || !fs::is_directory(expectedSubdir)) return;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(expectedSubdir, ec)) {
        fs::path dest = fs::path(outputPath) / entry.path().filename();
        if (fs::exists(dest)) {
            fs::remove_all(dest, ec);
        }
        fs::rename(entry.path(), dest, ec);
        if (ec) {
            fs::copy(entry.path(), dest, fs::copy_options::overwrite_existing | fs::copy_options::recursive, ec);
            if (!ec) {
                fs::remove_all(entry.path(), ec);
            }
            ec.clear();
        }
    }

    fs::path cur = expectedSubdir;
    while (cur != fs::path(outputPath) && cur != fs::path(outputPath).parent_path()) {
        if (!fs::exists(cur)) break;
        if (fs::is_empty(cur, ec)) {
            fs::remove(cur, ec);
            cur = cur.parent_path();
        } else break;
    }
}

fs::path FindDecompiledVmdl(const std::string& outputPath, const std::string& relativeInputPath) {
    // Derive the expected decompiled file name from the input resource name,
    // e.g. "models/heroes/lina/lina.vmdl_c" -> "lina.vmdl".
    fs::path relFs(relativeInputPath);
    std::string expectedName = relFs.filename().string();
    if (expectedName.size() > 2) {
        std::string tail = expectedName.substr(expectedName.size() - 2);
        std::transform(tail.begin(), tail.end(), tail.begin(),
            [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        if (tail == OBF_CSTR("_c")) expectedName = expectedName.substr(0, expectedName.size() - 2);
    }
    if (expectedName.empty()) return {};

    std::error_code ec;
    // 1) Decompiler preserves the input's directory structure under the output dir.
    // 2) Flattened layout (single-file decompile moved everything to the root).
    std::vector<fs::path> candidates = {
        fs::path(outputPath) / relFs.parent_path() / expectedName,
        fs::path(outputPath) / expectedName
    };
    for (const auto& c : candidates) {
        if (fs::exists(c, ec) && fs::is_regular_file(c, ec)) return c.lexically_normal();
    }

    // 3) Last resort: recursive search, but ONLY for an exact file-name match.
    //    Never grab an unrelated .vmdl left over from a previous decompile.
    std::string wanted = expectedName;
    std::transform(wanted.begin(), wanted.end(), wanted.begin(),
        [](unsigned char c) { return static_cast<char>(::tolower(c)); });
    for (const auto& entry : fs::recursive_directory_iterator(outputPath, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string fn = entry.path().filename().string();
        std::transform(fn.begin(), fn.end(), fn.begin(),
            [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        if (fn == wanted) return entry.path().lexically_normal();
    }
    return {};
}

bool ApplyMorphMergeToOutput(const std::string& outputPath,
    const fs::path& vmdlPath,
    const std::string& rawInput,
    const std::string& actualInput,
    bool inputIsVpkWithFilter,
    const std::string& vpkFilter,
    std::string& outLog)
{
    std::ostringstream log;
    auto append = [&](const std::string& s) { log << s << "\n"; };

    auto asciiLower = [](std::string s) {
        for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        return s;
    };
    auto icontains = [&](const std::string& hay, const std::string& needle) {
        if (needle.empty()) return false;
        return asciiLower(hay).find(asciiLower(needle)) != std::string::npos;
    };

    // 1) Collect EVERY render-mesh .dmx referenced by the .vmdl (RenderMeshFile.filename),
    //    in file order. Animations use source_filename= and are intentionally skipped by
    //    the regex (the char preceding "filename" must not be a letter/underscore).
    std::vector<std::string> meshRefs;
    try {
        std::ifstream vf(vmdlPath);
        std::stringstream ss; ss << vf.rdbuf();
        std::string vtext = ss.str();
        std::regex re(OBF_CSTR("(^|[^A-Za-z_])filename\\s*=\\s*\"([^\"]+\\.dmx)\""));
        for (std::sregex_iterator it(vtext.begin(), vtext.end(), re), end; it != end; ++it) {
            if (it->size() >= 3) meshRefs.push_back((*it)[2].str());
        }
    }
    catch (const std::exception& e) { append(OBF_CSTR("Morph: failed to read vmdl: ") + std::string(e.what())); }

    fs::path vmdlDir = vmdlPath.parent_path();
    auto resolveMesh = [&](const std::string& rel) -> fs::path {
        fs::path c1 = fs::path(outputPath) / rel;
        if (fs::exists(c1)) return c1;
        fs::path c2 = vmdlDir / fs::path(rel).filename();
        if (fs::exists(c2)) return c2;
        fs::path c3 = vmdlDir / rel;
        if (fs::exists(c3)) return c3;
        fs::path c4 = fs::path(outputPath) / fs::path(rel).filename();
        if (fs::exists(c4)) return c4;
        return {};
    };

    std::vector<fs::path> meshPaths;
    for (const auto& r : meshRefs) {
        fs::path p = resolveMesh(r);
        if (p.empty()) continue;
        std::string key = p.lexically_normal().string();
        bool dup = false;
        for (const auto& q : meshPaths) if (q.lexically_normal().string() == key) { dup = true; break; }
        if (!dup) meshPaths.push_back(p);
    }
    if (meshPaths.empty()) {
        std::error_code ec;
        uintmax_t best = 0;
        fs::path fallback;
        for (const auto& entry : fs::recursive_directory_iterator(vmdlDir, ec)) {
            if (entry.is_regular_file(ec) && entry.path().extension() == OBF_CSTR(".dmx")) {
                auto sz = entry.file_size(ec);
                if (sz > best) { best = sz; fallback = entry.path(); }
            }
        }
        if (!fallback.empty()) meshPaths.push_back(fallback);
    }
    if (meshPaths.empty()) {
        outLog = OBF_CSTR("Morph: mesh .dmx not found in output.");
        return false;
    }
    append(OBF_CSTR("Morph: render meshes in vmdl: ") + std::to_string(meshPaths.size()));

    // 2) Resolve compiled model input (mirror ASEQ extraction logic).
    std::string compiledPath;
    if (IsCompiledSource2Input(rawInput)) compiledPath = rawInput;
    else if (IsModelInput(rawInput)) {
        fs::path p(rawInput); p.replace_extension(OBF_CSTR(".vmdl_c")); compiledPath = p.string();
    }
    std::string src, filter;
    bool useFilter = false;
    if (!compiledPath.empty() && fs::exists(compiledPath)) src = compiledPath;
    else if (inputIsVpkWithFilter && !vpkFilter.empty() && fs::exists(actualInput)) { src = actualInput; filter = vpkFilter; useFilter = true; }
    else src = actualInput;

    // 3) Export glTF (with morph targets) to a temp directory.
    fs::path tempDir = GetTempPathInProject(OBF_CSTR("morph_gltf_"), OBF_CSTR(""));
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    std::string args = OBF_CSTR("-i \"") + src + OBF_CSTR("\" -o \"") + tempDir.string() + OBF_CSTR("\"");
    if (useFilter) args += OBF_CSTR(" -f \"") + filter + OBF_CSTR("\"");
    args += OBF_CSTR(" -d --gltf_export_format gltf --gltf_export_animations");
    std::string vout;
    append(OBF_CSTR("Morph: exporting glTF for morph targets..."));
    bool ok = VRF::GetInstance().DecompileWithOutput(args, vout);
    if (!ok) {
        fs::remove_all(tempDir, ec);
        outLog = log.str() + OBF_CSTR("Morph: glTF export failed.\n") + vout;
        return false;
    }

    fs::path gltfPath;
    for (const auto& entry : fs::recursive_directory_iterator(tempDir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == OBF_CSTR(".gltf")) { gltfPath = entry.path(); break; }
    }
    if (gltfPath.empty()) {
        fs::remove_all(tempDir, ec);
        outLog = log.str() + OBF_CSTR("Morph: no .gltf produced by export.");
        return false;
    }
    append(OBF_CSTR("Morph: glTF: ") + gltfPath.string());

    // 4) Load every glTF primitive that carries morph targets.
    std::string err;
    std::vector<gltfmorph::GltfMesh> gmeshes;
    if (!gltfmorph::Load(gltfPath.string(), gmeshes, err)) {
        fs::remove_all(tempDir, ec);
        outLog = log.str() + OBF_CSTR("Morph: failed to read glTF: ") + err;
        return false;
    }
    for (const auto& gm : gmeshes) {
        append(OBF_CSTR("Morph: glTF prim '") + gm.name + OBF_CSTR("' verts=") + std::to_string(gm.vertexCount) +
            OBF_CSTR(" morphTargets=") + std::to_string(gm.targets.size()));
    }

    // 5) For each render mesh, match it to a glTF morph primitive (by bind vertex count
    //    and/or mesh name) and merge. Meshes with no matching primitive (e.g. lower LODs
    //    that carry no flex data) are skipped rather than corrupted with mismatched deltas.
    std::vector<bool> used(gmeshes.size(), false);
    int mergedCount = 0, skippedCount = 0;
    morphmerge::Options opt;
    for (const auto& meshPath : meshPaths) {
        append(OBF_CSTR("Morph: mesh: ") + meshPath.string());
        std::string derr;
        dmxbin::Doc doc;
        if (!dmxbin::ReadFile(meshPath.string(), doc, derr)) {
            append(OBF_CSTR("Morph:   read failed: ") + derr); ++skippedCount; continue;
        }
        int bindVerts = morphmerge::GetBindVertexCount(doc);
        std::string meshName = morphmerge::GetMeshName(doc);

        int pick = -1;
        for (size_t gi = 0; gi < gmeshes.size() && pick < 0; ++gi) {
            if (used[gi]) continue;
            const auto& gm = gmeshes[gi];
            bool vmatch = (bindVerts > 0 && gm.vertexCount == bindVerts);
            bool nmatch = (!meshName.empty() && (icontains(meshName, gm.name) || icontains(gm.name, meshName)));
            if (vmatch && nmatch) pick = (int)gi;
        }
        if (pick < 0 && bindVerts > 0) {
            int cand = -1, nC = 0;
            for (size_t gi = 0; gi < gmeshes.size(); ++gi) {
                if (used[gi]) continue;
                if (gmeshes[gi].vertexCount == bindVerts) { cand = (int)gi; ++nC; }
            }
            if (nC == 1) pick = cand;
        }
        if (pick < 0 && bindVerts <= 0 && !meshName.empty()) {
            for (size_t gi = 0; gi < gmeshes.size() && pick < 0; ++gi) {
                if (used[gi]) continue;
                const auto& gm = gmeshes[gi];
                if (icontains(meshName, gm.name) || icontains(gm.name, meshName)) pick = (int)gi;
            }
        }

        if (pick < 0) {
            append(OBF_CSTR("Morph:   skipped (no glTF morph primitive matches bindVerts=") +
                std::to_string(bindVerts) + OBF_CSTR(" name='") + meshName + OBF_CSTR("')"));
            ++skippedCount;
            continue;
        }

        used[pick] = true;
        const gltfmorph::GltfMesh& gm = gmeshes[pick];
        append(OBF_CSTR("Morph:   matched glTF '") + gm.name + OBF_CSTR("' verts=") +
            std::to_string(gm.vertexCount) + OBF_CSTR(" targets=") + std::to_string(gm.targets.size()));
        morphmerge::StripMorphs(doc);
        std::string mlog;
        if (!morphmerge::Merge(doc, gm, opt, mlog)) {
            append(OBF_CSTR("Morph:   merge failed: ") + mlog); ++skippedCount; continue;
        }
        append(OBF_CSTR("Morph:   ") + mlog);
        morphmerge::WireCombinationOperatorToScene(doc);
        append(OBF_CSTR("Morph:   linked combinationOperator to scene root"));
        if (!dmxbin::WriteFile(meshPath.string(), doc, derr)) {
            append(OBF_CSTR("Morph:   write failed: ") + derr); ++skippedCount; continue;
        }
        append(OBF_CSTR("Morph:   wrote facial flexes -> ") + meshPath.string());
        ++mergedCount;
    }

    fs::remove_all(tempDir, ec);
    append(OBF_CSTR("Morph: done. meshes merged=") + std::to_string(mergedCount) +
        OBF_CSTR(" skipped=") + std::to_string(skippedCount));
    outLog = log.str();
    return mergedCount > 0;
}

bool ExtractAseqBlock(const std::string& rawInput,
                      const std::string& actualInput,
                      bool inputIsVpkWithFilter,
                      const std::string& vpkFilter,
                      const std::string& tempVpkFile,
                      std::string& outAseq,
                      std::string& outLog) {
    outAseq.clear();
    outLog.clear();

    std::string compiledPath;
    if (IsCompiledSource2Input(rawInput)) {
        compiledPath = rawInput;
    }
    else if (IsModelInput(rawInput)) {
        fs::path p(rawInput);
        p.replace_extension(OBF_CSTR(".vmdl_c"));
        compiledPath = p.string();
    }
    else {
        outLog = OBF_CSTR("Cannot extract ASEQ: input is not a model.");
        return false;
    }

    if (fs::exists(compiledPath)) {
        bool ok = VRF::GetInstance().DecompileBlock(compiledPath, OBF_CSTR("ASEQ"), outAseq, OBF_CSTR(""));
        if (!ok || outAseq.empty()) {
            outLog = OBF_CSTR("ASEQ extraction failed for: ") + compiledPath;
            return false;
        }
        return true;
    }

    if (inputIsVpkWithFilter && !vpkFilter.empty() && fs::exists(actualInput)) {
        std::string args = OBF_CSTR("-i \"") + actualInput + OBF_CSTR("\" -f \"") + vpkFilter + OBF_CSTR("\" -b ASEQ");
        bool ok = VRF::GetInstance().DecompileWithOutput(args, outAseq);
        if (!ok || outAseq.empty()) {
            outLog = OBF_CSTR("ASEQ extraction from VPK failed.");
            return false;
        }
        return true;
    }

    if (!tempVpkFile.empty() && fs::exists(tempVpkFile)) {
        std::string entryName = fs::path(rawInput).filename().string();
        std::replace(entryName.begin(), entryName.end(), '\\', '/');
        std::string args = OBF_CSTR("-i \"") + tempVpkFile + OBF_CSTR("\" -f \"") + entryName + OBF_CSTR("\" -b ASEQ");
        bool ok = VRF::GetInstance().DecompileWithOutput(args, outAseq);
        if (!ok || outAseq.empty()) {
            outLog = OBF_CSTR("ASEQ extraction from temporary VPK failed.");
            return false;
        }
        return true;
    }

    outLog = OBF_CSTR("Compiled model not found for ASEQ extraction: ") + compiledPath;
    return false;
}

void RunDecompileJob(std::shared_ptr<DecompilerState> state) {
    SH_AD_GUI();

    DecompilerConfig local;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        local = state->config;
        state->lastDecompilerOutput.clear();
    }

    auto logLine = [state](const std::string& line) {
        std::cout << OBF_CSTR("[Decompiler] ") << line << std::endl;
        if (state) {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->lastDecompilerOutput.empty()) state->lastDecompilerOutput += "\n";
            state->lastDecompilerOutput += line;
        }
    };

    auto setStatus = [state](float p, const std::string& msg) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->progress = p;
        state->status = msg;
    };

    auto finish = [state](bool ok, const std::string& msg) {
        std::function<void()> callback;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->busy = false;
            state->progress = ok ? 1.0f : 0.0f;
            state->status = msg;
            state->showStatus = true;
            state->lastError = ok ? "" : msg;
            g_decompilerRunning.store(false);
            callback = std::move(state->onCompletion);
            state->onCompletion = nullptr;
        }
        if (callback) callback();
    };

    logLine(OBF_CSTR("Job started. Input: ") + std::string(local.inputPath) + OBF_CSTR(", Output: ") + std::string(local.outputDir));
    logLine(OBF_CSTR("Enchantment flags: ") + std::to_string(local.enchantmentFlags));

    setStatus(0.05f, OBF_CSTR("Checking Dota 2 path..."));
    SteamManager& steam = SteamManager::GetInstance();
    if (!steam.HasValidPath()) {
        logLine(OBF_CSTR("Dota 2 path is not set."));
        finish(false, OBF_CSTR("Dota 2 path is not set. Configure Dota 2 path first."));
        return;
    }
    logLine(OBF_CSTR("Dota 2 path: ") + steam.dotaPath);
    logLine(OBF_CSTR("VPK path: ") + steam.vpkPath);

    setStatus(0.10f, OBF_CSTR("Checking decompiler tool..."));
    if (VRF::IsSetupNeeded()) {
        setStatus(0.12f, OBF_CSTR("Downloading Source2Viewer-CLI..."));
        logLine(OBF_CSTR("Source2Viewer-CLI missing, running Setup..."));
        if (!VRF::GetInstance().Setup()) {
            logLine(OBF_CSTR("Setup failed."));
            finish(false, OBF_CSTR("Failed to download or extract Source2Viewer-CLI."));
            return;
        }
        logLine(OBF_CSTR("Setup completed."));
    }

    std::string rawInput = local.inputPath;
    std::string outputDir = ResolveProjectRelativePath(local.outputDir).string();
    std::string inputPath = ResolveProjectRelativePath(rawInput).string();

    if (!IsVpkInput(inputPath) && !IsCompiledSource2Input(inputPath) && !IsModelInput(inputPath)) {
        logLine(OBF_CSTR("Input is not a compiled Source 2 file, model, or VPK archive."));
        finish(false, OBF_CSTR("Input must be a compiled Source 2 file (.vmdl_c, .vmat_c, .vtex_c, ...), a model (.vmdl), or a .vpk archive."));
        return;
    }

    std::string actualInput;
    bool inputIsVpkWithFilter = false;
    std::string vpkFilter;
    std::string tempVpkFile;
    std::string entryPathInVpk;
    bool isTempVpkFile = false;

    std::string inputEntryOverride;
    if (IsVpkInput(inputPath) && local.inputVpkEntry[0] != '\0') {
        inputEntryOverride = local.inputVpkEntry;
        std::replace(inputEntryOverride.begin(), inputEntryOverride.end(), '\\', '/');
        while (!inputEntryOverride.empty() && inputEntryOverride.front() == '/') inputEntryOverride.erase(inputEntryOverride.begin());
    }

    bool needsFilePass = local.vpkDecompile || local.gltfExportFormat != 0 || local.vpkList || local.vpkDir;
    bool needsEnchantments = local.enchantmentFlags != 0 &&
        ((!IsVpkInput(inputPath) && IsModelInput(inputPath)) ||
         (IsVpkInput(inputPath) && !inputEntryOverride.empty() && IsModelInput(inputEntryOverride)));
    if (needsEnchantments) {
        needsFilePass = true;
    }

    std::string vpkPath = steam.vpkPath;

    if (IsVpkInput(inputPath)) {
        actualInput = inputPath;
        if (!inputEntryOverride.empty()) {
            inputIsVpkWithFilter = true;
            vpkFilter = inputEntryOverride;
            entryPathInVpk = inputEntryOverride;
            logLine(OBF_CSTR("Using VPK archive input: ") + actualInput + OBF_CSTR(" with entry filter: ") + vpkFilter);
        }
        else {
            inputIsVpkWithFilter = false;
            logLine(OBF_CSTR("Using VPK archive input: ") + actualInput);
        }
    }
    else if (fs::exists(inputPath)) {
        actualInput = inputPath;
        inputIsVpkWithFilter = false;
        logLine(OBF_CSTR("Using disk input: ") + actualInput);
    }
    else {
        std::string compiledPath = rawInput;
        if (IsModelInput(rawInput)) {
            fs::path p(rawInput);
            p.replace_extension(OBF_CSTR(".vmdl_c"));
            compiledPath = p.string();
        }
        std::replace(compiledPath.begin(), compiledPath.end(), '\\', '/');

        auto validResult = VPKManager::GetInstance().isValid(vpkPath, compiledPath);
        if (!validResult.IsOk() || !validResult.Value()) {
            logLine(OBF_CSTR("Input not found on disk or in VPK: ") + rawInput);
            finish(false, OBF_CSTR("Input file not found on disk or in VPK: ") + rawInput);
            return;
        }

        entryPathInVpk = compiledPath;
        actualInput = vpkPath;
        inputIsVpkWithFilter = true;
        vpkFilter = entryPathInVpk;
    }

    std::string outputPath = BuildOutputPath(outputDir, inputPath);
    std::error_code ec;
    fs::create_directories(outputPath, ec);
    if (ec) {
        logLine(OBF_CSTR("Failed to create output directory: ") + ec.message());
        finish(false, OBF_CSTR("Failed to create output directory: ") + ec.message());
        return;
    }
    logLine(OBF_CSTR("Output directory: ") + outputPath);

    if (!IsVpkInput(inputPath) && fs::exists(inputPath) && needsFilePass) {
        fs::path inputFs(inputPath);
        std::string entryName = inputFs.filename().string();
        std::replace(entryName.begin(), entryName.end(), '\\', '/');
        tempVpkFile = GetTempPathInProject(OBF_CSTR("temp_input_"), OBF_CSTR(".vpk")).string();

        auto createResult = VPKManager::GetInstance().CreateVPK(tempVpkFile, {});
        if (createResult.IsErr()) {
            logLine(OBF_CSTR("Failed to create temporary VPK: ") + createResult.Error());
            finish(false, OBF_CSTR("Failed to create temporary VPK: ") + createResult.Error());
            return;
        }
        auto addResult = VPKManager::GetInstance().AddFileToVPK(tempVpkFile, entryName, inputPath);
        if (addResult.IsErr()) {
            logLine(OBF_CSTR("Failed to add disk file to temporary VPK: ") + addResult.Error());
            finish(false, OBF_CSTR("Failed to add disk file to temporary VPK: ") + addResult.Error());
            return;
        }
        auto flushResult = VPKManager::GetInstance().FlushCacheEntry(tempVpkFile);
        if (flushResult.IsErr()) {
            logLine(OBF_CSTR("Failed to bake temporary VPK: ") + flushResult.Error());
            finish(false, OBF_CSTR("Failed to bake temporary VPK: ") + flushResult.Error());
            return;
        }
        isTempVpkFile = true;
        logLine(OBF_CSTR("Packed disk file into temporary VPK: ") + tempVpkFile);
        logLine(OBF_CSTR("VPK entry name: ") + entryName);
    }

    VRF::GetInstance().TerminateLingeringDecompilerProcesses();

    std::string combinedOutput;
    bool filesOk = true;

    if (needsFilePass) {
        setStatus(0.50f, OBF_CSTR("Running Source2Viewer-CLI..."));
        DecompilerConfig fileConfig = local;
        if (needsEnchantments) {
            fileConfig.vpkDecompile = true;
        }
        std::string fileArgs;
        if (isTempVpkFile && !tempVpkFile.empty()) {
            std::string entryName = fs::path(inputPath).filename().string();
            std::replace(entryName.begin(), entryName.end(), '\\', '/');
            fileArgs = BuildFilesCommand(fileConfig, tempVpkFile, outputPath, true, entryName);
        }
        else {
            fileArgs = BuildFilesCommand(fileConfig, actualInput, outputPath, inputIsVpkWithFilter, vpkFilter);
        }
        std::cout << OBF_CSTR("[VRF] ") << fileArgs << std::endl;
        logLine(OBF_CSTR("CLI arguments: ") + fileArgs);

        std::string fileOutput;
        filesOk = VRF::GetInstance().DecompileWithOutput(fileArgs, fileOutput);
        if (!fileOutput.empty()) {
            if (!combinedOutput.empty()) combinedOutput += "\n";
            combinedOutput += OBF_CSTR("=== Source2Viewer-CLI output ===\n") + fileOutput;
        }
    }

    if (!combinedOutput.empty()) {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->lastDecompilerOutput.empty()) state->lastDecompilerOutput += "\n";
        state->lastDecompilerOutput += combinedOutput;
    }

    setStatus(0.70f, OBF_CSTR("Writing decompiler log..."));
    try {
        fs::path logDir = fs::path(outputPath);
        fs::create_directories(logDir, ec);
        fs::path logPath = logDir / OBF_CSTR("decompiler_output.txt");
        std::ofstream logFile(logPath);
        if (logFile.is_open()) {
            logFile << "Input: " << inputPath << "\n";
            logFile << "Output: " << outputPath << "\n";
            if (!tempVpkFile.empty()) logFile << "Temp VPK file: " << tempVpkFile << "\n";
            logFile << "\n=== Source2Viewer-CLI output ===\n";
            logFile << combinedOutput;
            logFile.close();
            logLine(OBF_CSTR("Decompiler log written to: ") + logPath.string());
        }
        else {
            logLine(OBF_CSTR("Failed to open decompiler log file: ") + logPath.string());
        }
    }
    catch (const std::exception& e) {
        logLine(OBF_CSTR("Failed to write decompiler log: ") + std::string(e.what()));
    }

    if (needsFilePass && !IsVpkInput(inputPath)) {
        setStatus(0.71f, OBF_CSTR("Flattening output..."));
        FlattenOutputIfSingleFile(outputPath, inputPath);
        logLine(OBF_CSTR("Output flattened to: ") + outputPath);
    }

    if (needsEnchantments) {
        setStatus(0.75f, OBF_CSTR("Preparing model for enchantments..."));
        std::string rawForEnhance = entryPathInVpk.empty() ? rawInput : entryPathInVpk;
        std::string modelRelPath = (inputIsVpkWithFilter && !entryPathInVpk.empty())
            ? entryPathInVpk
            : fs::path(inputPath).filename().string();
        fs::path vmdlPath = FindDecompiledVmdl(outputPath, modelRelPath);
        if (vmdlPath.empty()) {
            logLine(OBF_CSTR("Could not locate .vmdl file for enchantments."));
        }
        else {
            logLine(OBF_CSTR("Target .vmdl for enchantments: ") + vmdlPath.string());
            if (local.enchantmentFlags & decompiler_enhancements::EnchantmentFlags::RestoreMorphs) {
                setStatus(0.78f, OBF_CSTR("Restoring morphs (facial flex)..."));
                std::string morphLog;
                bool morphOk = ApplyMorphMergeToOutput(outputPath, vmdlPath, rawForEnhance, actualInput, inputIsVpkWithFilter, vpkFilter, morphLog);
                logLine(morphOk ? OBF_CSTR("Morph OK: ") + morphLog : OBF_CSTR("Morph failed: ") + morphLog);
            }
            setStatus(0.82f, OBF_CSTR("Extracting ASEQ block from compiled model..."));
            std::string aseqBlock;
            std::string aseqLog;
            bool aseqOk = ExtractAseqBlock(rawForEnhance, actualInput, inputIsVpkWithFilter, vpkFilter, tempVpkFile, aseqBlock, aseqLog);
            if (!aseqLog.empty()) {
                logLine(aseqLog);
            }
            if (!aseqOk || aseqBlock.empty()) {
                logLine(OBF_CSTR("Could not extract ASEQ block; skipping enchantments."));
            }
            else {
                setStatus(0.92f, OBF_CSTR("Applying decompiler enchantments..."));
                std::string enhanceLog;
                bool enhanceOk = decompiler_enhancements::ApplyEnhancements(vmdlPath.string(), "", aseqBlock, local.enchantmentFlags, enhanceLog);
                logLine(enhanceOk ? OBF_CSTR("Enhancement: ") + enhanceLog : OBF_CSTR("Enhancement failed: ") + enhanceLog);
            }
        }
    }

    setStatus(0.97f, OBF_CSTR("Cleaning up temporary files..."));
    if (isTempVpkFile) {
        VPKManager::GetInstance().ClearCacheEntry(tempVpkFile);
        std::error_code rmEc;
        for (int i = 0; i < 10; ++i) {
            fs::remove(tempVpkFile, rmEc);
            if (!rmEc) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (!rmEc) logLine(OBF_CSTR("Temp VPK file removed."));
        else logLine(OBF_CSTR("Failed to remove temp VPK file: ") + rmEc.message());
    }

    size_t outputFileCount = 0;
    std::string firstFile;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(outputPath, ec)) {
            if (entry.is_regular_file()) {
                ++outputFileCount;
                if (firstFile.empty()) firstFile = entry.path().string();
            }
        }
    }
    catch (const std::exception& e) {
        logLine(OBF_CSTR("Failed to enumerate output directory: ") + std::string(e.what()));
    }
    logLine(OBF_CSTR("Output directory file count: ") + std::to_string(outputFileCount));
    if (!firstFile.empty()) logLine(OBF_CSTR("First output file: ") + firstFile);

    if (!filesOk) {
        finish(false, OBF_CSTR("Source2Viewer-CLI reported an error. See decompiler output below."));
        return;
    }

    if (outputFileCount == 0) {
        finish(false, OBF_CSTR("Decompilation produced no files. The output folder is empty."));
        return;
    }

    finish(true, OBF_CSTR("Decompiled to: ") + outputPath);
}

void RunModelHelperJob(std::shared_ptr<DecompilerState> state) {
    SH_AD_GUI();

    std::string vmdlPath;
    std::vector<std::string> modifiers;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        vmdlPath = state->config.modelHelperVmdlPath;
        modifiers = state->modelHelperModifiers;
    }

    auto finish = [state](bool ok, const std::string& msg) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->busy = false;
        state->progress = ok ? 1.0f : 0.0f;
        state->status = msg;
        state->showStatus = true;
        state->lastError = ok ? "" : msg;
        g_modelHelperRunning.store(false);
    };

    std::string resolvedPath = ResolveProjectRelativePath(vmdlPath).string();
    if (!fs::exists(resolvedPath)) {
        finish(false, OBF_CSTR("VMDL file not found: ") + resolvedPath);
        return;
    }

    auto result = model_helper::ApplyActivityModifiers(resolvedPath, modifiers);
    finish(result.success, result.message);
}

bool DecompileTargetForMerge(const std::string& targetPath,
                             const std::string& vpkEntry,
                             std::string& outVmdlPath,
                             std::string& outSearchRoot,
                             fs::path& outTempDir,
                             fs::path& outTempVpk,
                             const std::function<void(const std::string&)>& logLine,
                             std::string& outError) {
    std::error_code ec;
    outTempDir = GetTempPathInProject(OBF_CSTR("merge_target_"), OBF_CSTR(""));
    fs::create_directories(outTempDir, ec);

    std::string lowerTarget = targetPath;
    std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(),
        [](unsigned char c) { return static_cast<char>(::tolower(c)); });
    bool isVpk = lowerTarget.size() >= 4 && lowerTarget.substr(lowerTarget.size() - 4) == OBF_CSTR(".vpk");

    std::string decompileInput;
    std::string filterEntry;
    std::string modelRelForSearch;
    bool inputIsVpkWithFilter = false;

    if (isVpk) {
        std::string entry = vpkEntry;
        std::replace(entry.begin(), entry.end(), '\\', '/');
        while (!entry.empty() && entry.front() == '/') entry.erase(entry.begin());
        if (entry.empty()) {
            outError = OBF_CSTR("Path inside VPK is empty.");
            return false;
        }

        std::vector<std::string> candidates = { entry };
        std::string lowerEntry = entry;
        std::transform(lowerEntry.begin(), lowerEntry.end(), lowerEntry.begin(),
            [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        if (lowerEntry.size() >= 5 && lowerEntry.substr(lowerEntry.size() - 5) == OBF_CSTR(".vmdl")) {
            candidates.push_back(entry + OBF_CSTR("_c"));
        }
        if (lowerEntry.size() < 7 || lowerEntry.substr(lowerEntry.size() - 7) != OBF_CSTR(".vmdl_c")) {
            candidates.push_back(entry + OBF_CSTR(".vmdl_c"));
        }

        bool found = false;
        for (const auto& candidate : candidates) {
            auto valid = VPKManager::GetInstance().isValid(targetPath, candidate);
            if (valid.IsOk() && valid.Value()) {
                filterEntry = candidate;
                found = true;
                break;
            }
        }
        if (!found) {
            outError = OBF_CSTR("Entry not found inside VPK: ") + entry;
            return false;
        }
        decompileInput = targetPath;
        modelRelForSearch = filterEntry;
        inputIsVpkWithFilter = true;
        logLine(OBF_CSTR("Target is a VPK entry: ") + filterEntry);
    }
    else {
        // Compiled file on disk: pack into a temporary single-file VPK (same as the decompiler does).
        std::string entryName = fs::path(targetPath).filename().string();
        std::replace(entryName.begin(), entryName.end(), '\\', '/');
        outTempVpk = GetTempPathInProject(OBF_CSTR("temp_merge_"), OBF_CSTR(".vpk"));

        auto createResult = VPKManager::GetInstance().CreateVPK(outTempVpk.string(), {});
        if (createResult.IsErr()) {
            outError = OBF_CSTR("Failed to create temporary VPK: ") + createResult.Error();
            return false;
        }
        auto addResult = VPKManager::GetInstance().AddFileToVPK(outTempVpk.string(), entryName, targetPath);
        if (addResult.IsErr()) {
            outError = OBF_CSTR("Failed to add target to temporary VPK: ") + addResult.Error();
            return false;
        }
        auto flushResult = VPKManager::GetInstance().FlushCacheEntry(outTempVpk.string());
        if (flushResult.IsErr()) {
            outError = OBF_CSTR("Failed to bake temporary VPK: ") + flushResult.Error();
            return false;
        }
        decompileInput = outTempVpk.string();
        filterEntry = entryName;
        modelRelForSearch = entryName;
        inputIsVpkWithFilter = true;
        logLine(OBF_CSTR("Packed compiled target into temporary VPK."));
    }

    std::string args = OBF_CSTR("-i ") + QuoteCmdArg(decompileInput) + OBF_CSTR(" -o ") + QuoteCmdArg(outTempDir.string()) +
        OBF_CSTR(" -f ") + QuoteCmdArg(filterEntry) + OBF_CSTR(" -d");
    logLine(OBF_CSTR("Decompiling target with all enchantments..."));
    std::cout << OBF_CSTR("[VRF] ") << args << std::endl;
    std::string cliOutput;
    if (!VRF::GetInstance().DecompileWithOutput(args, cliOutput)) {
        outError = OBF_CSTR("Target decompilation failed.\n") + cliOutput;
        return false;
    }

    if (!isVpk) {
        FlattenOutputIfSingleFile(outTempDir.string(), targetPath);
    }

    fs::path vmdlPath = FindDecompiledVmdl(outTempDir.string(), modelRelForSearch);
    if (vmdlPath.empty()) {
        outError = OBF_CSTR("Could not locate decompiled target .vmdl.");
        return false;
    }
    logLine(OBF_CSTR("Decompiled target: ") + vmdlPath.string());

    constexpr int kAllEnchantments =
        decompiler_enhancements::EnchantmentFlags::FixAnimationOrder |
        decompiler_enhancements::EnchantmentFlags::RestoreActivityModifiers |
        decompiler_enhancements::EnchantmentFlags::RestoreWeightLists |
        decompiler_enhancements::EnchantmentFlags::RestoreBlendAnimations |
        decompiler_enhancements::EnchantmentFlags::RestoreMorphs;

    std::string rawInputForMorph = isVpk ? filterEntry : targetPath;
    std::string actualInputForMorph = isVpk ? targetPath : decompileInput;

    std::string morphLog;
    bool morphOk = ApplyMorphMergeToOutput(outTempDir.string(), vmdlPath, rawInputForMorph, actualInputForMorph,
        inputIsVpkWithFilter, filterEntry, morphLog);
    logLine(morphOk ? OBF_CSTR("Morph OK: ") + morphLog : OBF_CSTR("Morph restore: ") + morphLog);

    std::string aseqBlock;
    std::string aseqLog;
    bool aseqOk = ExtractAseqBlock(rawInputForMorph, actualInputForMorph, inputIsVpkWithFilter, filterEntry,
        outTempVpk.string(), aseqBlock, aseqLog);
    if (!aseqLog.empty()) logLine(aseqLog);
    if (!aseqOk || aseqBlock.empty()) {
        logLine(OBF_CSTR("Could not extract ASEQ block; skipping vmdl enchantments."));
    }
    else {
        std::string enhanceLog;
        bool enhanceOk = decompiler_enhancements::ApplyEnhancements(vmdlPath.string(), "", aseqBlock, kAllEnchantments, enhanceLog);
        logLine(enhanceOk ? OBF_CSTR("Enhancement: ") + enhanceLog : OBF_CSTR("Enhancement failed: ") + enhanceLog);
    }

    outVmdlPath = vmdlPath.string();
    outSearchRoot = outTempDir.string();
    return true;
}

bool PrepareTargetVmdl(const std::string& targetRaw,
                       const std::string& vpkEntry,
                       const std::function<void(const std::string&)>& logLine,
                       const std::function<void(float, const std::string&)>& setStatus,
                       std::string& outTargetVmdl,
                       std::string& outSearchRoot,
                       fs::path& outTempDir,
                       fs::path& outTempVpk,
                       std::string& outError) {
    if (targetRaw.empty()) {
        outError = OBF_CSTR("Target model path is empty.");
        return false;
    }
    std::string targetPath = ResolveProjectRelativePath(targetRaw).string();
    if (!fs::exists(targetPath)) {
        outError = OBF_CSTR("Target file not found: ") + targetPath;
        return false;
    }

    std::string lowerTarget = targetPath;
    std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(),
        [](unsigned char c) { return static_cast<char>(::tolower(c)); });
    auto hasExt = [&](const char* ext) {
        size_t len = std::strlen(ext);
        return lowerTarget.size() >= len && lowerTarget.substr(lowerTarget.size() - len) == ext;
    };

    if (hasExt(".vmdl")) {
        outTargetVmdl = targetPath;
        // The search root is only used to resolve target refs; prefer the target's content root.
        std::string tp = targetPath;
        std::string low = tp;
        std::transform(low.begin(), low.end(), low.begin(),
            [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        size_t pos = std::string::npos;
        for (const char* pat : { "\\models\\", "/models/" }) {
            size_t q = low.rfind(pat);
            if (q != std::string::npos && (pos == std::string::npos || q > pos)) pos = q;
        }
        if (pos != std::string::npos) {
            outSearchRoot = tp.substr(0, pos);
        }
        else {
            outSearchRoot = fs::path(tp).parent_path().string();
        }
        logLine(OBF_CSTR("Target is a source .vmdl on disk."));
        return true;
    }

    if (hasExt(".vmdl_c") || hasExt(".vpk")) {
        if (VRF::IsSetupNeeded()) {
            setStatus(0.10f, OBF_CSTR("Downloading Source2Viewer-CLI..."));
            if (!VRF::GetInstance().Setup()) {
                outError = OBF_CSTR("Failed to download or extract Source2Viewer-CLI.");
                return false;
            }
        }
        VRF::GetInstance().TerminateLingeringDecompilerProcesses();
        setStatus(0.30f, OBF_CSTR("Decompiling target model..."));
        if (!DecompileTargetForMerge(targetPath, vpkEntry, outTargetVmdl, outSearchRoot,
                outTempDir, outTempVpk, logLine, outError)) {
            if (!outTempVpk.empty()) {
                VPKManager::GetInstance().ClearCacheEntry(outTempVpk.string());
                std::error_code rmEc;
                fs::remove(outTempVpk, rmEc);
            }
            if (!outTempDir.empty()) {
                std::error_code rmEc;
                fs::remove_all(outTempDir, rmEc);
            }
            return false;
        }
        return true;
    }

    outError = OBF_CSTR("Target must be a .vmdl, .vmdl_c or .vpk file.");
    return false;
}

void CleanupTempArtifacts(fs::path& tempDir, fs::path& tempVpk) {
    if (!tempVpk.empty()) {
        VPKManager::GetInstance().ClearCacheEntry(tempVpk.string());
        std::error_code rmEc;
        for (int i = 0; i < 10; ++i) {
            fs::remove(tempVpk, rmEc);
            if (!rmEc) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    if (!tempDir.empty()) {
        std::error_code rmEc;
        fs::remove_all(tempDir, rmEc);
    }
}

void RunMergeMeshesJob(std::shared_ptr<DecompilerState> state) {
    SH_AD_GUI();

    DecompilerConfig local;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        local = state->config;
        state->lastDecompilerOutput.clear();
    }

    auto logLine = [state](const std::string& line) {
        std::cout << OBF_CSTR("[MergeMeshes] ") << line << std::endl;
        if (state) {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->lastDecompilerOutput.empty()) state->lastDecompilerOutput += "\n";
            state->lastDecompilerOutput += line;
        }
    };

    auto setStatus = [state](float p, const std::string& msg) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->progress = p;
        state->status = msg;
    };

    auto finish = [state](bool ok, const std::string& msg) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->busy = false;
        state->progress = ok ? 1.0f : 0.0f;
        state->status = msg;
        state->showStatus = true;
        state->lastError = ok ? "" : msg;
        g_modelHelperRunning.store(false);
    };

    setStatus(0.05f, OBF_CSTR("Validating inputs..."));
    std::string sourcePath = ResolveProjectRelativePath(local.modelHelperVmdlPath).string();
    if (!fs::exists(sourcePath)) {
        finish(false, OBF_CSTR("Source VMDL file not found: ") + sourcePath);
        return;
    }

    std::string targetVmdl;
    std::string targetSearchRoot;
    fs::path tempDir;
    fs::path tempVpk;
    std::string targetError;
    if (!PrepareTargetVmdl(local.mergeTargetPath, local.mergeTargetVpkEntry, logLine, setStatus,
                           targetVmdl, targetSearchRoot, tempDir, tempVpk, targetError)) {
        finish(false, targetError);
        return;
    }

    setStatus(0.80f, OBF_CSTR("Merging meshes..."));
    model_helper::MergeMeshesOptions options;
    options.sourceVmdlPath = sourcePath;
    options.targetVmdlPath = targetVmdl;
    options.targetSearchRoot = targetSearchRoot;
    options.outputVmdlPath = sourcePath;

    auto result = model_helper::MergeMeshes(options);
    if (!result.log.empty()) logLine(result.log);

    setStatus(0.95f, OBF_CSTR("Cleaning up..."));
    CleanupTempArtifacts(tempDir, tempVpk);

    finish(result.success, result.message);
}

void RunTransferAnimationsJob(std::shared_ptr<DecompilerState> state) {
    SH_AD_GUI();

    DecompilerConfig local;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        local = state->config;
        state->lastDecompilerOutput.clear();
    }

    auto logLine = [state](const std::string& line) {
        std::cout << OBF_CSTR("[TransferAnimations] ") << line << std::endl;
        if (state) {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->lastDecompilerOutput.empty()) state->lastDecompilerOutput += "\n";
            state->lastDecompilerOutput += line;
        }
    };

    auto setStatus = [state](float p, const std::string& msg) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->progress = p;
        state->status = msg;
    };

    auto finish = [state](bool ok, const std::string& msg) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->busy = false;
        state->progress = ok ? 1.0f : 0.0f;
        state->status = msg;
        state->showStatus = true;
        state->lastError = ok ? "" : msg;
        g_modelHelperRunning.store(false);
    };

    setStatus(0.05f, OBF_CSTR("Validating inputs..."));
    std::string sourcePath = ResolveProjectRelativePath(local.modelHelperVmdlPath).string();
    if (!fs::exists(sourcePath)) {
        finish(false, OBF_CSTR("Source VMDL file not found: ") + sourcePath);
        return;
    }

    std::string targetVmdl;
    std::string targetSearchRoot;
    fs::path tempDir;
    fs::path tempVpk;
    std::string targetError;
    if (!PrepareTargetVmdl(local.transferTargetPath, local.transferTargetVpkEntry, logLine, setStatus,
                           targetVmdl, targetSearchRoot, tempDir, tempVpk, targetError)) {
        finish(false, targetError);
        return;
    }

    setStatus(0.80f, OBF_CSTR("Transferring animations..."));
    model_helper::TransferAnimationsOptions options;
    options.sourceVmdlPath = sourcePath;
    options.targetVmdlPath = targetVmdl;
    options.targetSearchRoot = targetSearchRoot;

    auto result = model_helper::TransferAnimations(options);
    if (!result.log.empty()) logLine(result.log);

    setStatus(0.95f, OBF_CSTR("Cleaning up..."));
    CleanupTempArtifacts(tempDir, tempVpk);

    finish(result.success, result.message);
}

} // namespace

namespace decompiler_ui {

void LoadDecompilerState(DecompilerState& state) {
    try {
        fs::path settingsPath = GetSettingsPath();
        if (!fs::exists(settingsPath)) {
            return;
        }
        std::ifstream file(settingsPath);
        if (!file.is_open()) return;
        json j;
        file >> j;

        LoadString(state.config.inputPath, sizeof(state.config.inputPath), j, "inputPath");
        LoadString(state.config.outputDir, sizeof(state.config.outputDir), j, "outputDir");
        LoadString(state.config.inputVpkEntry, sizeof(state.config.inputVpkEntry), j, "inputVpkEntry");

        if (j.contains("recursive")) state.config.recursive = j["recursive"].get<bool>();
        if (j.contains("recursiveVpk")) state.config.recursiveVpk = j["recursiveVpk"].get<bool>();
        LoadString(state.config.vpkExtensions, sizeof(state.config.vpkExtensions), j, "vpkExtensions");
        LoadString(state.config.vpkFilepath, sizeof(state.config.vpkFilepath), j, "vpkFilepath");
        if (j.contains("vpkCache")) state.config.vpkCache = j["vpkCache"].get<bool>();
        if (j.contains("vpkVerify")) state.config.vpkVerify = j["vpkVerify"].get<bool>();

        if (j.contains("allBlocks")) state.config.allBlocks = j["allBlocks"].get<bool>();
        LoadString(state.config.blockName, sizeof(state.config.blockName), j, "blockName");
        if (j.contains("vpkDecompile")) state.config.vpkDecompile = j["vpkDecompile"].get<bool>();
        if (j.contains("textureDecodeFlags")) state.config.textureDecodeFlags = j["textureDecodeFlags"].get<int>();
        if (j.contains("vpkList")) state.config.vpkList = j["vpkList"].get<bool>();
        if (j.contains("vpkDir")) state.config.vpkDir = j["vpkDir"].get<bool>();

        if (j.contains("gltfExportFormat")) state.config.gltfExportFormat = j["gltfExportFormat"].get<int>();
        if (j.contains("gltfExportMaterials")) state.config.gltfExportMaterials = j["gltfExportMaterials"].get<bool>();
        if (j.contains("gltfExportAnimations")) state.config.gltfExportAnimations = j["gltfExportAnimations"].get<bool>();
        if (j.contains("gltfTexturesAdapt")) state.config.gltfTexturesAdapt = j["gltfTexturesAdapt"].get<bool>();
        if (j.contains("gltfExportExtras")) state.config.gltfExportExtras = j["gltfExportExtras"].get<bool>();
        LoadString(state.config.gltfMeshList, sizeof(state.config.gltfMeshList), j, "gltfMeshList");
        LoadString(state.config.gltfAnimationList, sizeof(state.config.gltfAnimationList), j, "gltfAnimationList");

        if (j.contains("toolsAssetInfoShort")) state.config.toolsAssetInfoShort = j["toolsAssetInfoShort"].get<bool>();
        if (j.contains("threads")) state.config.threads = j["threads"].get<int>();
        if (j.contains("enchantmentFlags")) state.config.enchantmentFlags = j["enchantmentFlags"].get<int>();

        LoadString(state.config.modelHelperVmdlPath, sizeof(state.config.modelHelperVmdlPath), j, "modelHelperVmdlPath");
        if (j.contains("modelHelperModifiers") && j["modelHelperModifiers"].is_array()) {
            state.modelHelperModifiers.clear();
            for (const auto& item : j["modelHelperModifiers"]) {
                if (item.is_string()) state.modelHelperModifiers.push_back(item.get<std::string>());
            }
        }

        LoadString(state.config.mergeTargetPath, sizeof(state.config.mergeTargetPath), j, "mergeTargetPath");
        LoadString(state.config.mergeTargetVpkEntry, sizeof(state.config.mergeTargetVpkEntry), j, "mergeTargetVpkEntry");

        LoadString(state.config.transferTargetPath, sizeof(state.config.transferTargetPath), j, "transferTargetPath");
        LoadString(state.config.transferTargetVpkEntry, sizeof(state.config.transferTargetVpkEntry), j, "transferTargetVpkEntry");
    }

catch (const std::exception& e) {
    std::cerr << "LoadDecompilerState failed: " << e.what() << std::endl;
}
}

void SaveDecompilerState(const DecompilerState& state) {
    try {
        fs::path settingsPath = GetSettingsPath();
        fs::create_directories(settingsPath.parent_path());
        json j;

        SaveString(j, "inputPath", state.config.inputPath);
        SaveString(j, "outputDir", state.config.outputDir);
        SaveString(j, "inputVpkEntry", state.config.inputVpkEntry);

        j["recursive"] = state.config.recursive;
        j["recursiveVpk"] = state.config.recursiveVpk;
        SaveString(j, "vpkExtensions", state.config.vpkExtensions);
        SaveString(j, "vpkFilepath", state.config.vpkFilepath);
        j["vpkCache"] = state.config.vpkCache;
        j["vpkVerify"] = state.config.vpkVerify;

        j["allBlocks"] = state.config.allBlocks;
        SaveString(j, "blockName", state.config.blockName);
        j["vpkDecompile"] = state.config.vpkDecompile;
        j["textureDecodeFlags"] = state.config.textureDecodeFlags;
        j["vpkList"] = state.config.vpkList;
        j["vpkDir"] = state.config.vpkDir;

        j["gltfExportFormat"] = state.config.gltfExportFormat;
        j["gltfExportMaterials"] = state.config.gltfExportMaterials;
        j["gltfExportAnimations"] = state.config.gltfExportAnimations;
        j["gltfTexturesAdapt"] = state.config.gltfTexturesAdapt;
        j["gltfExportExtras"] = state.config.gltfExportExtras;
        SaveString(j, "gltfMeshList", state.config.gltfMeshList);
        SaveString(j, "gltfAnimationList", state.config.gltfAnimationList);

        j["toolsAssetInfoShort"] = state.config.toolsAssetInfoShort;
        j["threads"] = state.config.threads;
        j["enchantmentFlags"] = state.config.enchantmentFlags;

        SaveString(j, "modelHelperVmdlPath", state.config.modelHelperVmdlPath);
        j["modelHelperModifiers"] = state.modelHelperModifiers;

        SaveString(j, "mergeTargetPath", state.config.mergeTargetPath);
        SaveString(j, "mergeTargetVpkEntry", state.config.mergeTargetVpkEntry);

        SaveString(j, "transferTargetPath", state.config.transferTargetPath);
        SaveString(j, "transferTargetVpkEntry", state.config.transferTargetVpkEntry);

        std::ofstream file(settingsPath);
        if (file.is_open()) {
            file << j.dump(2);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "SaveDecompilerState failed: " << e.what() << std::endl;
    }
}

static void BeginCard(const char* label, float height = 0.0f)
{
    ImGui::Spacing();
    ImGui::BeginChild(label, ImVec2(0.0f, height), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.30f, 0.22f, 1.00f));
    ImGui::SeparatorText(label);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

static void EndCard()
{
    ImGui::Spacing();
    ImGui::EndChild();
}

static void DrawHeader()
{
    SteamManager& steam = SteamManager::GetInstance();
    bool hasPath = steam.HasValidPath();
    std::string path = hasPath ? steam.getDotaPath() : OBF_CSTR("not configured");

    ImVec2 fullSize = ImGui::GetContentRegionAvail();
    ImGui::BeginChild(OBF_CSTR("##header"), ImVec2(fullSize.x, 56.0f), ImGuiChildFlags_Borders);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.30f, 0.22f, 1.00f));
    ImGui::TextUnformatted(OBF_CSTR("Dota2ModHelper"));
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled(OBF_CSTR("· VRF / Model Decompiler"));

    float avail = ImGui::GetContentRegionAvail().x;
    const char* pathLabel = OBF_CSTR("Dota 2 path:");
    float lineWidth = ImGui::CalcTextSize(pathLabel).x + ImGui::CalcTextSize(path.c_str()).x + 8.0f;
    //ImGui::SetCursorPosX(ImGui::GetCursorPosX() - lineWidth - 8.0f);
    ImGui::TextUnformatted(pathLabel);
    ImGui::SameLine();
    if (hasPath)
        ImGui::TextColored(ImVec4(0.20f, 0.85f, 0.35f, 1.00f), "%s", path.c_str());
    else
        ImGui::TextColored(ImVec4(0.90f, 0.30f, 0.22f, 1.00f), "%s", path.c_str());

    ImGui::EndChild();
}

static void PathInputWithBrowse(const char* label, const char* hint, const char* buttonId, char* buf, size_t bufSize, bool file, const char* filter = nullptr)
{
    ImGui::TextUnformatted(label);
    float width = ImGui::GetContentRegionAvail().x - 110.0f;
    ImGui::SetNextItemWidth(width);
    ImGui::InputText(hint, buf, bufSize);
    ImGui::SameLine();
    ImGui::PushID(buttonId);
    if (ImGui::Button(OBF_CSTR("Browse..."), ImVec2(100, 0))) {
        std::string picked;
        if (file) {
            picked = OpenFileDialog(GetActiveWindow(), label, filter);
        }
        else {
            picked = OpenFolderDialog(GetActiveWindow(), label);
        }
        if (!picked.empty()) {
            strncpy_s(buf, bufSize, picked.c_str(), _TRUNCATE);
        }
    }
    ImGui::PopID();
}

static void DrawInputOutputCard(std::shared_ptr<DecompilerState> state)
{
    BeginCard(OBF_CSTR("Input / Output"));

    PathInputWithBrowse(
        OBF_CSTR("Input compiled file or VPK path"),
        OBF_CSTR("##input"),
        OBF_CSTR("##browse_input"),
        state->config.inputPath,
        IM_ARRAYSIZE(state->config.inputPath),
        true,
        "Compiled Source 2 (*.vmdl_c;*.vmat_c;*.vtex_c;*.vsnd_c;*.vpcf_c;*.vcs_c;*.vpk)\0"
        "*.vmdl_c;*.vmat_c;*.vtex_c;*.vsnd_c;*.vpcf_c;*.vcs_c;*.vpk\0"
        "All Files (*.*)\0*.*\0");

    {
        std::string inputStr = state->config.inputPath;
        std::transform(inputStr.begin(), inputStr.end(), inputStr.begin(),
            [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        if (inputStr.size() >= 4 && inputStr.substr(inputStr.size() - 4) == ".vpk") {
            ImGui::TextUnformatted(OBF_CSTR("Path inside VPK (optional)"));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputText(OBF_CSTR("##input_vpk_entry"), state->config.inputVpkEntry, IM_ARRAYSIZE(state->config.inputVpkEntry));
            ImGui::TextDisabled(OBF_CSTR("e.g. models/heroes/axe/axe.vmdl_c - passed as the -f filter."));
        }
    }

    PathInputWithBrowse(
        OBF_CSTR("Output directory"),
        OBF_CSTR("##output"),
        OBF_CSTR("##browse_output"),
        state->config.outputDir,
        IM_ARRAYSIZE(state->config.outputDir),
        false,
        nullptr);

    EndCard();
}

static void DrawDecompileTab(std::shared_ptr<DecompilerState> state, bool busy)
{
    DrawInputOutputCard(state);

    BeginCard(OBF_CSTR("Options"));

    if (ImGui::CollapsingHeader(OBF_CSTR("Input / Output"), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox(OBF_CSTR("Recursive folder scan"), &state->config.recursive);
        ImGui::Checkbox(OBF_CSTR("Recursive into VPK archives"), &state->config.recursiveVpk);
        ImGui::InputText(OBF_CSTR("VPK extension filter (-e)"), state->config.vpkExtensions, IM_ARRAYSIZE(state->config.vpkExtensions));
        ImGui::InputText(OBF_CSTR("VPK path filter (-f)"), state->config.vpkFilepath, IM_ARRAYSIZE(state->config.vpkFilepath));
        ImGui::Checkbox(OBF_CSTR("Use cached VPK manifest"), &state->config.vpkCache);
        ImGui::Checkbox(OBF_CSTR("Verify VPK checksums/signatures"), &state->config.vpkVerify);
    }

    if (ImGui::CollapsingHeader(OBF_CSTR("Decompile / Inspect"), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox(OBF_CSTR("Decompile supported resources (-d)"), &state->config.vpkDecompile);

        const char* decodeFlags[] = { "auto", "none", "ForceLDR" };
        ImGui::Combo(OBF_CSTR("Texture decode flags"), &state->config.textureDecodeFlags, decodeFlags, IM_ARRAYSIZE(decodeFlags));

        ImGui::Checkbox(OBF_CSTR("List VPK resources (--vpk_list)"), &state->config.vpkList);
        ImGui::Checkbox(OBF_CSTR("List VPK resources with metadata (--vpk_dir)"), &state->config.vpkDir);
    }

    if (ImGui::CollapsingHeader(OBF_CSTR("glTF export"), nullptr)) {
        const char* formats[] = { "Disabled", "gltf", "glb" };
        ImGui::Combo(OBF_CSTR("glTF export format"), &state->config.gltfExportFormat, formats, IM_ARRAYSIZE(formats));

        if (state->config.gltfExportFormat != 0) {
            ImGui::Checkbox(OBF_CSTR("Export materials"), &state->config.gltfExportMaterials);
            ImGui::Checkbox(OBF_CSTR("Export animations"), &state->config.gltfExportAnimations);
            ImGui::Checkbox(OBF_CSTR("Adapt textures for glTF spec"), &state->config.gltfTexturesAdapt);
            ImGui::Checkbox(OBF_CSTR("Export extra mesh properties"), &state->config.gltfExportExtras);
            ImGui::InputText(OBF_CSTR("Mesh whitelist"), state->config.gltfMeshList, IM_ARRAYSIZE(state->config.gltfMeshList));
            ImGui::InputText(OBF_CSTR("Animation whitelist"), state->config.gltfAnimationList, IM_ARRAYSIZE(state->config.gltfAnimationList));
        }
    }

    if (ImGui::CollapsingHeader(OBF_CSTR("Other"), nullptr)) {
        ImGui::Checkbox(OBF_CSTR("Short tools_asset_info output"), &state->config.toolsAssetInfoShort);
        ImGui::SliderInt(OBF_CSTR("Threads"), &state->config.threads, 1, 16);
    }

    EndCard();

    BeginCard(OBF_CSTR("Enchantments"));
    const auto& enchantments = decompiler_enhancements::GetEnchantments();
    std::string preview;
    for (const auto& e : enchantments) {
        if (state->config.enchantmentFlags & e.flag) {
            if (!preview.empty()) preview += ", ";
            preview += e.label;
        }
    }
    if (preview.empty()) preview = "None";
    if (ImGui::BeginCombo(OBF_CSTR("Active enchantments"), preview.c_str())) {
        for (const auto& e : enchantments) {
            bool selected = (state->config.enchantmentFlags & e.flag) != 0;
            if (ImGui::Selectable(e.label, selected, ImGuiSelectableFlags_DontClosePopups)) {
                if (selected) state->config.enchantmentFlags &= ~e.flag;
                else state->config.enchantmentFlags |= e.flag;
            }
            if (selected) ImGui::SetItemDefaultFocus();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e.description);
        }
        ImGui::EndCombo();
    }
    EndCard();

    BeginCard(OBF_CSTR("Actions"));
    bool canRun = !busy && !g_decompilerRunning.load() && !g_modelHelperRunning.load();
    if (!canRun) ImGui::BeginDisabled();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.20f, 0.15f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.28f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.16f, 0.12f, 1.00f));
    if (ImGui::Button(OBF_CSTR("Run"), ImVec2(140, 0))) {
        if (!SteamManager::GetInstance().HasValidPath()) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->lastError = OBF_CSTR("Dota 2 path is not configured.");
            state->showStatus = true;
        }
        else {
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->busy = true;
                state->showStatus = false;
                state->status = OBF_CSTR("Starting...");
                state->progress = 0.0f;
            }
            g_decompilerRunning.store(true);
            SaveDecompilerState(*state);
            std::thread(RunDecompileJob, state).detach();
        }
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    if (ImGui::Button(OBF_CSTR("Save settings"), ImVec2(140, 0))) {
        SaveDecompilerState(*state);
        std::lock_guard<std::mutex> lock(state->mutex);
        state->status = OBF_CSTR("Settings saved.");
        state->showStatus = true;
    }
    if (!canRun) ImGui::EndDisabled();
    EndCard();
}

static void DrawOutputTab(bool busy, float progress, const std::string& status,
                          const std::string& lastError, bool showStatus, const std::string& output)
{
    BeginCard(OBF_CSTR("Status"));
    if (busy) {
        ImGui::ProgressBar(progress, ImVec2(-1, 0));
        ImGui::Text("%s", status.c_str());
    }
    else if (showStatus) {
        if (!lastError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", status.c_str());
        }
        else {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", status.c_str());
        }
    }
    else {
        ImGui::TextDisabled(OBF_CSTR("No active job. Switch to the Decompile tab to start."));
    }
    EndCard();

    if (!output.empty()) {
        float remaining = ImGui::GetContentRegionAvail().y - 20.0f;
        BeginCard(OBF_CSTR("Decompiler output"), std::max(150.0f, remaining));
        ImVec2 avail = ImGui::GetContentRegionAvail();
        std::string mutableOutput = output;
        mutableOutput.reserve(mutableOutput.size() + 1);
        ImGui::InputTextMultiline("##decompiler_output",
                                  mutableOutput.data(),
                                  mutableOutput.size(),
                                  avail,
                                  ImGuiInputTextFlags_ReadOnly);
        EndCard();
    }
}

static void DrawModelHelperTab(std::shared_ptr<DecompilerState> state, bool busy)
{
    BeginCard(OBF_CSTR("Input"));
    PathInputWithBrowse(
        OBF_CSTR("VMDL source file"),
        OBF_CSTR("##model_helper_vmdl"),
        OBF_CSTR("##browse_model_helper_vmdl"),
        state->config.modelHelperVmdlPath,
        IM_ARRAYSIZE(state->config.modelHelperVmdlPath),
        true,
        "VMDL Source (*.vmdl)\0*.vmdl\0All Files (*.*)\0*.*\0");
    EndCard();

    BeginCard(OBF_CSTR("Apply Activity Modifier"));
    ImGui::TextUnformatted(OBF_CSTR("Activity modifiers:"));
    ImGui::TextDisabled(OBF_CSTR("Animations containing all listed modifiers will be copied onto matching target animations."));
    ImGui::Spacing();

    for (size_t i = 0; i < state->modelHelperModifiers.size();) {
        ImGui::PushID(static_cast<int>(i));
        char buf[128];
        strncpy_s(buf, sizeof(buf), state->modelHelperModifiers[i].c_str(), _TRUNCATE);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 50.0f);
        if (ImGui::InputText("##modifier", buf, sizeof(buf))) {
            state->modelHelperModifiers[i] = buf;
        }
        ImGui::SameLine();
        if (ImGui::Button("-", ImVec2(40, 0))) {
            state->modelHelperModifiers.erase(state->modelHelperModifiers.begin() + i);
        }
        else {
            ++i;
        }
        ImGui::PopID();
    }

    if (ImGui::Button(OBF_CSTR("+"), ImVec2(40, 0))) {
        state->modelHelperModifiers.emplace_back();
    }

    ImGui::Spacing();
    bool canRun = !busy && !g_decompilerRunning.load() && !g_modelHelperRunning.load();
    if (!canRun) ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.20f, 0.15f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.28f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.16f, 0.12f, 1.00f));
    if (ImGui::Button(OBF_CSTR("Apply"), ImVec2(140, 0))) {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->busy = true;
            state->showStatus = false;
            state->status = OBF_CSTR("Applying activity modifiers...");
            state->progress = 0.0f;
        }
        g_modelHelperRunning.store(true);
        SaveDecompilerState(*state);
        std::thread(RunModelHelperJob, state).detach();
    }
    ImGui::PopStyleColor(3);
    if (!canRun) ImGui::EndDisabled();
    EndCard();

    BeginCard(OBF_CSTR("Merge Meshes"));
    ImGui::TextDisabled(OBF_CSTR("Merge render meshes, attachments, skeleton and LODs from a target model into the source VMDL."));
    ImGui::TextDisabled(OBF_CSTR(".vmdl_c / .vpk targets are decompiled with all enchantments into a temp folder first."));
    ImGui::Spacing();

    PathInputWithBrowse(
        OBF_CSTR("Target model (.vmdl / .vmdl_c / .vpk)"),
        OBF_CSTR("##merge_target"),
        OBF_CSTR("##browse_merge_target"),
        state->config.mergeTargetPath,
        IM_ARRAYSIZE(state->config.mergeTargetPath),
        true,
        "Model files (*.vmdl;*.vmdl_c;*.vpk)\0*.vmdl;*.vmdl_c;*.vpk\0All Files (*.*)\0*.*\0");

    {
        std::string targetStr = state->config.mergeTargetPath;
        std::transform(targetStr.begin(), targetStr.end(), targetStr.begin(),
            [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        if (targetStr.size() >= 4 && targetStr.substr(targetStr.size() - 4) == ".vpk") {
            ImGui::TextUnformatted(OBF_CSTR("Path inside VPK"));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputText(OBF_CSTR("##merge_target_vpk_entry"), state->config.mergeTargetVpkEntry, IM_ARRAYSIZE(state->config.mergeTargetVpkEntry));
            ImGui::TextDisabled(OBF_CSTR("e.g. models/heroes/axe/axe.vmdl"));
        }
    }

    ImGui::Spacing();
    bool canMerge = !busy && !g_decompilerRunning.load() && !g_modelHelperRunning.load();
    if (!canMerge) ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.20f, 0.15f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.28f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.16f, 0.12f, 1.00f));
    if (ImGui::Button(OBF_CSTR("Merge Meshes"), ImVec2(140, 0))) {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->busy = true;
            state->showStatus = false;
            state->status = OBF_CSTR("Merging meshes...");
            state->progress = 0.0f;
        }
        g_modelHelperRunning.store(true);
        SaveDecompilerState(*state);
        std::thread(RunMergeMeshesJob, state).detach();
    }
    ImGui::PopStyleColor(3);
    if (!canMerge) ImGui::EndDisabled();
    EndCard();

    BeginCard(OBF_CSTR("Transfer Animations"));
    ImGui::TextDisabled(OBF_CSTR("Replace source animations with mirror animations (same activity + activity modifiers) from a target model."));
    ImGui::TextDisabled(OBF_CSTR("Animations without a mirror are renamed to <name>_MISSMATCH across the AnimationList."));
    ImGui::TextDisabled(OBF_CSTR(".vmdl_c / .vpk targets are decompiled with all enchantments into a temp folder first."));
    ImGui::Spacing();

    PathInputWithBrowse(
        OBF_CSTR("Target model (.vmdl / .vmdl_c / .vpk)"),
        OBF_CSTR("##transfer_target"),
        OBF_CSTR("##browse_transfer_target"),
        state->config.transferTargetPath,
        IM_ARRAYSIZE(state->config.transferTargetPath),
        true,
        "Model files (*.vmdl;*.vmdl_c;*.vpk)\0*.vmdl;*.vmdl_c;*.vpk\0All Files (*.*)\0*.*\0");

    {
        std::string targetStr = state->config.transferTargetPath;
        std::transform(targetStr.begin(), targetStr.end(), targetStr.begin(),
            [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        if (targetStr.size() >= 4 && targetStr.substr(targetStr.size() - 4) == ".vpk") {
            ImGui::TextUnformatted(OBF_CSTR("Path inside VPK"));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputText(OBF_CSTR("##transfer_target_vpk_entry"), state->config.transferTargetVpkEntry, IM_ARRAYSIZE(state->config.transferTargetVpkEntry));
            ImGui::TextDisabled(OBF_CSTR("e.g. models/heroes/axe/axe.vmdl"));
        }
    }

    ImGui::Spacing();
    bool canTransfer = !busy && !g_decompilerRunning.load() && !g_modelHelperRunning.load();
    if (!canTransfer) ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.20f, 0.15f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.28f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.16f, 0.12f, 1.00f));
    if (ImGui::Button(OBF_CSTR("Transfer Animations"), ImVec2(140, 0))) {
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->busy = true;
            state->showStatus = false;
            state->status = OBF_CSTR("Transferring animations...");
            state->progress = 0.0f;
        }
        g_modelHelperRunning.store(true);
        SaveDecompilerState(*state);
        std::thread(RunTransferAnimationsJob, state).detach();
    }
    ImGui::PopStyleColor(3);
    if (!canTransfer) ImGui::EndDisabled();
    EndCard();
}

void DrawDecompilerUI(std::shared_ptr<DecompilerState> state) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGuiWindowFlags mainFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    if (!ImGui::Begin(OBF_CSTR("Dota2ModHelper"), nullptr, mainFlags)) {
        ImGui::End();
        return;
    }

    bool busyLocal = false;
    float progressLocal = 0.0f;
    std::string statusLocal;
    std::string lastErrorLocal;
    bool showStatusLocal = false;
    std::string outputLocal;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        busyLocal = state->busy;
        progressLocal = state->progress;
        statusLocal = state->status;
        lastErrorLocal = state->lastError;
        showStatusLocal = state->showStatus;
        outputLocal = state->lastDecompilerOutput;
    }

    DrawHeader();

    static int activeTab = 0;
    ImGui::BeginTabBar(OBF_CSTR("##mainTabs"), ImGuiTabBarFlags_FittingPolicyResizeDown);
    if (ImGui::BeginTabItem(OBF_CSTR("Decompile")))     { activeTab = 0; ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem(OBF_CSTR("Output")))        { activeTab = 1; ImGui::EndTabItem(); }
    if (ImGui::BeginTabItem(OBF_CSTR("Model Helper")))   { activeTab = 2; ImGui::EndTabItem(); }
    ImGui::EndTabBar();

    ImGui::Spacing();

    if (activeTab == 0)
        DrawDecompileTab(state, busyLocal);
    else if (activeTab == 1)
        DrawOutputTab(busyLocal, progressLocal, statusLocal, lastErrorLocal, showStatusLocal, outputLocal);
    else
        DrawModelHelperTab(state, busyLocal);

    ImGui::End();
}

} // namespace decompiler_ui
