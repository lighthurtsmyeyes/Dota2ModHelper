#include "SteamManager.h"
#include "Structures.h"


namespace fs = std::filesystem;

SteamManager& SteamManager::GetInstance()
{
    static SteamManager instance;
    return instance;
}

bool SteamManager::validateSteamLibraryPath(const std::string& path) {
    if (path.empty()) return false;

    try {
        fs::path steamLibDir(path);

        if (!fs::exists(steamLibDir) || !fs::is_directory(steamLibDir)) {
            return false;
        }

        // Проверяем наличие папки steamapps/common - это основная характеристика библиотеки Steam
        fs::path steamappsCommon = steamLibDir / "steamapps" / "common";
        if (!fs::exists(steamappsCommon) || !fs::is_directory(steamappsCommon)) {
            return false;
        }

        // Дополнительные проверки для уверенности
        fs::path steamappsDir = steamLibDir / "steamapps";
        if (!fs::exists(steamappsDir) || !fs::is_directory(steamappsDir)) {
            return false;
        }

        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SteamManager::validateDotaPath(const std::string& path) const {
    if (path.empty()) return false;

    try {
        fs::path dotaDir(path);

        // Проверяем наличие основных папок и файлов Dota 2
        if (!fs::exists(dotaDir) || !fs::is_directory(dotaDir)) {
            return false;
        }

        // Проверяем наличие game/dota/pak01_dir.vpk
        fs::path vpkPath = dotaDir / "game" / "dota" / "pak01_dir.vpk";
        if (!fs::exists(vpkPath)) {
            return false;
        }

        // Проверяем наличие папки game/dota_addons
        fs::path addonsPath = dotaDir / "game" / "dota_addons";
        if (!fs::exists(addonsPath) || !fs::is_directory(addonsPath)) {
            return false;
        }

        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SteamManager::findInRegistry() {
    HKEY hKey;
    LONG result;
    DWORD bufferSize = MAX_PATH;
    char steamPath[MAX_PATH];

    std::vector<std::string> registryPaths = {
        "Software\\Valve\\Steam",
        "Software\\Wow6432Node\\Valve\\Steam"
    };

    bool found = false;

    for (const auto& regPath : registryPaths) {
        // Пробуем HKEY_CURRENT_USER
        result = RegOpenKeyExA(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_READ, &hKey);
        if (result == ERROR_SUCCESS) {
            DWORD bufferSize = MAX_PATH;
            result = RegQueryValueExA(hKey, "SteamPath", NULL, NULL, (LPBYTE)steamPath, &bufferSize);
            RegCloseKey(hKey);

            if (result == ERROR_SUCCESS) {
                std::string path(steamPath);
                std::replace(path.begin(), path.end(), '/', '\\');

                // Сначала проверяем основную папку Steam как библиотеку
                if (validateSteamLibraryPath(path)) {
                    found = true;
                    steamPaths.insert(path);
                }

                // Ищем дополнительные библиотеки в libraryfolders.vdf
                findAdditionalLibraries(path);
            }
        }

        // Пробуем HKEY_LOCAL_MACHINE
        result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey);
        if (result == ERROR_SUCCESS) {
            DWORD bufferSize = MAX_PATH;
            result = RegQueryValueExA(hKey, "SteamPath", NULL, NULL, (LPBYTE)steamPath, &bufferSize);
            RegCloseKey(hKey);

            if (result == ERROR_SUCCESS) {
                std::string path(steamPath);
                std::replace(path.begin(), path.end(), '/', '\\');

                if (validateSteamLibraryPath(path)) {
                    found = true;
                    steamPaths.insert(path);
                }

                findAdditionalLibraries(path);
            }
        }
    }

    return found;
}

bool SteamManager::findAdditionalLibraries(const std::string& steamInstallPath) {
    try {
        // Читаем libraryfolders.vdf для поиска дополнительных библиотек
        fs::path libraryFoldersPath = fs::path(steamInstallPath) / "steamapps" / "libraryfolders.vdf";

        if (!fs::exists(libraryFoldersPath)) {
            return false;
        }

        // Простой парсинг libraryfolders.vdf
        std::ifstream file(libraryFoldersPath);
        std::string line;
        bool found = false;

        while (std::getline(file, line)) {
            // Ищем строки с путями в кавычках
            size_t start = line.find('"');
            if (start != std::string::npos) {
                size_t end = line.find('"', start + 1);
                if (end != std::string::npos) {
                    std::string potentialPath = line.substr(start + 1, end - start - 1);

                    // Проверяем, является ли это путем (содержит :\ или /)
                    if (potentialPath.find(":") != std::string::npos ||
                        potentialPath.find("/") != std::string::npos ||
                        potentialPath.find("\\") != std::string::npos) {

                        // Заменяем слеши если нужно
                        std::replace(potentialPath.begin(), potentialPath.end(), '/', '\\');

                        if (validateSteamLibraryPath(potentialPath)) {
                            steamPaths.insert(potentialPath);
                            found = true;
                        }
                    }
                }
            }
        }
        return found;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool SteamManager::findInCommonLocations() {
    std::vector<std::string> commonPaths;
    bool found = false;

    // Стандартные расположения SteamLibrary
    char drivesBuffer[MAX_PATH];
    DWORD drives = GetLogicalDrives();

    for (char drive = 'A'; drive <= 'Z'; drive++) {
        if (drives & (1 << (drive - 'A'))) {
            // Проверяем различные возможные имена папок библиотек
            std::string driveStr = std::string(1, drive) + ":";

            commonPaths.push_back(driveStr + "\\SteamLibrary");
            commonPaths.push_back(driveStr + "\\Steam");
            commonPaths.push_back(driveStr + "\\Games\\Steam");
            commonPaths.push_back(driveStr + "\\Program Files\\Steam");
            commonPaths.push_back(driveStr + "\\Program Files (x86)\\Steam");
            commonPaths.push_back(driveStr + "\\SteamGames");
            commonPaths.push_back(driveStr + "\\Steam Games");
        }
    }

    // Также проверяем стандартные расположения Program Files
    char programFiles[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, programFiles) == S_OK) {
        commonPaths.push_back(std::string(programFiles) + "\\Steam");
    }

    char programFilesX86[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILESX86, NULL, SHGFP_TYPE_CURRENT, programFilesX86) == S_OK) {
        commonPaths.push_back(std::string(programFilesX86) + "\\Steam");
    }

    // Проверяем пользовательские расположения
    char userProfile[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, userProfile) == S_OK) {
        commonPaths.push_back(std::string(userProfile) + "\\Steam");
    }

    for (const auto& path : commonPaths) {
        if (validateSteamLibraryPath(path)) {
            steamPaths.insert(path);
            found = true;
        }
    }

    return found;
}

void SteamManager::findAllDotaPaths() {
    dotaPaths.clear();

    for (const auto& steamPath : steamPaths) {
        std::string potentialDotaPath = steamPath + "\\steamapps\\common\\dota 2 beta";
        if (validateDotaPath(potentialDotaPath)) {
            dotaPaths.push_back(potentialDotaPath);
        }
    }
}

void SteamManager::selectDotaPathInteractive() {
    if (dotaPaths.empty()) {
        std::cout << "Dota 2 не найдена в автоматическом режиме." << std::endl;
        std::cout << "Пожалуйста, введите путь к Dota 2 вручную: ";
        std::string manualPath;
        std::getline(std::cin, manualPath);

        if (validateDotaPath(manualPath)) {
            selectedDotaPath = manualPath;
            std::cout << "Путь принят: " << manualPath << std::endl;
        }
        else {
            std::cout << "Неверный путь к Dota 2!" << std::endl;
        }
        return;
    }

    if (dotaPaths.size() == 1) {
        selectedDotaPath = dotaPaths[0];
        std::cout << "Найдена одна установка Dota 2: " << selectedDotaPath << std::endl;
        return;
    }

    // Несколько путей найдено - предлагаем выбор
    std::cout << "Найдено несколько установок Dota 2:" << std::endl;
    for (size_t i = 0; i < dotaPaths.size(); ++i) {
        std::cout << i + 1 << ": " << dotaPaths[i] << std::endl;
    }

    std::cout << "Выберите номер установки (1-" << dotaPaths.size() << "): ";
    int choice = 0;
    if (!(std::cin >> choice)) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << "Неверный ввод, используем первую установку." << std::endl;
        selectedDotaPath = dotaPaths[0];
        return;
    }

    if (choice >= 1 && choice <= static_cast<int>(dotaPaths.size())) {
        selectedDotaPath = dotaPaths[choice - 1];
        std::cout << "Выбрана установка: " << selectedDotaPath << std::endl;
    }
    else {
        std::cout << "Неверный выбор, используем первую установку." << std::endl;
        selectedDotaPath = dotaPaths[0];
    }

    // Очищаем буфер ввода
    std::cin.ignore(10000, '\n');
}

void SteamManager::findSteamDirectory() {
    std::cout << "Поиск библиотек Steam..." << std::endl;

    steamPaths.clear();

    // Ищем все возможные пути Steam
    findInRegistry();
    findInCommonLocations();

    if (steamPaths.empty()) {
        std::cout << "Библиотеки Steam не найдены!" << std::endl;
        return;
    }

    std::cout << "Найдено библиотек Steam: " << steamPaths.size() << std::endl;
    for (const auto& path : steamPaths) {
        std::cout << "  - " << path << std::endl;
    }

    // Ищем все пути к Dota 2
    findAllDotaPaths();

    // Выбираем путь к Dota 2
    selectDotaPathInteractive();

    if (!selectedDotaPath.empty()) {
        // Устанавливаем производные пути
        this->dotaPath = selectedDotaPath;
        this->vpkPath = this->dotaPath + "\\game\\dota\\pak01_dir.vpk";
        this->modPath = this->dotaPath + "\\game\\mods\\pak02_dir.vpk";
        this->addonsContentPath = this->dotaPath + "\\content\\dota_addons";
        this->addonsGamePath = this->dotaPath + "\\game\\dota_addons";
        this->resourceCompiler = this->dotaPath + "\\game\\bin\\win64\\resourcecompiler.exe";

        std::cout << "Путь к Dota 2 установлен: " << dotaPath << std::endl;
    }
    else {
        std::cout << "Не удалось найти действительный путь к Dota 2!" << std::endl;
    }
}

// === GUI-friendly path management ===

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

fs::path SteamManager::GetSavedPathFile() {
    return GetExecutableDirectory() / "configs" / "steam_path.json";
}

bool SteamManager::LoadSavedPath() {
    try {
        fs::path pathFile = GetSavedPathFile();
        if (!fs::exists(pathFile)) {
            return false;
        }

        std::ifstream file(pathFile);
        if (!file.is_open()) {
            return false;
        }

        json j;
        file >> j;

        if (!j.contains("dota_path") || !j["dota_path"].is_string()) {
            return false;
        }

        std::string savedPath = j["dota_path"].get<std::string>();
        std::replace(savedPath.begin(), savedPath.end(), '/', '\\');
        return SetDotaPath(savedPath);
    }
    catch (const std::exception&) {
        return false;
    }
}

void SteamManager::SavePath() const {
    try {
        if (selectedDotaPath.empty()) {
            return;
        }
        fs::path pathFile = GetSavedPathFile();
        fs::create_directories(pathFile.parent_path());
        json j;
        j["dota_path"] = selectedDotaPath;
        std::ofstream file(pathFile);
        if (file.is_open()) {
            file << j.dump(2);
        }
    }
    catch (const std::exception&) {
        // Ignore save failures
    }
}

void SteamManager::ClearPaths() {
    selectedDotaPath.clear();
    selectedSteamPath.clear();
    dotaPath.clear();
    vpkPath.clear();
    modPath.clear();
    addonsContentPath.clear();
    addonsGamePath.clear();
    resourceCompiler.clear();
}

std::vector<std::string> SteamManager::FindDotaPathsSilent() {
    steamPaths.clear();
    dotaPaths.clear();

    findInRegistry();
    findInCommonLocations();

    findAllDotaPaths();
    return dotaPaths;
}

bool SteamManager::SetDotaPath(const std::string& path) {
    if (!validateDotaPath(path)) {
        return false;
    }

    selectedDotaPath = path;
    dotaPath = path;
    vpkPath = dotaPath + "\\game\\dota\\pak01_dir.vpk";
    modPath = dotaPath + "\\game\\mods\\pak02_dir.vpk";
    addonsContentPath = dotaPath + "\\content\\dota_addons";
    addonsGamePath = dotaPath + "\\game\\dota_addons";
    resourceCompiler = dotaPath + "\\game\\bin\\win64\\resourcecompiler.exe";

    // Try to derive the Steam library path that owns this Dota installation.
    fs::path p(path);
    fs::path libraryRoot = p.parent_path().parent_path().parent_path();
    if (validateSteamLibraryPath(libraryRoot.string())) {
        selectedSteamPath = libraryRoot.string();
    }

    return true;
}

bool SteamManager::HasValidPath() const {
    return !dotaPath.empty() && validateDotaPath(dotaPath);
}

// Новые методы для доступа к данным
std::string SteamManager::getSteamPath() const {
    return selectedSteamPath;
}

std::string SteamManager::getDotaPath() const {
    return dotaPath;
}

std::string SteamManager::getVpkPath() const {
    return vpkPath;
}

std::string SteamManager::getModPath() const {
    return modPath;
}

std::string SteamManager::getAddonsContentPath() const {
    return addonsContentPath;
}

std::string SteamManager::getAddonsGamePath() const {
    return addonsGamePath;
}

std::string SteamManager::getResourceCompiler() const {
    return resourceCompiler;
}