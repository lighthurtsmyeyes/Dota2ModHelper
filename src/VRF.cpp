#include "VRF.h"
#include "CrashLogger.h"
#include "SteamManager.h"
#include <windows.h>
#include <random>
#include <vector>
#include <sstream>
#include <fstream>
#include <mutex>
#include <unordered_set>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <cwchar>
#include <tlhelp32.h>
#include "SecurityHardening.h"
namespace {
__declspec(noinline) void SH_AD_VRF() noexcept {
    if (SH_PebBeingDebugged()) g_integritySeed ^= 0xAABBCCDD;
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
}
}

namespace {
std::string GetExecutableDirectory() {
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        try { return fs::current_path().string(); }
        catch (const std::exception&) { return OBF_CSTR("."); }
    }
    return fs::path(buffer).parent_path().string();
}

fs::path GetProjectDirectory() {
    fs::path exeDir = GetExecutableDirectory();
    fs::path root = exeDir.root_path();
    fs::path check = exeDir;
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
    return exeDir;
}

fs::path GetDecompilerDir() {
    fs::path exeDir = GetExecutableDirectory();
    fs::path projectDir = GetProjectDirectory();
    fs::path exeDecompiler = exeDir / OBF_CSTR("decompiler");
    fs::path projectDecompiler = projectDir / OBF_CSTR("decompiler");

    if (fs::exists(exeDecompiler / OBF_CSTR("Source2Viewer-CLI.exe"))) {
        return exeDecompiler;
    }
    if (fs::exists(projectDecompiler / OBF_CSTR("Source2Viewer-CLI.exe"))) {
        return projectDecompiler;
    }
    // Default to project directory in dev layout, otherwise executable directory.
    return (projectDir != exeDir) ? projectDecompiler : exeDecompiler;
}

std::mutex g_activeProcessesMutex;
std::unordered_set<DWORD> g_activeProcessIDs;

void RegisterActiveProcess(DWORD pid) {
    std::lock_guard<std::mutex> lock(g_activeProcessesMutex);
    g_activeProcessIDs.insert(pid);
}

void UnregisterActiveProcess(DWORD pid) {
    std::lock_guard<std::mutex> lock(g_activeProcessesMutex);
    g_activeProcessIDs.erase(pid);
}

bool IsActiveProcess(DWORD pid) {
    std::lock_guard<std::mutex> lock(g_activeProcessesMutex);
    return g_activeProcessIDs.find(pid) != g_activeProcessIDs.end();
}

void KillProcessByPID(DWORD pid, DWORD waitMs) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
    if (!hProcess) return;
    TerminateProcess(hProcess, 1);
    WaitForSingleObject(hProcess, waitMs);
    CloseHandle(hProcess);
}

std::vector<DWORD> FindDecompilerPIDs() {
    std::vector<DWORD> pids;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return pids;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    if (Process32First(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, OBFW_CSTR(L"Source2Viewer-CLI.exe")) == 0) {
                pids.push_back(pe.th32ProcessID);
            }
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return pids;
}

struct DecompilerCleanupRegistrar {
    DecompilerCleanupRegistrar() {
        std::atexit([]() {
            VRF::TerminateAllDecompilerProcesses();
        });
    }
} g_decompilerCleanupRegistrar;
}


VRF& VRF::GetInstance()
{
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    static VRF instance;
    return instance;
}

static size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    size_t written = fwrite(ptr, size, nmemb, stream);
    //std::cout << "Записано " << written << " байт" << std::endl;
    return written;
}

static int debug_callback(CURL* handle, curl_infotype type, char* data, size_t size, void* userptr) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (type == CURLINFO_TEXT) {
        //std::cout << "cURL: " << std::string(data, size);
    }
    return 0;
}

bool download_file(const std::string& url, const std::string& output_filename) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    CURL* curl;
    CURLcode res;
    FILE* fp;

    if (fs::exists(output_filename)) {
        fs::remove(output_filename);
    }

    curl = curl_easy_init();
    if (!curl) {
        std::cerr << OBF_CSTR("Ошибка инициализации cURL") << std::endl;
        return false;
    }

    fp = fopen(output_filename.c_str(), OBF_CSTR("wb"));
    if (!fp) {
        std::cerr << OBF_CSTR("Не могу открыть файл для записи: ") << output_filename << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);

    curl_easy_setopt(curl, CURLOPT_USERAGENT, OBF_CSTR("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36"));
    // TLS verification is enabled by default; do not disable it.
    // If corporate proxy requires custom CA, use CURLOPT_CAINFO.

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_callback);

    //std::cout << "Начинаем загрузку с URL: " << url << std::endl;
    res = curl_easy_perform(curl);

    fclose(fp);

    if (res != CURLE_OK) {
        std::cerr << OBF_CSTR("cURL ошибка: ") << curl_easy_strerror(res) << std::endl;
        if (fs::exists(output_filename) && fs::file_size(output_filename) == 0) {
            fs::remove(output_filename);
        }
        curl_easy_cleanup(curl);
        return false;
    }

    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    std::cout << OBF_CSTR("HTTP код ответа: ") << response_code << std::endl;

    double downloaded_size;
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD, &downloaded_size);
    std::cout << OBF_CSTR("Размер скачанных данных: ") << downloaded_size << OBF_CSTR(" байт") << std::endl;

    curl_easy_cleanup(curl);

    if (!fs::exists(output_filename)) {
        std::cerr << OBF_CSTR("Файл не был создан") << std::endl;
        return false;
    }

    auto file_size = fs::file_size(output_filename);
    std::cout << OBF_CSTR("Размер файла на диске: ") << file_size << OBF_CSTR(" байт") << std::endl;

    if (file_size == 0) {
        std::cerr << OBF_CSTR("Файл скачан с нулевым размером") << std::endl;
        fs::remove(output_filename);
        return false;
    }

    if (response_code != 200) {
        std::cerr << OBF_CSTR("Сервер вернул ошибку: ") << response_code << std::endl;
        if (fs::exists(output_filename)) {
            fs::remove(output_filename);
        }
        return false;
    }

    std::cout << OBF_CSTR("Файл успешно скачан: ") << output_filename << OBF_CSTR(" (") << file_size << OBF_CSTR(" байт)") << std::endl;
    return true;
}


static bool IsPathSafeForCommand(const std::string& path) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    const std::string forbidden = OBF_CSTR("\"&|;<>()$%");
    return path.find_first_of(forbidden) == std::string::npos;
}

bool extract_zip(const std::string& zip_filename, const std::string& output_folder) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::error_code ec;
    std::filesystem::create_directories(output_folder, ec);

    int err = 0;
    zip* z = zip_open(zip_filename.c_str(), 0, &err);
    if (!z) {
        std::cerr << OBF_CSTR("Ошибка открытия архива: ") << err << std::endl;
        return false;
    }

    constexpr zip_int64_t kMaxEntrySize = 1024LL * 1024 * 1024; // 1 GiB
    bool any_extracted = false;
    int num_entries = zip_get_num_entries(z, 0);
    for (int i = 0; i < num_entries; i++) {
        struct zip_stat st;
        zip_stat_init(&st);
        zip_stat_index(z, i, 0, &st);

        // ZIP Slip protection
        if (!st.name || st.name[0] == '\0') continue;
        std::string entry_name = st.name;
        if (entry_name.find(OBF_CSTR("..")) != std::string::npos ||
            entry_name.front() == '/' || entry_name.front() == '\\') {
            std::cerr << OBF_CSTR("Skipping unsafe zip entry: ") << entry_name << std::endl;
            continue;
        }

        fs::path out_path = fs::path(output_folder) / entry_name;
        fs::path canonical_out = fs::weakly_canonical(out_path);
        fs::path canonical_base = fs::weakly_canonical(output_folder);
        if (canonical_out.string().find(canonical_base.string()) != 0) {
            std::cerr << OBF_CSTR("Blocked zip slip attempt: ") << entry_name << std::endl;
            continue;
        }

        if (st.size > kMaxEntrySize) {
            std::cerr << OBF_CSTR("Skipping oversized zip entry: ") << entry_name << std::endl;
            continue;
        }

        std::string file_path = out_path.string();

        fs::path parent = std::filesystem::path(file_path).parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, ec);
        }

        zip_file* f = zip_fopen_index(z, i, 0);
        if (!f) {
            std::cerr << OBF_CSTR("Ошибка открытия файла в архиве: ") << st.name << std::endl;
            continue;
        }

        bool read_ok = true;
        std::vector<char> buf;
        if (st.size > 0) {
            buf.resize(static_cast<size_t>(st.size));
            zip_int64_t read = zip_fread(f, buf.data(), static_cast<zip_uint64_t>(st.size));
            if (read < 0 || read != st.size) {
                std::cerr << OBF_CSTR("Failed to read zip entry: ") << st.name << std::endl;
                read_ok = false;
            }
        }
        zip_fclose(f);

        if (!read_ok) {
            continue;
        }

        FILE* out = fopen(file_path.c_str(), OBF_CSTR("wb"));
        if (out) {
            if (st.size > 0) {
                fwrite(buf.data(), 1, static_cast<size_t>(st.size), out);
            }
            fclose(out);
            any_extracted = true;
        }
        else {
            std::cerr << OBF_CSTR("Ошибка записи файла: ") << file_path << std::endl;
        }
    }

    zip_close(z);
    return any_extracted;
}

std::string find_executable(const std::string& directory) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string extension = entry.path().extension().string();
                if (extension == OBF_CSTR(".exe") || extension == OBF_CSTR(".bat") || extension == OBF_CSTR(".cmd")) {
                    std::string filename = entry.path().filename().string();
                    if (filename.find(OBF_CSTR("Source2Viewer")) != std::string::npos ||
                        filename.find(OBF_CSTR("CLI")) != std::string::npos) {
                        return entry.path().string();
                    }
                }
            }
        }

        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == OBF_CSTR(".exe")) {
                return entry.path().string();
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << OBF_CSTR("Ошибка при поиске исполняемого файла: ") << e.what() << std::endl;
    }

    return OBF_CSTR("");
}

bool run_program_createprocess_capture(const std::string& program_path, const std::string& arguments, std::string& output);
bool run_program_sync_capture(const std::string& program_path, const std::string& arguments,
    const std::string& working_dir, std::string& output);
bool process_file_with_tool(const std::string& extract_dir,
    const std::string& input_file,
    const std::string& output_file, const std::string& add_args, std::string& output);
bool process_file_with_tool2(const std::string& extract_dir, const std::string& args, std::string& output);

bool run_program_sync(const std::string& program_path, const std::string& arguments, const std::string& working_dir = OBF_CSTR("")) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (!IsPathSafeForCommand(program_path)) {
        std::cerr << OBF_CSTR("Unsafe characters in program path, aborting.") << std::endl;
        return false;
    }
    if (!fs::exists(program_path)) {
        std::cerr << OBF_CSTR("Исполняемый файл не найден: ") << program_path << std::endl;
        return false;
    }

    std::string command;
    if (working_dir.empty()) {
        command = OBF_CSTR("\"") + program_path + OBF_CSTR("\" ") + arguments;
    }
    else {
        command = OBF_CSTR("cd /D \"") + working_dir + OBF_CSTR("\" && \"") + program_path + OBF_CSTR("\" ") + arguments;
    }

    //std::cout << "Выполняем команду: " << command << std::endl;

    int result = std::system(command.c_str());

    if (result == 0) {
        //std::cout << "Программа завершилась успешно" << std::endl;
        return true;
    }
    else {
        //std::cerr << "Программа завершилась с ошибкой. Код: " << result << std::endl;
        return false;
    }
}

bool run_program_createprocess(const std::string& program_path, const std::string& arguments) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (!IsPathSafeForCommand(program_path)) {
        std::cerr << OBF_CSTR("Unsafe characters in program path, aborting.") << std::endl;
        return false;
    }
    if (!fs::exists(program_path)) {
        std::cerr << OBF_CSTR("Исполняемый файл не найден: ") << program_path << std::endl;
        return false;
    }

    std::string full_command = OBF_CSTR("\"") + program_path + OBF_CSTR("\" ") + arguments;

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Создаем модифицируемую копию командной строки
    char* cmd = new char[full_command.length() + 1];
    strcpy(cmd, full_command.c_str());

    std::string working_dir = fs::path(program_path).parent_path().string();
    char* working_dir_buf = new char[working_dir.length() + 1];
    strcpy(working_dir_buf, working_dir.c_str());

    BOOL success = SH_CreateProcessA(
        NULL,           // Имя модуля
        cmd,            // Командная строка
        NULL,           // Дескриптор процесса
        NULL,           // Дескриптор потока
        FALSE,          // Наследование дескрипторов
        CREATE_NO_WINDOW, // Флаги создания
        NULL,           // Environment block
        working_dir_buf, // Рабочая директория
        &si,            // STARTUPINFO
        &pi             // PROCESS_INFORMATION
    );

    // Освобождаем память
    delete[] cmd;
    delete[] working_dir_buf;

    if (!success) {
        std::cerr << OBF_CSTR("Ошибка создания процесса: ") << GetLastError() << std::endl;
        return false;
    }

    RegisterActiveProcess(pi.dwProcessId);
    SH_WaitForSingleObject(pi.hProcess, INFINITE);
    UnregisterActiveProcess(pi.dwProcessId);

    DWORD exit_code;
    SH_GetExitCodeProcess(pi.hProcess, &exit_code);

    SH_CloseHandle(pi.hProcess);
    SH_CloseHandle(pi.hThread);

    return (exit_code == 0);
}

std::string make_absolute_path(const std::string& path) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    try {
        return fs::absolute(path).string();
    }
    catch (const std::exception& e) {
        std::cerr << OBF_CSTR("Ошибка создания абсолютного пути: ") << e.what() << std::endl;
        return path;
    }
}

std::string generate_unique_suffix() {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    return std::to_string(dis(gen));
}

bool process_file_with_tool(const std::string& extract_dir,
    const std::string& input_file,
    const std::string& output_file = OBF_CSTR(""), const std::string& add_args = OBF_CSTR("")) {
    std::string unused;
    return process_file_with_tool(extract_dir, input_file, output_file, add_args, unused);
}

bool process_file_with_tool(const std::string& extract_dir,
    const std::string& input_file,
    const std::string& output_file, const std::string& add_args, std::string& output) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    output.clear();
    std::string executable = find_executable(extract_dir);

    if (executable.empty()) {
        std::string details = OBF_CSTR("Source2Viewer-CLI executable not found in: ") + extract_dir;
        try {
            details += OBF_CSTR("\nDirectory contents:\n");
            for (const auto& entry : fs::recursive_directory_iterator(extract_dir)) {
                if (entry.is_regular_file()) {
                    details += OBF_CSTR("  ") + entry.path().string() + OBF_CSTR("\n");
                }
            }
        }
        catch (const std::exception& e) {
            details += OBF_CSTR("\nFailed to list directory: ") + std::string(e.what());
        }
        output = details;
        ERROR_LOG(OBF_CSTR("process_file_with_tool"), details);
        return false;
    }

    VRF::TerminateLingeringDecompilerProcesses();

    std::string abs_executable = make_absolute_path(executable);
    std::string abs_input_file = make_absolute_path(input_file);

    if (!IsPathSafeForCommand(abs_executable) || !IsPathSafeForCommand(abs_input_file)) {
        output = OBF_CSTR("Unsafe characters in paths, aborting.");
        std::cerr << output << std::endl;
        return false;
    }

    if (!fs::exists(abs_input_file)) {
        output = OBF_CSTR("Input file not found: ") + abs_input_file + OBF_CSTR("\nCurrent directory: ") + fs::current_path().string();
        std::cerr << output << std::endl;
        return false;
    }

    std::string abs_output_file;
    if (output_file.empty()) {
        fs::path input_path(abs_input_file);
        fs::path output_path = input_path;
        output_path.replace_extension(OBF_CSTR(".decoded"));
        abs_output_file = output_path.string();
    }
    else {
        abs_output_file = make_absolute_path(output_file);
    }

    std::string arguments = OBF_CSTR("-i \"") + abs_input_file + OBF_CSTR("\" -o \"") + abs_output_file + OBF_CSTR("\"") + OBF_CSTR(" ") + add_args;

    if (run_program_createprocess_capture(abs_executable, arguments, output)) {
        return true;
    }

    std::string working_dir = fs::path(abs_executable).parent_path().string();
    return run_program_sync_capture(abs_executable, arguments, working_dir, output);
}

bool process_file_with_tool2(const std::string& extract_dir, const std::string& args) {
    std::string unused;
    return process_file_with_tool2(extract_dir, args, unused);
}

bool process_file_with_tool2(const std::string& extract_dir, const std::string& args, std::string& output) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::string executable = find_executable(extract_dir);

    if (executable.empty()) {
        std::string details = OBF_CSTR("Source2Viewer-CLI executable not found in: ") + extract_dir;
        try {
            details += OBF_CSTR("\nDirectory contents:\n");
            for (const auto& entry : fs::recursive_directory_iterator(extract_dir)) {
                if (entry.is_regular_file()) {
                    details += OBF_CSTR("  ") + entry.path().string() + OBF_CSTR("\n");
                }
            }
        }
        catch (const std::exception& e) {
            details += OBF_CSTR("\nFailed to list directory: ") + std::string(e.what());
        }
        output = details;
        ERROR_LOG(OBF_CSTR("process_file_with_tool"), details);
        return false;
    }

    VRF::TerminateLingeringDecompilerProcesses();

    std::string abs_executable = make_absolute_path(executable);

    if (!IsPathSafeForCommand(abs_executable)) {
        output = OBF_CSTR("Unsafe characters in executable path, aborting.");
        std::cerr << output << std::endl;
        return false;
    }

    std::string arguments = args;

    if (run_program_createprocess_capture(abs_executable, arguments, output)) {
        return true;
    }

    std::string working_dir = fs::path(abs_executable).parent_path().string();
    return run_program_sync_capture(abs_executable, arguments, working_dir, output);
}

bool run_program_sync_capture(const std::string& program_path, const std::string& arguments,
    const std::string& working_dir, std::string& output) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (!fs::exists(program_path)) {
        std::cerr << OBF_CSTR("Исполняемый файл не найден: ") << program_path << std::endl;
        return false;
    }

    std::string temp_output_file = OBF_CSTR("temp_output_") + generate_unique_suffix() + OBF_CSTR(".txt");
    std::string command;

    if (working_dir.empty()) {
        command = OBF_CSTR("\"") + program_path + OBF_CSTR("\" ") + arguments + OBF_CSTR(" > \"") + temp_output_file + OBF_CSTR("\" 2>&1");
    }
    else {
        command = OBF_CSTR("cd /D \"") + working_dir + OBF_CSTR("\" && \"") + program_path + OBF_CSTR("\" ") + arguments + OBF_CSTR(" > \"") + temp_output_file + OBF_CSTR("\" 2>&1");
    }

    std::cout << OBF_CSTR("Выполняем команду с захватом вывода: ") << command << std::endl;

    int result = std::system(command.c_str());

    std::ifstream file(temp_output_file);
    if (file) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        output = buffer.str();
        file.close();
    }

    if (fs::exists(temp_output_file)) {
        fs::remove(temp_output_file);
    }

    if (result == 0) {
        std::cout << OBF_CSTR("Программа завершилась успешно, вывод захвачен (") << output.size() << OBF_CSTR(" байт)") << std::endl;
        return true;
    }
    else {
        std::cerr << OBF_CSTR("Программа завершилась с ошибкой. Код: ") << result << std::endl;
        std::cerr << OBF_CSTR("Вывод программы: ") << output << std::endl;
        return false;
    }
}

bool run_program_createprocess_capture(const std::string& program_path, const std::string& arguments,
    std::string& output) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    if (!fs::exists(program_path)) {
        std::cerr << OBF_CSTR("Исполняемый файл не найден: ") << program_path << std::endl;
        return false;
    }

    std::string full_command = OBF_CSTR("\"") + program_path + OBF_CSTR("\" ") + arguments;

    SECURITY_ATTRIBUTES sa;
    HANDLE hRead, hWrite;

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!SH_CreatePipe(&hRead, &hWrite, &sa, 0)) {
        std::cerr << OBF_CSTR("Ошибка создания pipe: ") << GetLastError() << std::endl;
        return false;
    }
    // The child must inherit only hWrite (its stdout/stderr), never hRead.
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = NULL;

    ZeroMemory(&pi, sizeof(pi));

    char* cmd = new char[full_command.length() + 1];
    strcpy(cmd, full_command.c_str());

    std::string working_dir = fs::path(program_path).parent_path().string();
    char* working_dir_buf = new char[working_dir.length() + 1];
    strcpy(working_dir_buf, working_dir.c_str());

    BOOL success = SH_CreateProcessA(
        NULL,           // Имя модуля
        cmd,            // Командная строка
        NULL,           // Дескриптор процесса
        NULL,           // Дескриптор потока
        TRUE,           // Наследование дескрипторов (важно для pipe)
        CREATE_NO_WINDOW, // Флаги создания
        NULL,           // Environment block
        working_dir_buf, // Рабочая директория
        &si,            // STARTUPINFO
        &pi             // PROCESS_INFORMATION
    );

    delete[] cmd;
    delete[] working_dir_buf;

    if (!success) {
        std::cerr << OBF_CSTR("Ошибка создания процесса: ") << GetLastError() << std::endl;
        SH_CloseHandle(hRead);
        SH_CloseHandle(hWrite);
        return false;
    }

    SH_CloseHandle(hWrite);

    CHAR buffer[4096];
    std::stringstream outputStream;

    // Source2Viewer-CLI often finishes its work but never exits (lingers as a
    // process), which would make an infinite WaitForSingleObject hang the app
    // forever. Drain the pipe without blocking and stop waiting once the CLI
    // either exits, goes idle after producing output, or hits a hard cap.
    const ULONGLONG kIdleKillMs = 15000;                 // 15s of no new output after work started -> done
    const ULONGLONG kIdleKillNoDataMs = 30000;           // 30s with no output at all -> done
    const ULONGLONG kHardCapMs = 10ULL * 60 * 1000;      // 10 min absolute safety cap

    ULONGLONG tStart = GetTickCount64();
    ULONGLONG tLastData = tStart;
    bool gotAnyData = false;
    bool processExited = false;

    while (true) {
        DWORD avail = 0;
        while (PeekNamedPipe(hRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
            DWORD toRead = (avail > sizeof(buffer) - 1) ? (sizeof(buffer) - 1) : avail;
            DWORD bytesRead = 0;
            if (!SH_ReadFile(hRead, buffer, toRead, &bytesRead, NULL) || bytesRead == 0) { avail = 0; break; }
            buffer[bytesRead] = '\0';
            outputStream << buffer;
            gotAnyData = true;
            tLastData = GetTickCount64();
        }

        if (SH_WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) { processExited = true; break; }

        ULONGLONG now = GetTickCount64();
        ULONGLONG idle = now - tLastData;
        if ((gotAnyData && idle > kIdleKillMs) || idle > kIdleKillNoDataMs) { break; }
        if (now - tStart > kHardCapMs) { break; }
        SH_Sleep(50);
    }

    // Drain anything left in the pipe after the process exited/was terminated.
    DWORD availTail = 0;
    while (PeekNamedPipe(hRead, NULL, 0, NULL, &availTail, NULL) && availTail > 0) {
        DWORD toRead = (availTail > sizeof(buffer) - 1) ? (sizeof(buffer) - 1) : availTail;
        DWORD bytesRead = 0;
        if (!SH_ReadFile(hRead, buffer, toRead, &bytesRead, NULL) || bytesRead == 0) break;
        buffer[bytesRead] = '\0';
        outputStream << buffer;
    }

    output = outputStream.str();

    DWORD exit_code = 0;
    if (processExited) {
        SH_GetExitCodeProcess(pi.hProcess, &exit_code);
    } else {
        // Finished working but did not exit on its own -> terminate the lingerer.
        TerminateProcess(pi.hProcess, 0);
        SH_WaitForSingleObject(pi.hProcess, 3000);
        exit_code = 0;
    }

    SH_CloseHandle(pi.hProcess);
    SH_CloseHandle(pi.hThread);
    SH_CloseHandle(hRead);

    if (exit_code == 0) {
        std::cout << OBF_CSTR("Программа завершилась успешно, вывод захвачен (") << output.size() << OBF_CSTR(" байт)") << std::endl;
        return true;
    }
    else {
        std::cerr << OBF_CSTR("Программа завершилась с ошибкой. Код: ") << exit_code << std::endl;
        std::cerr << OBF_CSTR("Вывод программы: ") << output << std::endl;
        return false;
    }
}

bool process_file_with_tool_block(const std::string& extract_dir, const std::string& input_file,
    const std::string& block_name, std::string& output, const std::string& add_args = OBF_CSTR("")) {
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    std::string executable = find_executable(extract_dir);

    if (executable.empty()) {
        std::cerr << OBF_CSTR("Не найден исполняемый файл в директории: ") << extract_dir << std::endl;
        return false;
    }

    VRF::TerminateLingeringDecompilerProcesses();

    std::string abs_executable = make_absolute_path(executable);
    std::string abs_input_file = make_absolute_path(input_file);

    if (!fs::exists(abs_input_file)) {
        std::cerr << OBF_CSTR("Входной файл не найден: ") << abs_input_file << std::endl;
        return false;
    }

    std::string arguments = OBF_CSTR("-i \"") + abs_input_file + OBF_CSTR("\"");
    if (block_name.empty()) {
        arguments += OBF_CSTR(" -a");
    }
    else {
        arguments += OBF_CSTR(" -b ") + block_name;
    }
    if (!add_args.empty()) {
        arguments += OBF_CSTR(" ") + add_args;
    }

    //std::cout << "Извлекаем блок '" << block_name << "' из файла: " << abs_input_file << std::endl;
    //std::cout << "Аргументы команды: " << arguments << std::endl;

    //std::cout << "Пробуем запустить через CreateProcess с захватом вывода..." << std::endl;
    if (run_program_createprocess_capture(abs_executable, arguments, output)) {
        return true;
    }

    //std::cout << "Пробуем запустить через system с захватом вывода..." << std::endl;
    std::string working_dir = fs::path(abs_executable).parent_path().string();
    return run_program_sync_capture(abs_executable, arguments, working_dir, output);
}

bool VRF::IsSetupNeeded()
{
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    fs::path decompilerDir = GetDecompilerDir();
    static const std::vector<std::string> requiredFiles = {
        OBF_CSTR("Source2Viewer-CLI.exe"),
        OBF_CSTR("spirv-cross.dll"),
        OBF_CSTR("libSkiaSharp.dll"),
        OBF_CSTR("TinyEXRNative.dll")
    };
    for (const auto& file : requiredFiles) {
        if (!fs::exists(decompilerDir / file)) return true;
    }
    return false;
}

bool VRF::Setup()
{
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::string url = R"(https://github.com/ValveResourceFormat/ValveResourceFormat/releases/download/19.2/cli-windows-x64.zip)";
    fs::path decompilerDir = GetDecompilerDir();
    std::error_code ec;
    fs::create_directories(decompilerDir, ec);
    std::string zip_filename = (decompilerDir / OBF_CSTR("cli-windows-x64.zip")).string();
    std::string extract_to = decompilerDir.string();

    if (!download_file(url, zip_filename)) {
        curl_global_cleanup();
        return false;
    }

    if (!extract_zip(zip_filename, extract_to)) {
        curl_global_cleanup();
        return false;
    }

    fs::remove(zip_filename, ec);

    if (IsSetupNeeded()) {
        curl_global_cleanup();
        return false;
    }

    curl_global_cleanup();
    return true;
}

void VRF::Decompile(std::string input, std::string output, std::string args)
{
    std::string unused;
    DecompileWithOutput(std::move(input), std::move(output), std::move(args), unused);
}

bool VRF::DecompileWithOutput(std::string input, std::string output, std::string args, std::string& outLog)
{
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    bool ok = process_file_with_tool(GetDecompilerDir().string(), input, output, args, outLog);
    if (!ok) {
        std::cerr << OBF_CSTR("Ошибка при обработке файла!") << std::endl;
    }
    return ok;
}

void VRF::Decompile(std::string args)
{
    std::string unused;
    DecompileWithOutput(std::move(args), unused);
}

bool VRF::DecompileWithOutput(std::string args, std::string& outLog)
{
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    bool ok = process_file_with_tool2(GetDecompilerDir().string(), args, outLog);
    if (!ok) {
        std::cerr << OBF_CSTR("Ошибка при обработке файла!") << std::endl;
    }
    return ok;
}

bool VRF::DecompileBlock(std::string input, std::string block_name, std::string& output, std::string args)
{
    SH_AD_VRF();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
    bool ok = process_file_with_tool_block(GetDecompilerDir().string(), input, block_name, output, args);
    if (ok) {
        //std::cout << "Блок '" << block_name << "' успешно извлечен из файла: " << input << std::endl;
        //std::cout << "Размер извлеченных данных: " << output.size() << " байт" << std::endl;
    }
    else {
        std::cerr << OBF_CSTR("Ошибка при извлечении блока '") << block_name << OBF_CSTR("' из файла: ") << input << std::endl;
        output.clear();
    }
    return ok;
}

void VRF::TerminateLingeringDecompilerProcesses() {
    SH_AD_VRF();
    for (DWORD pid : FindDecompilerPIDs()) {
        if (IsActiveProcess(pid)) continue;
        KillProcessByPID(pid, 500);
    }
}

void VRF::TerminateAllDecompilerProcesses() {
    SH_AD_VRF();
    for (DWORD pid : FindDecompilerPIDs()) {
        KillProcessByPID(pid, 1000);
    }
}

std::string VRF::GenerateUniqueSuffix() {
    SH_AD_VRF();
    return generate_unique_suffix();
}
