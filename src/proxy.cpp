#include "common.hpp"

namespace {

using GetUserProfileDirectoryAFn = BOOL (WINAPI*)(HANDLE, LPSTR, LPDWORD);
using GetUserProfileDirectoryWFn = BOOL (WINAPI*)(HANDLE, LPWSTR, LPDWORD);

HMODULE g_module{};
INIT_ONCE g_systemOnce = INIT_ONCE_STATIC_INIT;
INIT_ONCE g_hookOnce = INIT_ONCE_STATIC_INIT;
HMODULE g_systemUserenv{};
GetUserProfileDirectoryAFn g_profileA{};
GetUserProfileDirectoryWFn g_profileW{};

void ProxyLog(const wchar_t* message) {
    const std::wstring state = ief::LocalStateDirectory();
    if (state.empty()) return;
    HANDLE file = CreateFileW(ief::Join(state, L"proxy.log").c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written{};
    WriteFile(file, message, static_cast<DWORD>(wcslen(message) * sizeof(wchar_t)), &written, nullptr);
    static constexpr wchar_t newline[] = L"\r\n";
    WriteFile(file, newline, static_cast<DWORD>((std::size(newline) - 1) * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(file);
}

BOOL CALLBACK ResolveSystemUserenv(PINIT_ONCE, PVOID, PVOID*) {
    wchar_t systemDirectory[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH);
    if (!length || length >= MAX_PATH) return TRUE;
    const std::wstring path = ief::Join(systemDirectory, L"userenv.dll");
    g_systemUserenv = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!g_systemUserenv) return TRUE;
    g_profileA = reinterpret_cast<GetUserProfileDirectoryAFn>(
        GetProcAddress(g_systemUserenv, "GetUserProfileDirectoryA"));
    g_profileW = reinterpret_cast<GetUserProfileDirectoryWFn>(
        GetProcAddress(g_systemUserenv, "GetUserProfileDirectoryW"));
    return TRUE;
}

void EnsureSystemUserenv() {
    InitOnceExecuteOnce(&g_systemOnce, ResolveSystemUserenv, nullptr, nullptr);
}

BOOL CALLBACK LoadHook(PINIT_ONCE, PVOID, PVOID*) {
    EnsureSystemUserenv();
    if (!g_systemUserenv || !g_profileA || !g_profileW) {
        ProxyLog(L"system userenv forwarding validation failed; hook not loaded");
        return TRUE;
    }
    const std::wstring proxyPath = ief::ModulePath(g_module);
    if (proxyPath.empty()) return TRUE;
    const std::wstring hookPath = ief::Join(ief::DirectoryOf(proxyPath), L"emote_input_hook.dll");
    if (!ief::FileExists(hookPath)) {
        ProxyLog(L"sibling emote_input_hook.dll not found; system forwarding remains active");
        return TRUE;
    }
    if (!LoadLibraryW(hookPath.c_str())) {
        ProxyLog(L"emote_input_hook.dll failed to load; system forwarding remains active");
        return TRUE;
    }
    ProxyLog(L"system userenv forwarded and emote_input_hook.dll loaded");
    return TRUE;
}

void EnsureHookLoaded() {
    // The game dynamically loads userenv.dll, calls the exported function and
    // quickly releases the module. Loading the hook synchronously while the
    // export is still executing prevents a worker thread from returning into
    // an already-unloaded proxy image.
    InitOnceExecuteOnce(&g_hookOnce, LoadHook, nullptr, nullptr);
}

} // namespace

extern "C" BOOL WINAPI GetUserProfileDirectoryA(HANDLE token, LPSTR directory, LPDWORD size) {
    EnsureSystemUserenv();
    if (!g_profileA) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return FALSE;
    }
    EnsureHookLoaded();
    return g_profileA(token, directory, size);
}

extern "C" BOOL WINAPI GetUserProfileDirectoryW(HANDLE token, LPWSTR directory, LPDWORD size) {
    EnsureSystemUserenv();
    if (!g_profileW) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return FALSE;
    }
    EnsureHookLoaded();
    return g_profileW(token, directory, size);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
