#include <windows.h>

#include <iostream>
#include <string>

using ProfileFn = BOOL (WINAPI*)(HANDLE, LPWSTR, LPDWORD);

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::wcerr << L"usage: proxy_smoke_test <full-path-to-userenv.dll>\n";
        return 2;
    }
    HMODULE proxy = LoadLibraryW(argv[1]);
    if (!proxy) {
        std::wcerr << L"LoadLibrary failed: " << GetLastError() << L"\n";
        return 3;
    }
    wchar_t loadedPath[32768]{};
    GetModuleFileNameW(proxy, loadedPath, static_cast<DWORD>(std::size(loadedPath)));
    std::wcout << L"loaded_module=" << loadedPath << L"\n";
    auto function = reinterpret_cast<ProfileFn>(GetProcAddress(proxy, "GetUserProfileDirectoryW"));
    if (!function) {
        std::wcerr << L"GetProcAddress failed: " << GetLastError() << L"\n";
        FreeLibrary(proxy);
        return 4;
    }
    DWORD length{};
    SetLastError(ERROR_SUCCESS);
    const BOOL first = function(GetCurrentProcessToken(), nullptr, &length);
    const DWORD firstError = GetLastError();
    std::wstring directory(length ? length : 1, L'\0');
    BOOL second = FALSE;
    if (length) second = function(GetCurrentProcessToken(), directory.data(), &length);
    std::wcout << L"probe_result=" << first << L" probe_error=" << firstError
               << L" final_result=" << second << L" profile=" << directory.c_str() << L"\n";
    Sleep(1000);
    FreeLibrary(proxy);
    return second ? 0 : 5;
}
