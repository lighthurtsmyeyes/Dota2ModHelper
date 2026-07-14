// PathValidator.h
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include "Result.h"

namespace fs = std::filesystem;

/**
 * @brief Р’Р°Р»РёРґР°С‚РѕСЂ РїСѓС‚РµР№ РґР»СЏ Р·Р°С‰РёС‚С‹ РѕС‚ path traversal Рё symlink Р°С‚Р°Рє
 */
class PathValidator {
public:
    // РџСЂРѕРІРµСЂРєР° РЅР° path traversal (../)
    [[nodiscard]] static bool IsPathSafe(const fs::path& path, const fs::path& baseDirectory);

    // РќРѕСЂРјР°Р»РёР·Р°С†РёСЏ РїСѓС‚Рё
    [[nodiscard]] static fs::path NormalizePath(const fs::path& path);

    // РџСЂРѕРІРµСЂРєР° СЂР°СЃС€РёСЂРµРЅРёСЏ С„Р°Р№Р»Р°
    [[nodiscard]] static bool HasValidExtension(
        const fs::path& path,
        const std::vector<std::string>& allowedExtensions
    );

    // Р‘РµР·РѕРїР°СЃРЅРѕРµ РїРѕР»СѓС‡РµРЅРёРµ С„Р°Р№Р»Р° РёР· РґРёСЂРµРєС‚РѕСЂРёРё
    [[nodiscard]] static std::optional<fs::path> ResolveSafePath(
        const fs::path& userPath,
        const fs::path& baseDirectory
    );

    // РџСЂРѕРІРµСЂРєР° РЅР° СЃРёРјРІРѕР»РёС‡РµСЃРєРёРµ СЃСЃС‹Р»РєРё
    [[nodiscard]] static bool IsSymlinkSafe(const fs::path& path);

    // Р’Р°Р»РёРґР°С†РёСЏ РїСѓС‚Рё Рє VPK С„Р°Р№Р»Сѓ
    [[nodiscard]] static Result<fs::path, std::string> ValidateVPKPath(const fs::path& path);

    // Р’Р°Р»РёРґР°С†РёСЏ РїСѓС‚Рё Рє РєРѕРЅС„РёРіСѓ
    [[nodiscard]] static Result<fs::path, std::string> ValidateConfigPath(const fs::path& path);

private:
    [[nodiscard]] static fs::path GetCanonicalPath(const fs::path& path);
    [[nodiscard]] static bool IsWithinDirectory(const fs::path& path, const fs::path& directory);
};