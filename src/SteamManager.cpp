#include "SteamManager.h"
#include "Structures.h"
#include "SecurityHardening.h"
namespace {
__declspec(noinline) void SH_AD_SteamManager() noexcept {
    if (SH_PebBeingDebugged()) g_integritySeed ^= 0xAABBCCDD;
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
}
}


namespace fs = std::filesystem;

SteamManager& SteamManager::GetInstance()
{
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    static SteamManager instance;
    return instance;
}

bool SteamManager::validateSteamLibraryPath(const std::string& path) {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (path.empty()) return false;

    try {
        fs::path steamLibDir(path);

        if (!fs::exists(steamLibDir) || !fs::is_directory(steamLibDir)) {
            return false;
        }

        // Проверяем наличие папки steamapps/common - это основная характеристика библиотеки Steam
        fs::path steamappsCommon = steamLibDir / OBF_CSTR("steamapps") / OBF_CSTR("common");
        if (!fs::exists(steamappsCommon) || !fs::is_directory(steamappsCommon)) {
            return false;
        }

        // Дополнительные проверки для уверенности
        fs::path steamappsDir = steamLibDir / OBF_CSTR("steamapps");
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
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (path.empty()) return false;

    try {
        fs::path dotaDir(path);

        // Проверяем наличие основных папок и файлов Dota 2
        if (!fs::exists(dotaDir) || !fs::is_directory(dotaDir)) {
            return false;
        }

        // Проверяем наличие game/dota/pak01_dir.vpk
        fs::path vpkPath = dotaDir / OBF_CSTR("game") / OBF_CSTR("dota") / OBF_CSTR("pak01_dir.vpk");
        if (!fs::exists(vpkPath)) {
            return false;
        }

        // Проверяем наличие папки game/dota_addons
        fs::path addonsPath = dotaDir / OBF_CSTR("game") / OBF_CSTR("dota_addons");
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
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    HKEY hKey;
    LONG result;
    DWORD bufferSize = MAX_PATH;
    char steamPath[MAX_PATH];

    std::vector<std::string> registryPaths = {
        OBF_CSTR("Software\\Valve\\Steam"),
        OBF_CSTR("Software\\Wow6432Node\\Valve\\Steam")
    };

    bool found = false;

    for (const auto& regPath : registryPaths) {
        // Пробуем HKEY_CURRENT_USER
        result = SH_RegOpenKeyExA(HKEY_CURRENT_USER, regPath.c_str(), 0, KEY_READ, &hKey);
        if (result == ERROR_SUCCESS) {
            DWORD bufferSize = MAX_PATH;
            result = SH_RegQueryValueExA(hKey, OBF_CSTR("SteamPath"), NULL, NULL, (LPBYTE)steamPath, &bufferSize);
            SH_RegCloseKey(hKey);

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
        result = SH_RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey);
        if (result == ERROR_SUCCESS) {
            DWORD bufferSize = MAX_PATH;
            result = SH_RegQueryValueExA(hKey, OBF_CSTR("SteamPath"), NULL, NULL, (LPBYTE)steamPath, &bufferSize);
            SH_RegCloseKey(hKey);

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
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    try {
        // Читаем libraryfolders.vdf для поиска дополнительных библиотек
        fs::path libraryFoldersPath = fs::path(steamInstallPath) / OBF_CSTR("steamapps") / OBF_CSTR("libraryfolders.vdf");

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
                    if (potentialPath.find(OBF_CSTR(":")) != std::string::npos ||
                        potentialPath.find(OBF_CSTR("/")) != std::string::npos ||
                        potentialPath.find(OBF_CSTR("\\")) != std::string::npos) {

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
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::vector<std::string> commonPaths;
    bool found = false;

    // Стандартные расположения SteamLibrary
    char drivesBuffer[MAX_PATH];
    DWORD drives = SH_GetLogicalDrives();

    for (char drive = 'A'; drive <= 'Z'; drive++) {
        if (drives & (1 << (drive - 'A'))) {
            // Проверяем различные возможные имена папок библиотек
            std::string driveStr = std::string(1, drive) + OBF_CSTR(":");

            commonPaths.push_back(driveStr + OBF_CSTR("\\SteamLibrary"));
            commonPaths.push_back(driveStr + OBF_CSTR("\\Steam"));
            commonPaths.push_back(driveStr + OBF_CSTR("\\Games\\Steam"));
            commonPaths.push_back(driveStr + OBF_CSTR("\\Program Files\\Steam"));
            commonPaths.push_back(driveStr + OBF_CSTR("\\Program Files (x86)\\Steam"));
            commonPaths.push_back(driveStr + OBF_CSTR("\\SteamGames"));
            commonPaths.push_back(driveStr + OBF_CSTR("\\Steam Games"));
        }
    }

    // Также проверяем стандартные расположения Program Files
    char programFiles[MAX_PATH];
    if (SH_SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, programFiles) == S_OK) {
        commonPaths.push_back(std::string(programFiles) + OBF_CSTR("\\Steam"));
    }

    char programFilesX86[MAX_PATH];
    if (SH_SHGetFolderPathA(NULL, CSIDL_PROGRAM_FILESX86, NULL, SHGFP_TYPE_CURRENT, programFilesX86) == S_OK) {
        commonPaths.push_back(std::string(programFilesX86) + OBF_CSTR("\\Steam"));
    }

    // Проверяем пользовательские расположения
    char userProfile[MAX_PATH];
    if (SH_SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, userProfile) == S_OK) {
        commonPaths.push_back(std::string(userProfile) + OBF_CSTR("\\Steam"));
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
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    dotaPaths.clear();

    for (const auto& steamPath : steamPaths) {
        std::string potentialDotaPath = steamPath + OBF_CSTR("\\steamapps\\common\\dota 2 beta");
        if (validateDotaPath(potentialDotaPath)) {
            dotaPaths.push_back(potentialDotaPath);
        }
    }
}

void SteamManager::selectDotaPathInteractive() {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (dotaPaths.empty()) {
        std::cout << OBF_CSTR("Dota 2 не найдена в автоматическом режиме.") << std::endl;
        std::cout << OBF_CSTR("Пожалуйста, введите путь к Dota 2 вручную: ");
        std::string manualPath;
        std::getline(std::cin, manualPath);

        if (validateDotaPath(manualPath)) {
            selectedDotaPath = manualPath;
            std::cout << OBF_CSTR("Путь принят: ") << manualPath << std::endl;
        }
        else {
            std::cout << OBF_CSTR("Неверный путь к Dota 2!") << std::endl;
        }
        return;
    }

    if (dotaPaths.size() == 1) {
        selectedDotaPath = dotaPaths[0];
        std::cout << OBF_CSTR("Найдена одна установка Dota 2: ") << selectedDotaPath << std::endl;
        return;
    }

    // Несколько путей найдено - предлагаем выбор
    std::cout << OBF_CSTR("Найдено несколько установок Dota 2:") << std::endl;
    for (size_t i = 0; i < dotaPaths.size(); ++i) {
        std::cout << i + 1 << OBF_CSTR(": ") << dotaPaths[i] << std::endl;
    }

    std::cout << OBF_CSTR("Выберите номер установки (1-") << dotaPaths.size() << OBF_CSTR("): ");
    int choice = 0;
    if (!(std::cin >> choice)) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << OBF_CSTR("Неверный ввод, используем первую установку.") << std::endl;
        selectedDotaPath = dotaPaths[0];
        return;
    }

    if (choice >= 1 && choice <= static_cast<int>(dotaPaths.size())) {
        selectedDotaPath = dotaPaths[choice - 1];
        std::cout << OBF_CSTR("Выбрана установка: ") << selectedDotaPath << std::endl;
    }
    else {
        std::cout << OBF_CSTR("Неверный выбор, используем первую установку.") << std::endl;
        selectedDotaPath = dotaPaths[0];
    }

    // Очищаем буфер ввода
    std::cin.ignore(10000, '\n');
}

void SteamManager::findSteamDirectory() {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::cout << OBF_CSTR("Поиск библиотек Steam...") << std::endl;

    steamPaths.clear();

    // Ищем все возможные пути Steam
    findInRegistry();
    findInCommonLocations();

    if (steamPaths.empty()) {
        std::cout << OBF_CSTR("Библиотеки Steam не найдены!") << std::endl;
        return;
    }

    std::cout << OBF_CSTR("Найдено библиотек Steam: ") << steamPaths.size() << std::endl;
    for (const auto& path : steamPaths) {
        std::cout << OBF_CSTR("  - ") << path << std::endl;
    }

    // Ищем все пути к Dota 2
    findAllDotaPaths();

    // Выбираем путь к Dota 2
    selectDotaPathInteractive();

    if (!selectedDotaPath.empty()) {
        // Устанавливаем производные пути
        this->dotaPath = selectedDotaPath;
        this->vpkPath = this->dotaPath + OBF_CSTR("\\game\\dota\\pak01_dir.vpk");
        this->modPath = this->dotaPath + OBF_CSTR("\\game\\mods\\pak02_dir.vpk");
        this->addonsContentPath = this->dotaPath + OBF_CSTR("\\content\\dota_addons");
        this->addonsGamePath = this->dotaPath + OBF_CSTR("\\game\\dota_addons");
        this->resourceCompiler = this->dotaPath + OBF_CSTR("\\game\\bin\\win64\\resourcecompiler.exe");

        std::cout << OBF_CSTR("Путь к Dota 2 установлен: ") << dotaPath << std::endl;
    }
    else {
        std::cout << OBF_CSTR("Не удалось найти действительный путь к Dota 2!") << std::endl;
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
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    return GetExecutableDirectory() / OBF_CSTR("configs") / OBF_CSTR("steam_path.json");
}

bool SteamManager::LoadSavedPath() {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
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

        if (!j.contains(OBF_CSTR("dota_path")) || !j[OBF_CSTR("dota_path")].is_string()) {
            return false;
        }

        std::string savedPath = j[OBF_CSTR("dota_path")].get<std::string>();
        std::replace(savedPath.begin(), savedPath.end(), '/', '\\');
        return SetDotaPath(savedPath);
    }
    catch (const std::exception&) {
        return false;
    }
}

void SteamManager::SavePath() const {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    try {
        if (selectedDotaPath.empty()) {
            return;
        }
        fs::path pathFile = GetSavedPathFile();
        fs::create_directories(pathFile.parent_path());
        json j;
        j[OBF_CSTR("dota_path")] = selectedDotaPath;
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
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
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
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    steamPaths.clear();
    dotaPaths.clear();

    findInRegistry();
    findInCommonLocations();

    findAllDotaPaths();
    return dotaPaths;
}

bool SteamManager::SetDotaPath(const std::string& path) {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (!validateDotaPath(path)) {
        return false;
    }

    selectedDotaPath = path;
    dotaPath = path;
    vpkPath = dotaPath + OBF_CSTR("\\game\\dota\\pak01_dir.vpk");
    modPath = dotaPath + OBF_CSTR("\\game\\mods\\pak02_dir.vpk");
    addonsContentPath = dotaPath + OBF_CSTR("\\content\\dota_addons");
    addonsGamePath = dotaPath + OBF_CSTR("\\game\\dota_addons");
    resourceCompiler = dotaPath + OBF_CSTR("\\game\\bin\\win64\\resourcecompiler.exe");

    // Try to derive the Steam library path that owns this Dota installation.
    fs::path p(path);
    fs::path libraryRoot = p.parent_path().parent_path().parent_path();
    if (validateSteamLibraryPath(libraryRoot.string())) {
        selectedSteamPath = libraryRoot.string();
    }

    return true;
}

bool SteamManager::HasValidPath() const {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    return !dotaPath.empty() && validateDotaPath(dotaPath);
}

// Новые методы для доступа к данным
std::string SteamManager::getSteamPath() const {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    return selectedSteamPath;
}

std::string SteamManager::getDotaPath() const {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    return dotaPath;
}

std::string SteamManager::getVpkPath() const {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    return vpkPath;
}

std::string SteamManager::getModPath() const {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    return modPath;
}

std::string SteamManager::getAddonsContentPath() const {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    return addonsContentPath;
}

std::string SteamManager::getAddonsGamePath() const {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    return addonsGamePath;
}

std::string SteamManager::getResourceCompiler() const {
    SH_AD_SteamManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    return resourceCompiler;
}