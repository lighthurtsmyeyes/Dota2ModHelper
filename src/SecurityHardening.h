#pragma once

#include <windows.h>
#include <winternl.h>
#include <intrin.h>
#include <shlobj.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <string>
#include <iostream>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

// ============================================================================
// Runtime obfuscation seed (randomized once per process start)
// ============================================================================
inline volatile size_t g_obfSeed = 0;

__forceinline size_t SH_GetObfSeed() noexcept {
    size_t seed = g_obfSeed;
    if (seed == 0) {
        seed = static_cast<size_t>(__rdtsc());
        seed ^= static_cast<size_t>(GetTickCount());
        seed ^= reinterpret_cast<size_t>(&seed);
        seed |= 1;
        g_obfSeed = seed;
    }
    return seed;
}

// ============================================================================
// Runtime integrity seed
// ============================================================================
inline volatile DWORD g_integritySeed = 0x9E3779B9;
inline volatile BOOL  g_debugDetected = FALSE;

// ============================================================================
// Runtime XOR string obfuscation (runtime key � compiler cannot constant-fold)
// ============================================================================
template<size_t N>
struct ObfStr {
    char data[N];
    size_t key;

    ObfStr(const char* str, size_t seed) noexcept : key(seed) {
        for (size_t i = 0; i < N; ++i) {
            data[i] = static_cast<char>(str[i] ^ static_cast<char>((key + i * 0x9E) & 0xFF));
        }
    }

    __declspec(noinline) void decrypt(char* out) const noexcept {
        for (size_t i = 0; i < N; ++i) {
            out[i] = static_cast<char>(data[i] ^ static_cast<char>((key + i * 0x9E) & 0xFF));
        }
        out[N - 1] = '\0';
    }

    __declspec(noinline) void wipe(volatile char* out) const noexcept {
        for (size_t i = 0; i < N; ++i) out[i] = 0;
    }

    __declspec(noinline) const char* c_str() const noexcept {
        static thread_local char bufs[8][N];
        static thread_local int idx = 0;
        decrypt(bufs[idx]);
        const char* result = bufs[idx];
        idx = (idx + 1) & 7;
        return result;
    }
};

template<size_t N>
struct ObfStrW {
    wchar_t data[N];
    size_t key;

    ObfStrW(const wchar_t* str, size_t seed) noexcept : key(seed) {
        for (size_t i = 0; i < N; ++i) {
            data[i] = static_cast < wchar_t > (str[i] ^ static_cast < wchar_t > ((key + i * 0x9E) & 0xFFFF));
        }
    }

    __declspec(noinline) void decrypt(wchar_t* out) const noexcept {
        for (size_t i = 0; i < N; ++i) {
            out[i] = static_cast < wchar_t > (data[i] ^ static_cast < wchar_t > ((key + i * 0x9E) & 0xFFFF));
        }
        out[N - 1] = L'\0';
    }

    __declspec(noinline) void wipe(volatile wchar_t* out) const noexcept {
        for (size_t i = 0; i < N; ++i) out[i] = 0;
    }

    __declspec(noinline) const wchar_t* c_str() const noexcept {
        static thread_local wchar_t bufs[8][N];
        static thread_local int idx = 0;
        decrypt(bufs[idx]);
        const wchar_t* result = bufs[idx];
        idx = (idx + 1) & 7;
        return result;
    }
};

#define OBF(str)  (ObfStr<sizeof(str)>(str, SH_GetObfSeed()))
#define OBFW(str) (ObfStrW<sizeof(str) / sizeof(wchar_t)>(str, SH_GetObfSeed()))

#define OBF_CSTR(str) (OBF(str).c_str())
#define OBF_STR(str)  (std::string(OBF(str).c_str()))
#define OBFW_CSTR(str) (OBFW(str).c_str())
#define OBFW_STR(str)  (std::wstring(OBFW(str).c_str()))

// ============================================================================
// Implicit-conversion helpers
// ============================================================================
class ObfString : public std::string {
public:
    template <size_t N>
    ObfString(const ObfStr<N>& o) : std::string(o.c_str()) {}
    __forceinline operator const char* () const noexcept { return c_str(); }
};

class ObfWString : public std::wstring {
public:
    template <size_t N>
    ObfWString(const ObfStrW<N>& o) : std::wstring(o.c_str()) {}
    __forceinline operator const wchar_t* () const noexcept { return c_str(); }
};

#define OBF_AUTO(str)  (ObfString(OBF(str)))
#define OBFW_AUTO(str) (ObfWString(OBFW(str)))

// ============================================================================
// Anti-debug helpers
// ============================================================================
__forceinline BOOL SH_PebBeingDebugged() noexcept {
#ifdef _WIN64
    return *reinterpret_cast < BYTE* > (__readgsqword(0x60) + 0x2);
#else
    return *reinterpret_cast < BYTE* > (__readfsdword(0x30) + 0x2);
#endif
}

__forceinline BOOL SH_NtGlobalFlag() noexcept {
#ifdef _WIN64
    DWORD ntGlobal = *reinterpret_cast<DWORD*>(__readgsqword(0x60) + 0xBC);
#else
    DWORD ntGlobal = *reinterpret_cast<DWORD*>(__readfsdword(0x30) + 0x68);
#endif
    return (ntGlobal & 0x70) != 0;
}

__forceinline BOOL SH_RdtscTiming() noexcept {
    ULONGLONG t1 = __rdtsc();
    volatile int dummy = 0;
    for (int i = 0; i < 100; ++i) dummy ^= i;
    ULONGLONG t2 = __rdtsc();
    (void)dummy;
    return (t2 - t1) > 0xFFFFF;
}

__forceinline BOOL SH_HardwareBreakpoints() noexcept {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(GetCurrentThread(), &ctx)) {
        return (ctx.Dr0 | ctx.Dr1 | ctx.Dr2 | ctx.Dr3) != 0;
    }
    return FALSE;
}

using PFN_NtQueryInformationProcess = NTSTATUS(NTAPI*)(HANDLE, DWORD, PVOID, ULONG, PULONG);

__forceinline PFN_NtQueryInformationProcess SH_GetNtQueryInformationProcess() noexcept {
    static volatile PFN_NtQueryInformationProcess fn = nullptr;
    if (!fn) {
        char ntdll[16];  OBF("ntdll.dll").decrypt(ntdll);
        char name[32];   OBF("NtQueryInformationProcess").decrypt(name);
        HMODULE hNtdll = GetModuleHandleA(ntdll);
        if (!hNtdll) hNtdll = LoadLibraryA(ntdll);
        if (hNtdll) {
            fn = reinterpret_cast < PFN_NtQueryInformationProcess > (
                reinterpret_cast<void*>(GetProcAddress(hNtdll, name)));
        }
        OBF("ntdll.dll").wipe(ntdll);
        OBF("NtQueryInformationProcess").wipe(name);
    }
    return fn;
}

__forceinline BOOL SH_DebugPort() noexcept {
    auto fn = SH_GetNtQueryInformationProcess();
    if (!fn) return FALSE;
    DWORD_PTR debugPort = 0;
    NTSTATUS st = fn(GetCurrentProcess(), 7, &debugPort, sizeof(debugPort), nullptr);
    return NT_SUCCESS(st) && debugPort != 0;
}

__forceinline BOOL SH_CheckRemoteDebuggerPresent() noexcept {
    using PFN_CRP = BOOL(WINAPI*)(HANDLE, PBOOL);
    static volatile PFN_CRP fn = nullptr;
    if (!fn) {
        char dll[32];  OBF("kernel32.dll").decrypt(dll);
        char name[32]; OBF("CheckRemoteDebuggerPresent").decrypt(name);
        HMODULE h = GetModuleHandleA(dll);
        if (!h) h = LoadLibraryA(dll);
        if (h) fn = reinterpret_cast < PFN_CRP > (reinterpret_cast<void*>(GetProcAddress(h, name)));
        OBF("kernel32.dll").wipe(dll);
        OBF("CheckRemoteDebuggerPresent").wipe(name);
    }
    if (!fn) return FALSE;
    BOOL present = FALSE;
    fn(GetCurrentProcess(), &present);
    return present;
}

__forceinline BOOL SH_IsDebuggerPresent() noexcept {
    using PFN_IDP = BOOL(WINAPI*)();
    static volatile PFN_IDP fn = nullptr;
    if (!fn) {
        char dll[32];  OBF("kernel32.dll").decrypt(dll);
        char name[32]; OBF("IsDebuggerPresent").decrypt(name);
        HMODULE h = GetModuleHandleA(dll);
        if (!h) h = LoadLibraryA(dll);
        if (h) fn = reinterpret_cast < PFN_IDP > (reinterpret_cast<void*>(GetProcAddress(h, name)));
        OBF("kernel32.dll").wipe(dll);
        OBF("IsDebuggerPresent").wipe(name);
    }
    if (!fn) return FALSE;
    return fn();
}

// ============================================================================
// Dynamic API resolution
// ============================================================================
#define DYN_RESOLVE_A(dllClear, funcClear, type) \
    ([]() -> type { \
        static volatile type _fn = nullptr; \
        if (!_fn) { \
            char _dll[64];  OBF(dllClear).decrypt(_dll); \
            char _func[64]; OBF(funcClear).decrypt(_func); \
            HMODULE _h = GetModuleHandleA(_dll); \
            if (!_h) _h = LoadLibraryA(_dll); \
            if (_h) _fn = reinterpret_cast<type>(reinterpret_cast<void*>(GetProcAddress(_h, _func))); \
            OBF(dllClear).wipe(_dll); \
            OBF(funcClear).wipe(_func); \
        } \
        return _fn; \
    }())

// ============================================================================
// WinAPI wrappers (��������� ��� ���������)
// ============================================================================
__forceinline HMODULE SH_LoadLibraryA(LPCSTR lpLibFileName) {
    using PFN = HMODULE(WINAPI*)(LPCSTR);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "LoadLibraryA", PFN);
    return fn ? fn(lpLibFileName) : nullptr;
}

__forceinline HMODULE SH_GetModuleHandleA(LPCSTR lpModuleName) {
    using PFN = HMODULE(WINAPI*)(LPCSTR);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "GetModuleHandleA", PFN);
    return fn ? fn(lpModuleName) : nullptr;
}

__forceinline FARPROC SH_GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    using PFN = FARPROC(WINAPI*)(HMODULE, LPCSTR);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "GetProcAddress", PFN);
    return fn ? fn(hModule, lpProcName) : nullptr;
}

__forceinline HANDLE SH_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    using PFN = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "CreateFileA", PFN);
    return fn ? fn(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile) : INVALID_HANDLE_VALUE;
}

__forceinline LPVOID SH_VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    using PFN = LPVOID(WINAPI*)(LPVOID, SIZE_T, DWORD, DWORD);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "VirtualAlloc", PFN);
    return fn ? fn(lpAddress, dwSize, flAllocationType, flProtect) : nullptr;
}

__forceinline BOOL SH_VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect) {
    using PFN = BOOL(WINAPI*)(LPVOID, SIZE_T, DWORD, PDWORD);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "VirtualProtect", PFN);
    return fn ? fn(lpAddress, dwSize, flNewProtect, lpflOldProtect) : FALSE;
}

__forceinline BOOL SH_CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
    LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation) {
    using PFN = BOOL(WINAPI*)(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
        BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "CreateProcessA", PFN);
    return fn ? fn(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
        bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory,
        lpStartupInfo, lpProcessInformation) : FALSE;
}

__forceinline HANDLE SH_CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
    LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId) {
    using PFN = HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "CreateThread", PFN);
    return fn ? fn(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId) : nullptr;
}

__forceinline DWORD SH_WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    using PFN = DWORD(WINAPI*)(HANDLE, DWORD);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "WaitForSingleObject", PFN);
    return fn ? fn(hHandle, dwMilliseconds) : WAIT_FAILED;
}

__forceinline BOOL SH_GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode) {
    using PFN = BOOL(WINAPI*)(HANDLE, LPDWORD);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "GetExitCodeProcess", PFN);
    return fn ? fn(hProcess, lpExitCode) : FALSE;
}

__forceinline BOOL SH_CloseHandle(HANDLE hObject) {
    using PFN = BOOL(WINAPI*)(HANDLE);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "CloseHandle", PFN);
    return fn ? fn(hObject) : FALSE;
}

__forceinline BOOL SH_CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe,
    LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize) {
    using PFN = BOOL(WINAPI*)(PHANDLE, PHANDLE, LPSECURITY_ATTRIBUTES, DWORD);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "CreatePipe", PFN);
    return fn ? fn(hReadPipe, hWritePipe, lpPipeAttributes, nSize) : FALSE;
}

__forceinline BOOL SH_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    using PFN = BOOL(WINAPI*)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "ReadFile", PFN);
    return fn ? fn(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped) : FALSE;
}

__forceinline DWORD SH_GetLogicalDrives(void) {
    using PFN = DWORD(WINAPI*)();
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "GetLogicalDrives", PFN);
    return fn ? fn() : 0;
}

__forceinline int SH_MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr,
    int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar) {
    using PFN = int(WINAPI*)(UINT, DWORD, LPCCH, int, LPWSTR, int);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "MultiByteToWideChar", PFN);
    return fn ? fn(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar) : 0;
}

__forceinline int SH_WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWCH lpWideCharStr,
    int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar, LPBOOL lpUsedDefaultChar) {
    using PFN = int(WINAPI*)(UINT, DWORD, LPCWCH, int, LPSTR, int, LPCCH, LPBOOL);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "WideCharToMultiByte", PFN);
    return fn ? fn(CodePage, dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cbMultiByte, lpDefaultChar, lpUsedDefaultChar) : 0;
}

__forceinline LSTATUS SH_RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    using PFN = LSTATUS(WINAPI*)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("advapi32.dll", "RegOpenKeyExA", PFN);
    return fn ? fn(hKey, lpSubKey, ulOptions, samDesired, phkResult) : ERROR_MOD_NOT_FOUND;
}

__forceinline LSTATUS SH_RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
    LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    using PFN = LSTATUS(WINAPI*)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("advapi32.dll", "RegQueryValueExA", PFN);
    return fn ? fn(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData) : ERROR_MOD_NOT_FOUND;
}

__forceinline LSTATUS SH_RegCloseKey(HKEY hKey) {
    using PFN = LSTATUS(WINAPI*)(HKEY);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("advapi32.dll", "RegCloseKey", PFN);
    return fn ? fn(hKey) : ERROR_MOD_NOT_FOUND;
}

__forceinline HRESULT SH_SHGetFolderPathA(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR pszPath) {
    using PFN = HRESULT(WINAPI*)(HWND, int, HANDLE, DWORD, LPSTR);
    static volatile PFN fn = nullptr;
    if (!fn) {
        char dll[32];  OBF("shell32.dll").decrypt(dll);
        char name[32]; OBF("SHGetFolderPathA").decrypt(name);
        HMODULE h = GetModuleHandleA(dll);
        if (!h) h = LoadLibraryA(dll);
        if (h) fn = reinterpret_cast < PFN > (reinterpret_cast<void*>(GetProcAddress(h, name)));
        OBF("shell32.dll").wipe(dll);
        OBF("SHGetFolderPathA").wipe(name);
    }
    return fn ? fn(hwnd, csidl, hToken, dwFlags, pszPath) : E_FAIL;
}

__forceinline HRSRC SH_FindResourceW(HMODULE hModule, LPCWSTR lpName, LPCWSTR lpType) {
    using PFN = HRSRC(WINAPI*)(HMODULE, LPCWSTR, LPCWSTR);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "FindResourceW", PFN);
    return fn ? fn(hModule, lpName, lpType) : nullptr;
}

__forceinline HGLOBAL SH_LoadResource(HMODULE hModule, HRSRC hResInfo) {
    using PFN = HGLOBAL(WINAPI*)(HMODULE, HRSRC);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "LoadResource", PFN);
    return fn ? fn(hModule, hResInfo) : nullptr;
}

__forceinline DWORD SH_SizeofResource(HMODULE hModule, HRSRC hResInfo) {
    using PFN = DWORD(WINAPI*)(HMODULE, HRSRC);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "SizeofResource", PFN);
    return fn ? fn(hModule, hResInfo) : 0;
}

__forceinline LPVOID SH_LockResource(HGLOBAL hResData) {
    using PFN = LPVOID(WINAPI*)(HGLOBAL);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "LockResource", PFN);
    return fn ? fn(hResData) : nullptr;
}

__forceinline HANDLE SH_CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    using PFN = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "CreateFileW", PFN);
    return fn ? fn(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile) : INVALID_HANDLE_VALUE;
}

__forceinline BOOL SH_WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
    LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped) {
    using PFN = BOOL(WINAPI*)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "WriteFile", PFN);
    return fn ? fn(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped) : FALSE;
}

__forceinline HMODULE SH_GetModuleHandleW(LPCWSTR lpModuleName) {
    using PFN = HMODULE(WINAPI*)(LPCWSTR);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "GetModuleHandleW", PFN);
    return fn ? fn(lpModuleName) : nullptr;
}

__forceinline void SH_OutputDebugStringA(LPCSTR lpOutputString) {
    using PFN = void(WINAPI*)(LPCSTR);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "OutputDebugStringA", PFN);
    if (fn) fn(lpOutputString);
}

__forceinline HINSTANCE SH_ShellExecuteA(HWND hwnd, LPCSTR lpOperation, LPCSTR lpFile,
    LPCSTR lpParameters, LPCSTR lpDirectory, INT nShowCmd) {
    using PFN = HINSTANCE(WINAPI*)(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("shell32.dll", "ShellExecuteA", PFN);
    return fn ? fn(hwnd, lpOperation, lpFile, lpParameters, lpDirectory, nShowCmd) : nullptr;
}

__forceinline BOOL SH_DwmSetWindowAttribute(HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute) {
    using PFN = BOOL(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("dwmapi.dll", "DwmSetWindowAttribute", PFN);
    return fn ? fn(hwnd, dwAttribute, pvAttribute, cbAttribute) : FALSE;
}

__forceinline BOOL SH_ShowWindow(HWND hWnd, int nCmdShow) {
    using PFN = BOOL(WINAPI*)(HWND, int);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "ShowWindow", PFN);
    return fn ? fn(hWnd, nCmdShow) : FALSE;
}

__forceinline BOOL SH_UpdateWindow(HWND hWnd) {
    using PFN = BOOL(WINAPI*)(HWND);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "UpdateWindow", PFN);
    return fn ? fn(hWnd) : FALSE;
}

__forceinline BOOL SH_TranslateMessage(const MSG* lpMsg) {
    using PFN = BOOL(WINAPI*)(const MSG*);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "TranslateMessage", PFN);
    return fn ? fn(lpMsg) : FALSE;
}

__forceinline LRESULT SH_DispatchMessage(const MSG* lpMsg) {
    using PFN = LRESULT(WINAPI*)(const MSG*);
    static volatile PFN fn = nullptr;
    if (!fn) {
#ifdef UNICODE
        fn = DYN_RESOLVE_A("user32.dll", "DispatchMessageW", PFN);
#else
        fn = DYN_RESOLVE_A("user32.dll", "DispatchMessageA", PFN);
#endif
    }
    return fn ? fn(lpMsg) : 0;
}

__forceinline BOOL SH_PeekMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
    using PFN = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
    static volatile PFN fn = nullptr;
    if (!fn) {
#ifdef UNICODE
        fn = DYN_RESOLVE_A("user32.dll", "PeekMessageW", PFN);
#else
        fn = DYN_RESOLVE_A("user32.dll", "PeekMessageA", PFN);
#endif
    }
    return fn ? fn(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg) : FALSE;
}

__forceinline LRESULT SH_DefWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    using PFN = LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM);
    static volatile PFN fn = nullptr;
    if (!fn) {
#ifdef UNICODE
        fn = DYN_RESOLVE_A("user32.dll", "DefWindowProcW", PFN);
#else
        fn = DYN_RESOLVE_A("user32.dll", "DefWindowProcA", PFN);
#endif
    }
    return fn ? fn(hWnd, Msg, wParam, lParam) : 0;
}

__forceinline BOOL SH_GetClientRect(HWND hWnd, LPRECT lpRect) {
    using PFN = BOOL(WINAPI*)(HWND, LPRECT);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "GetClientRect", PFN);
    return fn ? fn(hWnd, lpRect) : FALSE;
}

__forceinline void SH_PostQuitMessage(int nExitCode) {
    using PFN = void(WINAPI*)(int);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "PostQuitMessage", PFN);
    if (fn) fn(nExitCode);
}

__forceinline BOOL SH_GetCursorPos(LPPOINT lpPoint) {
    using PFN = BOOL(WINAPI*)(LPPOINT);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "GetCursorPos", PFN);
    return fn ? fn(lpPoint) : FALSE;
}

__forceinline BOOL SH_ScreenToClient(HWND hWnd, LPPOINT lpPoint) {
    using PFN = BOOL(WINAPI*)(HWND, LPPOINT);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "ScreenToClient", PFN);
    return fn ? fn(hWnd, lpPoint) : FALSE;
}

__forceinline BOOL SH_ReleaseCapture() {
    using PFN = BOOL(WINAPI*)();
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "ReleaseCapture", PFN);
    return fn ? fn() : FALSE;
}

__forceinline LONG SH_GetWindowLong(HWND hWnd, int nIndex) {
    using PFN = LONG(WINAPI*)(HWND, int);
    static volatile PFN fn = nullptr;
    if (!fn) {
#ifdef UNICODE
        fn = DYN_RESOLVE_A("user32.dll", "GetWindowLongW", PFN);
#else
        fn = DYN_RESOLVE_A("user32.dll", "GetWindowLongA", PFN);
#endif
    }
    return fn ? fn(hWnd, nIndex) : 0;
}

__forceinline ULONG_PTR SH_SetClassLongPtr(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
    using PFN = ULONG_PTR(WINAPI*)(HWND, int, LONG_PTR);
    static volatile PFN fn = nullptr;
    if (!fn) {
#ifdef UNICODE
        fn = DYN_RESOLVE_A("user32.dll", "SetClassLongPtrW", PFN);
#else
        fn = DYN_RESOLVE_A("user32.dll", "SetClassLongPtrA", PFN);
#endif
    }
    return fn ? fn(hWnd, nIndex, dwNewLong) : 0;
}

__forceinline BOOL SH_IsZoomed(HWND hWnd) {
    using PFN = BOOL(WINAPI*)(HWND);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "IsZoomed", PFN);
    return fn ? fn(hWnd) : FALSE;
}

__forceinline int SH_GetSystemMetrics(int nIndex) {
    using PFN = int(WINAPI*)(int);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "GetSystemMetrics", PFN);
    return fn ? fn(nIndex) : 0;
}

__forceinline void SH_StripPEHeader() noexcept {
    (void)0;
}

__forceinline void SH_TaintSeed() noexcept {
    if (SH_PebBeingDebugged())       g_integritySeed ^= 0xAABBCCDD;
    if (SH_NtGlobalFlag())           g_integritySeed ^= 0x11223344;
    if (SH_HardwareBreakpoints())    g_integritySeed ^= 0x55667788;
    if (SH_DebugPort())              g_integritySeed ^= 0x99AABBCC;
    if (SH_CheckRemoteDebuggerPresent()) g_integritySeed ^= 0xDDEEFF00;
    if (SH_IsDebuggerPresent())      g_integritySeed ^= 0x12345678;
}

__forceinline BOOL SH_VerifySeed() noexcept {
    return g_integritySeed == 0x9E3779B9;
}

__forceinline int SH_DecoyLoop() noexcept {
    volatile int decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int junk = 0;
    for (int i = 0; i < decoy + 1; ++i) junk ^= i * 0x1337;
    return junk;
}

__forceinline BOOL SH_OpaqueTrue() noexcept {
    volatile int x = static_cast<int>(__rdtsc() & 0x7);
    return (x * x) >= 0;
}

__forceinline DWORD SH_CRC32(const void* data, size_t len) noexcept {
    static const DWORD table[256] = {
        0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
        0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
        0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
        0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
        0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
        0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
        0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
        0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
        0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
        0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
        0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
        0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
        0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
        0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
        0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
        0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
        0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
        0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
        0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
        0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
        0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
        0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
        0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
        0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36E04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
        0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
        0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
        0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
        0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
        0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
        0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
        0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
        0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
    };
    const BYTE* p = static_cast<const BYTE*>(data);
    DWORD crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc = (crc >> 8) ^ table[(crc ^ p[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

__forceinline void SH_Sleep(DWORD dwMilliseconds) {
    using PFN = void(WINAPI*)(DWORD);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("kernel32.dll", "Sleep", PFN);
    if (fn) fn(dwMilliseconds);
}

__forceinline HWND SH_CreateWindowW(LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
    int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu,
    HINSTANCE hInstance, LPVOID lpParam) {
    using PFN = HWND(WINAPI*)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "CreateWindowExW", PFN);
    return fn ? fn(0, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam) : NULL;
}

__forceinline ATOM SH_RegisterClassExW(const WNDCLASSEXW* lpwcx) {
    using PFN = ATOM(WINAPI*)(const WNDCLASSEXW*);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "RegisterClassExW", PFN);
    return fn ? fn(lpwcx) : 0;
}

__forceinline BOOL SH_DestroyWindow(HWND hWnd) {
    using PFN = BOOL(WINAPI*)(HWND);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "DestroyWindow", PFN);
    return fn ? fn(hWnd) : FALSE;
}

__forceinline LRESULT SH_SendMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    using PFN = LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM);
    static volatile PFN fn = nullptr;
    if (!fn) {
#ifdef UNICODE
        fn = DYN_RESOLVE_A("user32.dll", "SendMessageW", PFN);
#else
        fn = DYN_RESOLVE_A("user32.dll", "SendMessageA", PFN);
#endif
    }
    return fn ? fn(hWnd, Msg, wParam, lParam) : 0;
}

__forceinline BOOL SH_PostMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    using PFN = BOOL(WINAPI*)(HWND, UINT, WPARAM, LPARAM);
    static volatile PFN fn = nullptr;
    if (!fn) {
#ifdef UNICODE
        fn = DYN_RESOLVE_A("user32.dll", "PostMessageW", PFN);
#else
        fn = DYN_RESOLVE_A("user32.dll", "PostMessageA", PFN);
#endif
    }
    return fn ? fn(hWnd, Msg, wParam, lParam) : FALSE;
}

__forceinline HRESULT SH_DwmExtendFrameIntoClientArea(HWND hWnd, const MARGINS* pMarInset) {
    using PFN = HRESULT(WINAPI*)(HWND, const MARGINS*);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("dwmapi.dll", "DwmExtendFrameIntoClientArea", PFN);
    return fn ? fn(hWnd, pMarInset) : E_FAIL;
}

__forceinline BOOL SH_GetWindowRect(HWND hWnd, LPRECT lpRect) {
    using PFN = BOOL(WINAPI*)(HWND, LPRECT);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "GetWindowRect", PFN);
    return fn ? fn(hWnd, lpRect) : FALSE;
}

__forceinline BOOL SH_ClientToScreen(HWND hWnd, LPPOINT lpPoint) {
    using PFN = BOOL(WINAPI*)(HWND, LPPOINT);
    static volatile PFN fn = nullptr;
    if (!fn) fn = DYN_RESOLVE_A("user32.dll", "ClientToScreen", PFN);
    return fn ? fn(hWnd, lpPoint) : FALSE;
}

inline DWORD WINAPI SH_DecoyThread(LPVOID) {
    while (SH_OpaqueTrue()) {
        SH_TaintSeed();
        SH_Sleep(1000 + (GetTickCount() % 2000));
        SH_DecoyLoop();
    }
    return 0;
}

// Anti-tamper stub used at the top of GUI functions. Kept inline so it can be
// included from any translation unit that builds UI code.
__declspec(noinline) inline void SH_AD_GUI() noexcept {
    if (SH_NtGlobalFlag()) g_integritySeed ^= 0x11223344;
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
}