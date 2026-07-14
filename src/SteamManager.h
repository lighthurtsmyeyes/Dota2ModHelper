#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <fstream>
#include <windows.h>
#include <shlobj.h>
#include <limits>

namespace fs = std::filesystem;

class SteamManager {
private:
    std::set<std::string> steamPaths;  // Все найденные пути Steam
    std::vector<std::string> dotaPaths; // Все найденные пути Dota 2
    std::string selectedDotaPath;       // Выбранный путь к Dota 2
    std::string selectedSteamPath;      // Соответствующий путь Steam

    // Пути к различным компонентам

public:
    static SteamManager& GetInstance();
    std::string dotaPath;
    std::string vpkPath;
    std::string modPath;
    std::string addonsContentPath;
    std::string addonsGamePath;
    std::string resourceCompiler;

    // Legacy console-based detection (kept for compatibility, not used by GUI).
    void findSteamDirectory();

    // === GUI-friendly path management ===
    // Returns true if a previously saved Dota path was loaded and is still valid.
    bool LoadSavedPath();
    // Saves the currently selected Dota path to disk.
    void SavePath() const;
    // Clears any currently selected paths.
    void ClearPaths();
    // Performs a silent auto-detection of all Dota 2 installations.
    // Returns the list of valid absolute Dota paths (no console I/O).
    std::vector<std::string> FindDotaPathsSilent();
    // Sets the active Dota path and all derived paths. Returns false if path is invalid.
    bool SetDotaPath(const std::string& path);
    // True if a valid Dota path is currently selected.
    bool HasValidPath() const;

    // Методы для получения путей
    std::string getSteamPath() const;
    std::string getDotaPath() const;
    std::string getVpkPath() const;
    std::string getModPath() const;
    std::string getAddonsContentPath() const;
    std::string getAddonsGamePath() const;
    std::string getResourceCompiler() const;

    // Path validation exposed so the GUI can preview manual entries.
    bool validateDotaPath(const std::string& path) const;

private:
    // Вспомогательные методы
    bool validateSteamLibraryPath(const std::string& path);
    bool findInRegistry();
    bool findAdditionalLibraries(const std::string& steamInstallPath);
    bool findInCommonLocations();
    void findAllDotaPaths();
    void selectDotaPathInteractive();

    static fs::path GetSavedPathFile();
};