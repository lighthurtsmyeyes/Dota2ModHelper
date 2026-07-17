// PathValidator.cpp
#include "PathValidator.h"
#include <algorithm>
#include <cctype>


bool PathValidator::IsPathSafe(const fs::path& path, const fs::path& baseDirectory) {
    try {
        auto canonicalPath = GetCanonicalPath(path);
        auto canonicalBase = GetCanonicalPath(baseDirectory);

        return IsWithinDirectory(canonicalPath, canonicalBase);
    }
    catch (const fs::filesystem_error&) {
        // Если canonical не удалось (например, файл не существует),
        // проверяем лексически относительно baseDirectory.
        fs::path resolved = path.is_absolute() ? path.lexically_normal()
                                               : (baseDirectory / path).lexically_normal();
        fs::path baseNormalized = baseDirectory.lexically_normal();
        return IsWithinDirectory(resolved, baseNormalized);
    }
}

fs::path PathValidator::NormalizePath(const fs::path& path) {
    return path.lexically_normal();
}

bool PathValidator::HasValidExtension(
    const fs::path& path,
    const std::vector<std::string>& allowedExtensions
) {
    std::string ext = path.extension().string();

    // РџСЂРёРІРѕРґРёРј Рє РЅРёР¶РЅРµРјСѓ СЂРµРіРёСЃС‚СЂСѓ
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return std::tolower(c); });

    for (const auto& allowed : allowedExtensions) {
        std::string allowedLower = allowed;
        std::transform(allowedLower.begin(), allowedLower.end(),
            allowedLower.begin(), [](unsigned char c) { return std::tolower(c); });

        if (ext == allowedLower) {
            return true;
        }
    }

    return false;
}

std::optional<fs::path> PathValidator::ResolveSafePath(
    const fs::path& userPath,
    const fs::path& baseDirectory
) {
    try {
        fs::path resolved;

        if (userPath.is_absolute()) {
            resolved = GetCanonicalPath(userPath);
        }
        else {
            resolved = GetCanonicalPath(baseDirectory / userPath);
        }

        if (!IsWithinDirectory(resolved, baseDirectory)) {
            return std::nullopt;
        }

        return resolved;
    }
    catch (const fs::filesystem_error&) {
        return std::nullopt;
    }
}

bool PathValidator::IsSymlinkSafe(const fs::path& path) {
    try {
        if (!fs::exists(path)) {
            return true; // Р¤Р°Р№Р» РЅРµ СЃСѓС‰РµСЃС‚РІСѓРµС‚, РЅРµ РїСЂРѕРІРµСЂСЏРµРј
        }

        if (fs::is_symlink(path)) {
            auto target = fs::read_symlink(path);
            auto parentDir = path.parent_path();

            // РџСЂРѕРІРµСЂСЏРµРј, С‡С‚Рѕ СЃСЃС‹Р»РєР° РЅРµ РІРµРґС‘С‚ Р·Р° РїСЂРµРґРµР»С‹ СЂР°Р·СЂРµС€С‘РЅРЅРѕР№ Р·РѕРЅС‹
            return IsPathSafe(target, parentDir);
        }

        return true;
    }
    catch (const fs::filesystem_error&) {
        return false;
    }
}

Result<fs::path, std::string> PathValidator::ValidateVPKPath(const fs::path& path) {
    // РџСЂРѕРІРµСЂРєР° РЅР° РїСѓСЃС‚РѕС‚Сѓ
    if (path.empty()) {
        return Result<fs::path, std::string>::Err("VPK path is empty");
    }

    // РџСЂРѕРІРµСЂРєР° СЂР°СЃС€РёСЂРµРЅРёСЏ
    if (!HasValidExtension(path, { ".vpk" })) {
        return Result<fs::path, std::string>::Err(
            "Invalid VPK extension: " + path.extension().string()
        );
    }

    // РџСЂРѕРІРµСЂРєР° СЃСѓС‰РµСЃС‚РІРѕРІР°РЅРёСЏ
    if (!fs::exists(path)) {
        return Result<fs::path, std::string>::Err("VPK file does not exist: " + path.string());
    }

    // РџСЂРѕРІРµСЂРєР° РЅР° СЃРёРјРІРѕР»РёС‡РµСЃРєРёРµ СЃСЃС‹Р»РєРё
    if (!IsSymlinkSafe(path)) {
        return Result<fs::path, std::string>::Err("VPK path contains unsafe symlink");
    }

    try {
        return Result<fs::path, std::string>::Ok(fs::canonical(path));
    }
    catch (const fs::filesystem_error& e) {
        return Result<fs::path, std::string>::Err(
            "Failed to canonicalize VPK path: " + std::string(e.what())
        );
    }
}

Result<fs::path, std::string> PathValidator::ValidateConfigPath(const fs::path& path) {
    if (path.empty()) {
        return Result<fs::path, std::string>::Err("Config path is empty");
    }

    // РџСЂРѕРІРµСЂРєР° СЂР°СЃС€РёСЂРµРЅРёСЏ
    if (!HasValidExtension(path, { ".cfg", ".json" })) {
        return Result<fs::path, std::string>::Err(
            "Invalid config extension: " + path.extension().string()
        );
    }

    // РџСЂРѕРІРµСЂРєР° РЅР° path traversal
    std::string pathStr = path.string();
    if (pathStr.find("..") != std::string::npos) {
        return Result<fs::path, std::string>::Err("Config path contains '..'");
    }

    // РџСЂРѕРІРµСЂРєР° РёРјРµРЅРё С„Р°Р№Р»Р°
    fs::path filename = path.filename();
    if (filename.string().length() > 255) {
        return Result<fs::path, std::string>::Err("Config filename too long");
    }

    // РџСЂРѕРІРµСЂРєР° РЅР° РЅРµРґРѕРїСѓСЃС‚РёРјС‹Рµ СЃРёРјРІРѕР»С‹
    const std::string invalidChars = "<>:\"/\\|?*";
    for (char c : filename.string()) {
        if (invalidChars.find(c) != std::string::npos) {
            return Result<fs::path, std::string>::Err(
                "Config filename contains invalid character: " + std::string(1, c)
            );
        }
    }

    return Result<fs::path, std::string>::Ok(path);
}

fs::path PathValidator::GetCanonicalPath(const fs::path& path) {
    try {
        return fs::canonical(path);
    }
    catch (const fs::filesystem_error&) {
        return path.lexically_normal();
    }
}

bool PathValidator::IsWithinDirectory(const fs::path& path, const fs::path& directory) {
    try {
        fs::path rel = fs::relative(path, directory);
        std::string relStr = rel.string();
        // Empty relative path means path == directory (allowed)
        if (relStr.empty()) return true;
        // Any ".." component means path escapes the directory
        for (const auto& part : rel) {
            if (part == "..") return false;
        }
        return true;
    }
    catch (const fs::filesystem_error&) {
        // If relative() fails (e.g. different drive letters), fall back to lexical check
        auto normalized = path.lexically_normal();
        auto baseNormalized = directory.lexically_normal();
        std::string baseStr = baseNormalized.string();
        std::string pathStr = normalized.string();
        if (pathStr.size() < baseStr.size()) return false;
        if (pathStr.compare(0, baseStr.size(), baseStr) != 0) return false;
        // Ensure boundary: next char must be path separator or end
        if (pathStr.size() == baseStr.size()) return true;
        char next = pathStr[baseStr.size()];
        return next == '\\' || next == '/';
    }
}