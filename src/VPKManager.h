// VPKManager.h
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <optional>
#include <memory>
#include <chrono>
#include <queue>
#include <vpkpp/vpkpp.h>
#include "Structures.h"
#include "Result.h"
#include "PathValidator.h"

namespace fs = std::filesystem;

/**
* @brief Менеджер VPK архивов с кэшированием, пакетными операциями и потокобезопасностью
*
* Оптимизации:
* - Кэширование открытых VPK дескрипторов (избегает повторного парсинга дерева)
* - Пакетные операции (один bake() вместо bake() на каждый файл)
* - Reader-writer locks для конкурентного доступа
* - Оптимизированные конверсии FileData ↔ std::string
* - Защита от deadlock через thread_local флаг и очередь отложенных операций
*/
class VPKManager
{
public:
    static VPKManager& GetInstance();

    // ========================================================================
    // ОСНОВНЫЕ МЕТОДЫ (с кэшированием)
    // ========================================================================
    /**
    * @brief Создание нового VPK архива
    */
    Result<void, std::string> CreateVPK(
        const std::string& path,
        const std::vector<FileEntry>& files = {}
    );

    /**
    * @brief Получение файла из VPK архива (с кэшированием дескриптора)
    */
    Result<FileData, std::string> GetFileFromVPK(
        const std::string& vpkPath,
        const std::string& vpkFilePath
    );

    /**
    * @brief Сохранение файла из VPK на диск
    */
    Result<void, std::string> SaveFileFromVPK(
        const std::string& vpkPath,
        const std::string& vpkFilePath,
        const std::string& diskFilePath
    );

    /**
    * @brief Сохранение данных в файл на диске
    */
    Result<void, std::string> SaveFileFromData(
        const FileData& data,
        const std::string& diskFilePath
    );

    /**
    * @brief Добавление файла в VPK архив (с кэшированием и защитой от deadlock)
    */
    Result<void, std::string> AddFileToVPK(
        const std::string& vpkPath,
        const std::string& vpkFilePath,
        const FileData& data,
        bool deferBake = true
    );

    /**
    * @brief Добавление файла с диска в VPK архив
    */
    Result<void, std::string> AddFileToVPK(
        const std::string& vpkPath,
        const std::string& vpkFilePath,
        const std::string& diskFilePath
    );

    /**
    * @brief Выполнение функции для всех записей в директории VPK
    */
    Result<void, std::string> ExecForEntries(
        const std::string& vpkPath,
        const std::string& dir,
        const vpkpp::PackFile::EntryCallback& callback
    );

    /**
    * @brief Проверка существования файла в VPK
    */
    Result<bool, std::string> isValid(
        const std::string& vpkPath,
        const std::string& vpkFilePath
    );

    // ========================================================================
    // ПАКЕТНЫЕ ОПЕРАЦИИ (для массового добавления файлов)
    // ========================================================================
    /**
    * @brief Начать пакетную операцию добавления файлов
    * @details Открывает VPK один раз, позволяет добавлять файлы без bake()
    * @warning Обязательно вызвать EndBatch() после завершения!
    */
    Result<void, std::string> BeginBatch(const std::string& vpkPath);

    /**
    * @brief Добавить файл в текущую пакетную операцию
    * @details Файл добавляется в память, bake() не вызывается
    */
    Result<void, std::string> AddFileToBatch(
        const std::string& vpkFilePath,
        FileData data
    );

    /**
    * @brief Добавить файл с диска в текущую пакетную операцию
    */
    Result<void, std::string> AddFileToBatch(
        const std::string& vpkFilePath,
        const std::string& diskFilePath
    );

    /**
    * @brief Завершить пакетную операцию
    * @details Вызывает bake() один раз для всех добавленных файлов
    *          и обрабатывает отложенные операции
    */
    Result<void, std::string> EndBatch();

    /**
    * @brief Проверить, активна ли пакетная операция
    */
    bool IsBatchActive() const;

    /**
    * @brief Проверить, есть ли файл в текущей пакетной операции
    */
    bool IsFileInBatch(
        const std::string& vpkPath,
        const std::string& vpkFilePath
    ) const;

    /**
    * @brief Получить файл из текущей пакетной операции
    */
    Result<FileData, std::string> GetFileFromBatch(
        const std::string& vpkPath,
        const std::string& vpkFilePath
    ) const;

    // ========================================================================
    // УПРАВЛЕНИЕ КЭШЕМ
    // ========================================================================
    /**
    * @brief Очистить весь кэш VPK
    * @details Освобождает память, закрывает все дескрипторы
    */
    void ClearCache();

    /**
    * @brief Очистить конкретный VPK из кэша
    */
    void ClearCacheEntry(const std::string& vpkPath);

    /**
    * @brief Принудительно сохранить изменения для VPK в кэше
    */
    Result<void, std::string> FlushCacheEntry(const std::string& vpkPath);

    /**
    * @brief Получить статистику кэша
    */
    struct CacheStats {
        size_t cachedVPKs;          // Количество закэшированных VPK
        size_t totalHits;           // Всего попаданий в кэш
        size_t totalMisses;         // Всего промахов кэша
        size_t maxCacheSize;        // Максимальный размер кэша
        double hitRate;             // Процент попаданий (0-100)
    };
    CacheStats GetCacheStats() const;

    /**
    * @brief Установить максимальный размер кэша
    * @param maxEntries Максимальное количество VPK в кэше
    */
    void SetMaxCacheSize(size_t maxEntries);

    /**
    * @brief Обработать все отложенные операции
    * @details Вызывается после завершения пакетной операции
    */
    void ProcessDeferredOperations();

    /**
    * @brief Получить количество отложенных операций
    */
    size_t GetDeferredOperationsCount() const;

    /**
    * @brief Чтение файла с диска в память
    */
    Result<FileData, std::string> ReadFileFromDisk(const std::string& diskFilePath);

private:
    VPKManager() = default;
    ~VPKManager();
    VPKManager(const VPKManager&) = delete;
    VPKManager& operator=(const VPKManager&) = delete;

    /**
    * @brief Получить или создать VPK из кэша
    * @return nullptr если не удалось открыть
    */
    vpkpp::PackFile* GetOrCreateVPK(const std::string& vpkPath);

    /**
    * @brief Внутренний метод добавления в batch БЕЗ захвата мьютекса
    * @warning Вызывать только при уже захваченном m_batchMutex!
    */
    Result<void, std::string> AddFileToBatch_Unlocked(
        const std::string& vpkFilePath,
        FileData data
    );

    /**
    * @brief Получить мьютекс для конкретного VPK
    */
    std::recursive_mutex& GetVPKLock(const std::string& vpkPath);

    /**
    * @brief Удалить старые записи из кэша (LRU eviction)
    */
    void EvictOldCacheEntries();

    /**
    * @brief Добавить файл в отложенные операции
    */
    Result<void, std::string> AddFileToVPKDeferred(
        const std::string& vpkPath,
        const std::string& vpkFilePath,
        const FileData& data,
        bool deferBake
    );

    // ========================================================================
    // СТРУКТУРЫ ДАННЫХ
    // ========================================================================
    struct VPCCacheEntry {
        std::unique_ptr<vpkpp::PackFile> vpk;
        std::string vpkPath;
        std::atomic<std::time_t> lastAccess;
        std::time_t createTime;
        std::atomic<bool> isModified;
        std::atomic<size_t> accessCount;

        VPCCacheEntry()
            : lastAccess(0), createTime(0), isModified(false), accessCount(0) {
        }

        // Move-конструктор: atomics не копируются, значения переносятся вручную
        VPCCacheEntry(VPCCacheEntry&& other) noexcept
            : vpk(std::move(other.vpk)),
              vpkPath(std::move(other.vpkPath)),
              lastAccess(other.lastAccess.load()),
              createTime(other.createTime),
              isModified(other.isModified.load()),
              accessCount(other.accessCount.load()) {
        }

        VPCCacheEntry& operator=(VPCCacheEntry&& other) noexcept {
            if (this != &other) {
                vpk = std::move(other.vpk);
                vpkPath = std::move(other.vpkPath);
                lastAccess.store(other.lastAccess.load());
                createTime = other.createTime;
                isModified.store(other.isModified.load());
                accessCount.store(other.accessCount.load());
            }
            return *this;
        }

        // Запрещаем копирование
        VPCCacheEntry(const VPCCacheEntry&) = delete;
        VPCCacheEntry& operator=(const VPCCacheEntry&) = delete;
    };

    struct BatchContext {
        std::string vpkPath;
        vpkpp::PackFile* vpk;
        // Map deduplicates by VPK path and keeps only the last version of each file.
        std::unordered_map<std::string, FileData> pendingFiles;
        bool isActive;
        std::time_t startTime;
        size_t filesAdded;

        BatchContext()
            : vpk(nullptr), isActive(false), startTime(0), filesAdded(0) {
        }
    };

    // Структура для отложенной операции
    struct DeferredOperation {
        std::string vpkPath;
        std::string vpkFilePath;
        FileData data;
        bool deferBake;

        DeferredOperation() : deferBake(true) {}
        DeferredOperation(const std::string& path, const std::string& file,
            const FileData& d, bool defer)
            : vpkPath(path), vpkFilePath(file), data(d), deferBake(defer) {
        }
    };

    // ========================================================================
    // ЧЛЕНЫ КЛАССА
    // ========================================================================
    // Кэш VPK дескрипторов
    std::unordered_map<std::string, VPCCacheEntry> m_vpkCache;
    mutable std::shared_mutex m_cacheMutex;

    // Статистика кэша
    std::atomic<size_t> m_cacheHits{0};
    std::atomic<size_t> m_cacheMisses{0};
    size_t m_maxCacheSize = 10;  // Максимум 10 VPK в кэше

    // Контекст пакетной операции
    std::optional<BatchContext> m_activeBatch;
    std::mutex m_batchMutex;

    // Мьютексы для каждого VPK (для потокобезопасности)
    std::unordered_map<std::string, std::unique_ptr<std::recursive_mutex>> m_vpkLocks;
    std::mutex m_locksMutex;

    // Очередь отложенных операций (для защиты от deadlock)
    std::queue<DeferredOperation> m_deferredOperations;
    mutable std::mutex m_deferredMutex;

    // Константы
    static constexpr size_t MAX_VPK_PATH_LENGTH = 1024;
    static constexpr size_t MAX_FILE_PATH_LENGTH = 512;
    static constexpr size_t MAX_FILE_SIZE = 1024 * 1024 * 1024;  // 1GB

    // Thread-local флаг для предотвращения рекурсивной блокировки
    // Объявляется в .cpp файле
    friend class VPKManagerInitializer;
};

// Класс-инициализатор для thread_local переменной
class VPKManagerInitializer {
public:
    static void Init();
};