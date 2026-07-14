// VPKManager.cpp
#include "VPKManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <utility>
#include <iostream>
#include "SecurityHardening.h"
namespace {
__declspec(noinline) void SH_AD_VPKManager() noexcept {
    if (SH_HardwareBreakpoints()) g_integritySeed ^= 0x99AABBCC;
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
}
}


// ============================================================================
// THREAD-LOCAL ПЕРЕМЕННАЯ ДЛЯ ЗАЩИТЫ ОТ DEADLOCK
// ============================================================================
thread_local bool m_inVPKOperation = false;

struct VPKOperationGuard {
    VPKOperationGuard() {
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk; m_inVPKOperation = true; }
    ~VPKOperationGuard() {
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk; m_inVPKOperation = false; }
    VPKOperationGuard(const VPKOperationGuard&) = delete;
    VPKOperationGuard& operator=(const VPKOperationGuard&) = delete;
};

// ============================================================================
// SINGLETON И КОНСТРУКТОРЫ
// ============================================================================
VPKManager& VPKManager::GetInstance()
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    static VPKManager instance;
    return instance;
}

VPKManager::~VPKManager()
{
    ClearCache();
}

// ============================================================================
// УПРАВЛЕНИЕ КЭШЕМ
// ============================================================================
void VPKManager::SetMaxCacheSize(size_t maxEntries)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
    m_maxCacheSize = maxEntries > 0 ? maxEntries : 10;
}

void VPKManager::ClearCache()
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
    // Сначала сохраняем все изменённые VPK
    for (auto& [path, entry] : m_vpkCache) {
        if (entry.isModified.load() && entry.vpk) {
            fs::path savePath(entry.vpkPath);
            entry.vpk->bake(savePath.parent_path().string(), {}, nullptr);
        }
    }
        m_vpkCache.clear();
        m_cacheHits.store(0, std::memory_order_relaxed);
        m_cacheMisses.store(0, std::memory_order_relaxed);
}

void VPKManager::ClearCacheEntry(const std::string& vpkPath)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
    auto it = m_vpkCache.find(vpkPath);
    if (it != m_vpkCache.end()) {
        // Сохраняем если есть изменения
        if (it->second.isModified.load() && it->second.vpk) {
            fs::path savePath(vpkPath);
            it->second.vpk->bake(savePath.parent_path().string(), {}, nullptr);
        }
        m_vpkCache.erase(it);
    }
}

Result<void, std::string> VPKManager::FlushCacheEntry(const std::string& vpkPath)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
    auto it = m_vpkCache.find(vpkPath);
    if (it == m_vpkCache.end()) {
        return Result<void, std::string>::Ok();  // Нет в кэше - ничего не делаем
    }

    if (it->second.isModified.load() && it->second.vpk) {
        fs::path savePath(vpkPath);
        if (!it->second.vpk->bake(savePath.parent_path().string(), {}, nullptr)) {
            return Result<void, std::string>::Err(
                OBF_CSTR("Failed to bake VPK: ") + vpkPath
            );
        }
        it->second.isModified.store(false);
    }
    return Result<void, std::string>::Ok();
}

VPKManager::CacheStats VPKManager::GetCacheStats() const
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
    double hitRate = 0.0;
    size_t hits = m_cacheHits.load(std::memory_order_relaxed);
    size_t misses = m_cacheMisses.load(std::memory_order_relaxed);
    if (hits + misses > 0) {
        hitRate = (static_cast<double>(hits) /
            (hits + misses)) * 100.0;
    }
    return {
        m_vpkCache.size(),
        hits,
        misses,
        m_maxCacheSize,
        hitRate
    };
}

void VPKManager::EvictOldCacheEntries()
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    // Требует уже захваченного m_cacheMutex (unique_lock)
    while (m_vpkCache.size() >= m_maxCacheSize) {
        // Находим наименее используемую запись (LRU)
        auto oldest = std::min_element(
            m_vpkCache.begin(),
            m_vpkCache.end(),
            [](const auto& a, const auto& b) {
                // Приоритет: наименьшее количество доступов, затем наименьшее время
                size_t aCount = a.second.accessCount.load(std::memory_order_relaxed);
                size_t bCount = b.second.accessCount.load(std::memory_order_relaxed);
                if (aCount != bCount) {
                    return aCount < bCount;
                }
                return a.second.lastAccess.load(std::memory_order_relaxed) <
                       b.second.lastAccess.load(std::memory_order_relaxed);
            }
        );

        // Сохраняем если есть изменения
        if (oldest->second.isModified.load() && oldest->second.vpk) {
            fs::path savePath(oldest->second.vpkPath);
            oldest->second.vpk->bake(savePath.parent_path().string(), {}, nullptr);
        }
        m_vpkCache.erase(oldest);
    }
}

// ============================================================================
// ВНУТРЕННИЕ МЕТОДЫ
// ============================================================================
std::recursive_mutex& VPKManager::GetVPKLock(const std::string& vpkPath)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::lock_guard<std::mutex> lock(m_locksMutex);
    auto it = m_vpkLocks.find(vpkPath);
    if (it == m_vpkLocks.end()) {
        m_vpkLocks[vpkPath] = std::make_unique<std::recursive_mutex>();
    }
    return *m_vpkLocks[vpkPath];
}

vpkpp::PackFile* VPKManager::GetOrCreateVPK(const std::string& vpkPath)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    // Быстрая проверка через shared_lock для чтения
    {
        std::shared_lock<std::shared_mutex> readLock(m_cacheMutex);
        auto it = m_vpkCache.find(vpkPath);
        if (it != m_vpkCache.end()) {
            it->second.lastAccess.store(std::time(nullptr));
            it->second.accessCount.fetch_add(1, std::memory_order_relaxed);
            m_cacheHits.fetch_add(1, std::memory_order_relaxed);
            return it->second.vpk.get();
        }
    }

    // Кэш-промах - нужен unique_lock для записи
    std::unique_lock<std::shared_mutex> writeLock(m_cacheMutex);
    m_cacheMisses.fetch_add(1, std::memory_order_relaxed);

    // Проверяем ещё раз (double-checked locking)
    auto it = m_vpkCache.find(vpkPath);
    if (it != m_vpkCache.end()) {
        it->second.lastAccess.store(std::time(nullptr));
        it->second.accessCount.fetch_add(1, std::memory_order_relaxed);
        return it->second.vpk.get();
    }

    // Очищаем старые записи если кэш полон
    EvictOldCacheEntries();

    // Открываем VPK
    auto VPK = vpkpp::VPK::open(vpkPath);
    if (!VPK) {
        return nullptr;
    }

    // Используем try_emplace для безопасного добавления
    auto [cacheIt, inserted] = m_vpkCache.try_emplace(vpkPath);
    auto& entry = cacheIt->second;
    entry.vpk = std::move(VPK);
    entry.vpkPath = vpkPath;
    entry.lastAccess.store(std::time(nullptr));
    entry.createTime = std::time(nullptr);
    entry.isModified.store(false);
    entry.accessCount.store(1);

    return entry.vpk.get();
}

// ============================================================================
// ОСНОВНЫЕ МЕТОДЫ
// ============================================================================
Result<void, std::string> VPKManager::CreateVPK(
    const std::string& path,
    const std::vector<FileEntry>& files
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (path.empty()) {
        return Result<void, std::string>::Err(OBF_CSTR("VPK path is empty"));
    }

    if (path.length() > MAX_VPK_PATH_LENGTH) {
        return Result<void, std::string>::Err(OBF_CSTR("VPK path too long"));
    }

    fs::path fpath(path);
    if (!PathValidator::HasValidExtension(fpath, { OBF_CSTR(".vpk") })) {
        return Result<void, std::string>::Err(
            OBF_CSTR("Invalid VPK extension: ") + fpath.extension().string()
        );
    }

    if (fpath.has_parent_path() && !fs::exists(fpath.parent_path())) {
        std::error_code ec;
        if (!fs::create_directories(fpath.parent_path(), ec)) {
            return Result<void, std::string>::Err(
                OBF_CSTR("Cannot create VPK directory: ") + ec.message()
            );
        }
    }

    try {
        auto VPK = vpkpp::VPK::create(fpath.filename().string(), 2);
        if (!VPK) {
            return Result<void, std::string>::Err(
                OBF_CSTR("Failed to create VPK archive: ") + fpath.filename().string()
            );
        }

        // Используем пакетное добавление если есть файлы
        if (!files.empty()) {
            for (const FileEntry& fe : files) {
                if (!fe.data || fe.data->empty()) {
                    continue;
                }
                vpkpp::EntryOptions entryOptions;
                entryOptions.vpk_saveToDirectory = true;
                VPK->addEntry(fe.vpkPath, fe.data->data(), fe.data->size(), entryOptions);
            }
        }

        fs::path savePath = fpath.parent_path().empty() ?
            fs::current_path() : fpath.parent_path();
        if (VPK->bake(savePath.string(), {}, nullptr)) {
            // Добавляем в кэш
            {
                std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
                auto& entry = m_vpkCache[path];
                entry.vpk = std::move(VPK);
                entry.vpkPath = path;
                entry.lastAccess.store(std::time(nullptr));
                entry.createTime = std::time(nullptr);
                entry.isModified.store(false);
                entry.accessCount.store(1);
            }
            return Result<void, std::string>::Ok();
        }
        else {
            return Result<void, std::string>::Err(
                OBF_CSTR("Failed to save VPK archive: ") + fpath.filename().string()
            );
        }
    }
    catch (const std::exception& e) {
        return Result<void, std::string>::Err(
            OBF_CSTR("Exception while creating VPK: ") + std::string(e.what())
        );
    }
}

Result<FileData, std::string> VPKManager::GetFileFromVPK(
    const std::string& vpkPath,
    const std::string& vpkFilePath
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (vpkPath.empty()) {
        return Result<FileData, std::string>::Err(OBF_CSTR("VPK path is empty"));
    }
    if (vpkFilePath.empty()) {
        return Result<FileData, std::string>::Err(OBF_CSTR("File path is empty"));
    }
    if (vpkPath.length() > MAX_VPK_PATH_LENGTH) {
        return Result<FileData, std::string>::Err(OBF_CSTR("VPK path too long"));
    }
    if (vpkFilePath.length() > MAX_FILE_PATH_LENGTH) {
        return Result<FileData, std::string>::Err(OBF_CSTR("File path too long"));
    }
    if (!fs::exists(vpkPath)) {
        return Result<FileData, std::string>::Err(
            OBF_CSTR("VPK file does not exist: ") + vpkPath
        );
    }

    // Блокировка для конкретного VPK
    auto& vpkLock = GetVPKLock(vpkPath);
    std::lock_guard<std::recursive_mutex> lock(vpkLock);

    // Получаем из кэша
    auto* VPK = GetOrCreateVPK(vpkPath);
    if (!VPK) {
        return Result<FileData, std::string>::Err(
            OBF_CSTR("Failed to open VPK: ") + vpkPath
        );
    }

    auto data = VPK->readEntry(vpkFilePath);
    if (!data || data->empty()) {
        return Result<FileData, std::string>::Err(
            OBF_CSTR("File not found in VPK: ") + vpkFilePath
        );
    }

    return Result<FileData, std::string>::Ok(std::move(data));
}

Result<void, std::string> VPKManager::SaveFileFromVPK(
    const std::string& vpkPath,
    const std::string& vpkFilePath,
    const std::string& diskFilePath
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (diskFilePath.empty()) {
        return Result<void, std::string>::Err(OBF_CSTR("Disk file path is empty"));
    }

    fs::path dstPath(diskFilePath);
    if (dstPath.has_parent_path() && !fs::exists(dstPath.parent_path())) {
        std::error_code ec;
        if (!fs::create_directories(dstPath.parent_path(), ec)) {
            return Result<void, std::string>::Err(
                OBF_CSTR("Cannot create output directory: ") + ec.message()
            );
        }
    }

    auto getResult = GetFileFromVPK(vpkPath, vpkFilePath);
    if (getResult.IsErr()) {
        return Result<void, std::string>::Err(getResult.Error());
    }

    return SaveFileFromData(getResult.Value(), diskFilePath);
}

Result<void, std::string> VPKManager::SaveFileFromData(
    const FileData& data,
    const std::string& diskFilePath
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (!data || data->empty()) {
        return Result<void, std::string>::Err(OBF_CSTR("No data to save"));
    }
    if (diskFilePath.empty()) {
        return Result<void, std::string>::Err(OBF_CSTR("Disk file path is empty"));
    }

    fs::path dstPath(diskFilePath);
    if (dstPath.has_parent_path() && !fs::exists(dstPath.parent_path())) {
        std::error_code ec;
        if (!fs::create_directories(dstPath.parent_path(), ec)) {
            return Result<void, std::string>::Err(
                OBF_CSTR("Cannot create output directory: ") + ec.message()
            );
        }
    }

    try {
        std::ofstream temp(dstPath.string(), std::ios::binary);
        if (!temp.is_open()) {
            return Result<void, std::string>::Err(
                OBF_CSTR("Cannot open file for writing: ") + diskFilePath
            );
        }

        // Оптимизированная запись
        temp.write(reinterpret_cast<const char*>(data->data()), data->size());
        temp.close();

        if (temp.fail()) {
            return Result<void, std::string>::Err(
                OBF_CSTR("Failed to write file: ") + diskFilePath
            );
        }

        return Result<void, std::string>::Ok();
    }
    catch (const std::exception& e) {
        return Result<void, std::string>::Err(
            OBF_CSTR("Error saving file: ") + std::string(e.what())
        );
    }
}

// ============================================================================
// ИСПРАВЛЕННЫЙ AddFileToVPK С ЗАЩИТОЙ ОТ DEADLOCK
// ============================================================================
Result<void, std::string> VPKManager::AddFileToVPK(
    const std::string& vpkPath,
    const std::string& vpkFilePath,
    const FileData& data,
    bool deferBake
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    // Защита от рекурсии
    if (m_inVPKOperation) {
        return AddFileToVPKDeferred(vpkPath, vpkFilePath, data, deferBake);
    }
    VPKOperationGuard guard;

    try {
        // Проверка batch — с блокировкой
        {
            std::lock_guard<std::mutex> batchLock(m_batchMutex);
            if (m_activeBatch && m_activeBatch->isActive &&
                m_activeBatch->vpkPath == vpkPath) {
                return AddFileToBatch_Unlocked(vpkFilePath, data);
            }
        }

        // === ВАЛИДАЦИЯ ПАРАМЕТРОВ ===
        if (vpkPath.empty()) {
            return Result<void, std::string>::Err(OBF_CSTR("VPK path is empty"));
        }
        if (vpkFilePath.empty()) {
            return Result<void, std::string>::Err(OBF_CSTR("File path is empty"));
        }
        if (!data || data->empty()) {
            return Result<void, std::string>::Err(OBF_CSTR("No data to add"));
        }
        if (!fs::exists(vpkPath)) {
            return Result<void, std::string>::Err(
                OBF_CSTR("VPK file does not exist: ") + vpkPath
            );
        }

        // === БЛОКИРОВКА ДЛЯ КОНКРЕТНОГО VPK ===
        auto& vpkLock = GetVPKLock(vpkPath);
        std::lock_guard<std::recursive_mutex> lock(vpkLock);

        auto* VPK = GetOrCreateVPK(vpkPath);
        if (!VPK) {
            return Result<void, std::string>::Err(OBF_CSTR("Failed to open VPK: ") + vpkPath);
        }

        try {
            vpkpp::EntryOptions entryOptions;
            entryOptions.vpk_saveToDirectory = true;

            if (VPK->hasEntry(vpkFilePath)) {
                VPK->removeEntry(vpkFilePath);
            }

            VPK->addEntry(vpkFilePath, data->data(), data->size(), entryOptions);

            {
                std::shared_lock<std::shared_mutex> cacheLock(m_cacheMutex);
                auto it = m_vpkCache.find(vpkPath);
                if (it != m_vpkCache.end()) {
                    it->second.isModified.store(true);
                }
            }

            if (!deferBake) {
                fs::path savePath(vpkPath);
                if (!VPK->bake(savePath.parent_path().string(), {}, nullptr)) {
                    return Result<void, std::string>::Err(OBF_CSTR("Failed to save VPK: ") + vpkPath);
                }
            }

            return Result<void, std::string>::Ok();
        }
        catch (const std::exception& e) {
            return Result<void, std::string>::Err(
                OBF_CSTR("Exception in AddFileToVPK: ") + std::string(e.what()) +
                OBF_CSTR(" | vpkPath: ") + vpkPath +
                OBF_CSTR(" | vpkFilePath: ") + vpkFilePath
            );
        }
    }
    catch (...) {
        return Result<void, std::string>::Err(
            OBF_CSTR("Unknown exception in AddFileToVPK")
        );
    }
}

// ============================================================================
// НОВЫЙ МЕТОД: AddFileToVPKDeferred
// ============================================================================
Result<void, std::string> VPKManager::AddFileToVPKDeferred(
    const std::string& vpkPath,
    const std::string& vpkFilePath,
    const FileData& data,
    bool deferBake
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    // Добавляем в очередь отложенных операций
    std::lock_guard<std::mutex> lock(m_deferredMutex);

    // Проверка на переполнение очереди (защита от утечки памяти)
    if (m_deferredOperations.size() > 10000) {
        return Result<void, std::string>::Err(
            OBF_CSTR("Deferred operations queue overflow (>10000 operations)")
        );
    }

    m_deferredOperations.emplace(vpkPath, vpkFilePath, data, deferBake);
    return Result<void, std::string>::Ok();
}

// ============================================================================
// НОВЫЙ МЕТОД: ProcessDeferredOperations
// ============================================================================
void VPKManager::ProcessDeferredOperations()
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    // Быстрая проверка без блокировки
    {
        std::lock_guard<std::mutex> lock(m_deferredMutex);
        if (m_deferredOperations.empty()) {
            return;
        }
    }

    // Обрабатываем все отложенные операции
    // Важно: не держим lock во время обработки
    while (true) {
        DeferredOperation op;

        // Извлекаем операцию из очереди
        {
            std::lock_guard<std::mutex> lock(m_deferredMutex);
            if (m_deferredOperations.empty()) {
                break;
            }
            op = std::move(m_deferredOperations.front());
            m_deferredOperations.pop();
        }

        // Обрабатываем операцию (теперь m_inVPKOperation = false)
        AddFileToVPK(op.vpkPath, op.vpkFilePath, op.data, op.deferBake);
    }
}

// ============================================================================
// НОВЫЙ МЕТОД: GetDeferredOperationsCount
// ============================================================================
size_t VPKManager::GetDeferredOperationsCount() const
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_deferredMutex));
    return m_deferredOperations.size();
}

Result<void, std::string> VPKManager::AddFileToVPK(
    const std::string& vpkPath,
    const std::string& vpkFilePath,
    const std::string& diskFilePath
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    auto readResult = ReadFileFromDisk(diskFilePath);
    if (readResult.IsErr()) {
        return Result<void, std::string>::Err(readResult.Error());
    }
    return AddFileToVPK(vpkPath, vpkFilePath, readResult.Value());
}

Result<void, std::string> VPKManager::ExecForEntries(
    const std::string& vpkPath,
    const std::string& dir,
    const vpkpp::PackFile::EntryCallback& callback
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (vpkPath.empty()) {
        return Result<void, std::string>::Err(OBF_CSTR("VPK path is empty"));
    }
    if (!fs::exists(vpkPath)) {
        return Result<void, std::string>::Err(
            OBF_CSTR("VPK file does not exist: ") + vpkPath
        );
    }

    // Блокировка для конкретного VPK
    auto& vpkLock = GetVPKLock(vpkPath);
    std::lock_guard<std::recursive_mutex> lock(vpkLock);

    auto* VPK = GetOrCreateVPK(vpkPath);
    if (!VPK) {
        return Result<void, std::string>::Err(
            OBF_CSTR("Failed to open VPK: ") + vpkPath
        );
    }

    try {
        VPK->runForAllEntries(dir, callback, true, true);
        return Result<void, std::string>::Ok();
    }
    catch (const std::exception& e) {
        return Result<void, std::string>::Err(
            OBF_CSTR("Error executing callback: ") + std::string(e.what())
        );
    }
}

Result<bool, std::string> VPKManager::isValid(
    const std::string& vpkPath,
    const std::string& vpkFilePath
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (vpkPath.empty()) {
        return Result<bool, std::string>::Err(OBF_CSTR("VPK path is empty"));
    }
    if (vpkFilePath.empty()) {
        return Result<bool, std::string>::Err(OBF_CSTR("File path is empty"));
    }
    if (!fs::exists(vpkPath)) {
        return Result<bool, std::string>::Err(
            OBF_CSTR("VPK file does not exist: ") + vpkPath
        );
    }

    // Блокировка для конкретного VPK
    auto& vpkLock = GetVPKLock(vpkPath);
    std::lock_guard<std::recursive_mutex> lock(vpkLock);

    auto* VPK = GetOrCreateVPK(vpkPath);
    if (!VPK) {
        return Result<bool, std::string>::Err(
            OBF_CSTR("Failed to open VPK: ") + vpkPath
        );
    }

    if (VPK->hasEntry(vpkFilePath)) {
        auto data = VPK->readEntry(vpkFilePath);
        if (!data || data->empty()) {
            return Result<bool, std::string>::Ok(false);
        }
        return Result<bool, std::string>::Ok(true);
    }
    else {
        return Result<bool, std::string>::Ok(false);
    }
}

// ============================================================================
// ПАКЕТНЫЕ ОПЕРАЦИИ
// ============================================================================
Result<void, std::string> VPKManager::BeginBatch(const std::string& vpkPath)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::lock_guard<std::mutex> batchLock(m_batchMutex);
    if (m_activeBatch && m_activeBatch->isActive) {
        return Result<void, std::string>::Err(
            OBF_CSTR("Batch already active for: ") + m_activeBatch->vpkPath
        );
    }

    if (vpkPath.empty()) {
        return Result<void, std::string>::Err(OBF_CSTR("VPK path is empty"));
    }
    if (!fs::exists(vpkPath)) {
        return Result<void, std::string>::Err(
            OBF_CSTR("VPK file does not exist: ") + vpkPath
        );
    }

    // Блокировка для конкретного VPK
    auto& vpkLock = GetVPKLock(vpkPath);
    std::lock_guard<std::recursive_mutex> lock(vpkLock);

    auto* vpk = GetOrCreateVPK(vpkPath);
    if (!vpk) {
        return Result<void, std::string>::Err(
            OBF_CSTR("Failed to open VPK: ") + vpkPath
        );
    }

    m_activeBatch = BatchContext();
    m_activeBatch->vpkPath = vpkPath;
    m_activeBatch->vpk = vpk;
    m_activeBatch->pendingFiles.clear();
    m_activeBatch->isActive = true;
    m_activeBatch->startTime = std::time(nullptr);
    m_activeBatch->filesAdded = 0;

    return Result<void, std::string>::Ok();
}

Result<void, std::string> VPKManager::AddFileToBatch(
    const std::string& vpkFilePath,
    FileData data
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::lock_guard<std::mutex> batchLock(m_batchMutex);
    return AddFileToBatch_Unlocked(vpkFilePath, std::move(data));
}

Result<void, std::string> VPKManager::AddFileToBatch(
    const std::string& vpkFilePath,
    const std::string& diskFilePath
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    auto readResult = ReadFileFromDisk(diskFilePath);
    if (readResult.IsErr()) {
        return Result<void, std::string>::Err(readResult.Error());
    }

    std::lock_guard<std::mutex> batchLock(m_batchMutex);
    return AddFileToBatch_Unlocked(vpkFilePath, std::move(readResult.Value()));
}

Result<void, std::string> VPKManager::EndBatch()
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;

    Result<void, std::string> result = Result<void, std::string>::Ok();
    {
        std::lock_guard<std::mutex> batchLock(m_batchMutex);
        if (!m_activeBatch || !m_activeBatch->isActive) {
            return Result<void, std::string>::Err(OBF_CSTR("No active batch"));
        }

        auto& batch = *m_activeBatch;
        try {
            // Блокировка для конкретного VPK
            auto& vpkLock = GetVPKLock(batch.vpkPath);
            std::lock_guard<std::recursive_mutex> lock(vpkLock);

            // Добавляем все файлы
            for (const auto& [vpkFilePath, fileData] : batch.pendingFiles) {
                vpkpp::EntryOptions entryOptions;
                entryOptions.vpk_saveToDirectory = true;

                if (batch.vpk->hasEntry(vpkFilePath)) {
                    batch.vpk->removeEntry(vpkFilePath);
                }

                batch.vpk->addEntry(
                    vpkFilePath,
                    fileData->data(),
                    fileData->size(),
                    entryOptions
                );
            }

            // Один bake в конце
            fs::path savePath(batch.vpkPath);
            if (!batch.vpk->bake(savePath.parent_path().string(), {}, nullptr)) {
                result = Result<void, std::string>::Err(
                    OBF_CSTR("Failed to save VPK: ") + batch.vpkPath
                );
            }

            // Сбрасываем флаг модификации в кэше
            if (result.IsOk()) {
                std::unique_lock<std::shared_mutex> cacheLock(m_cacheMutex);
                auto it = m_vpkCache.find(batch.vpkPath);
                if (it != m_vpkCache.end()) {
                    it->second.isModified.store(false);
                }
            }

            batch.isActive = false;
            batch.pendingFiles.clear();
            m_activeBatch.reset();
        }
        catch (const std::exception& e) {
            batch.isActive = false;
            batch.pendingFiles.clear();
            m_activeBatch.reset();
            result = Result<void, std::string>::Err(
                OBF_CSTR("Exception during batch end: ") + std::string(e.what())
            );
        }
    }

    // Обрабатываем отложенные операции ВНЕ блокировки batch
    if (result.IsOk()) {
        ProcessDeferredOperations();
    }

    return result;
}

bool VPKManager::IsBatchActive() const
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::lock_guard<std::mutex> batchLock(const_cast<std::mutex&>(m_batchMutex));
    return m_activeBatch && m_activeBatch->isActive;
}

bool VPKManager::IsFileInBatch(
    const std::string& vpkPath,
    const std::string& vpkFilePath
) const
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::lock_guard<std::mutex> batchLock(const_cast<std::mutex&>(m_batchMutex));
    if (!m_activeBatch || !m_activeBatch->isActive) {
        return false;
    }
    if (m_activeBatch->vpkPath != vpkPath) {
        return false;
    }
    return m_activeBatch->pendingFiles.find(vpkFilePath) != m_activeBatch->pendingFiles.end();
}

Result<FileData, std::string> VPKManager::GetFileFromBatch(
    const std::string& vpkPath,
    const std::string& vpkFilePath
) const
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::lock_guard<std::mutex> batchLock(const_cast<std::mutex&>(m_batchMutex));
    if (!m_activeBatch || !m_activeBatch->isActive) {
        return Result<FileData, std::string>::Err(OBF_CSTR("No active batch"));
    }
    if (m_activeBatch->vpkPath != vpkPath) {
        return Result<FileData, std::string>::Err(OBF_CSTR("Batch is for a different VPK"));
    }
    auto it = m_activeBatch->pendingFiles.find(vpkFilePath);
    if (it == m_activeBatch->pendingFiles.end()) {
        return Result<FileData, std::string>::Err(OBF_CSTR("File not found in batch"));
    }
    if (!it->second || it->second->empty()) {
        return Result<FileData, std::string>::Err(OBF_CSTR("Batch file is empty"));
    }
    return Result<FileData, std::string>::Ok(it->second);
}

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
// ============================================================================
Result<FileData, std::string> VPKManager::ReadFileFromDisk(
    const std::string& diskFilePath
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (diskFilePath.empty()) {
        return Result<FileData, std::string>::Err(OBF_CSTR("File path is empty"));
    }
    if (!fs::exists(diskFilePath)) {
        return Result<FileData, std::string>::Err(
            OBF_CSTR("File does not exist: ") + diskFilePath
        );
    }

    try {
        std::ifstream file(diskFilePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return Result<FileData, std::string>::Err(
                OBF_CSTR("Cannot open file: ") + diskFilePath
            );
        }

        auto size = file.tellg();
        if (size < 0 || static_cast<size_t>(size) > MAX_FILE_SIZE) {
            return Result<FileData, std::string>::Err(
                OBF_CSTR("Invalid file size: ") + std::to_string(size)
            );
        }

        file.seekg(0, std::ios::beg);
        std::vector<std::byte> buffer(static_cast<size_t>(size));

        // Оптимизированное чтение
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        if (file) {
            return Result<FileData, std::string>::Ok(std::move(buffer));
        }
        else {
            return Result<FileData, std::string>::Err(
                OBF_CSTR("Failed to read file: ") + diskFilePath
            );
        }
    }
    catch (const std::exception& e) {
        return Result<FileData, std::string>::Err(
            OBF_CSTR("Error reading file: ") + std::string(e.what())
        );
    }
}

// ============================================================================
// ВНУТРЕННИЙ МЕТОД: AddFileToBatch_Unlocked (без блокировки)
// ============================================================================
Result<void, std::string> VPKManager::AddFileToBatch_Unlocked(
    const std::string& vpkFilePath,
    FileData data
)
{
    SH_AD_VPKManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    // ПРЕДУСЛОВИЕ: m_batchMutex должен быть уже захвачен вызывающим кодом!

    if (!m_activeBatch || !m_activeBatch->isActive) {
        return Result<void, std::string>::Err(
            OBF_CSTR("No active batch. Call BeginBatch() first")
        );
    }

    if (vpkFilePath.empty()) {
        return Result<void, std::string>::Err(OBF_CSTR("File path is empty"));
    }
    if (!data || data->empty()) {
        return Result<void, std::string>::Err(OBF_CSTR("No data to add"));
    }

    m_activeBatch->pendingFiles[vpkFilePath] = std::move(data);
    m_activeBatch->filesAdded++;
    return Result<void, std::string>::Ok();
}