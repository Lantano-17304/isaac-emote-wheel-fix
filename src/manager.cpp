#include "common.hpp"

#include <commctrl.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <fstream>
#include <sstream>

namespace {

constexpr wchar_t kWindowClass[] = L"IsaacEmoteFixManagerWindow";
constexpr int kStatus = 100;
constexpr int kRefresh = 101;
constexpr int kInstall = 102;
constexpr int kDisable = 104;
constexpr int kUninstall = 105;
constexpr int kOpenLogs = 106;
constexpr wchar_t kVerifiedJ460Hash[] =
    L"3BDFC8BAE0DC7E334B76009D0AD45DFBB16EE5F00C06FFBC3A0094E34D44616B";

HINSTANCE g_instance{};
HWND g_status{};
std::wstring g_managerDirectory;
std::wstring g_gameDirectory;
std::wstring g_stateDirectory;

std::wstring ReadIni(const std::wstring& file, const wchar_t* section,
    const wchar_t* key, const wchar_t* fallback = L"") {
    wchar_t buffer[32768]{};
    GetPrivateProfileStringW(section, key, fallback, buffer,
        static_cast<DWORD>(_countof(buffer)), file.c_str());
    return buffer;
}

bool WriteIni(const std::wstring& file, const wchar_t* section,
    const wchar_t* key, const std::wstring& value) {
    return WritePrivateProfileStringW(section, key, value.c_str(), file.c_str()) != FALSE;
}

std::wstring ReadText(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return L"";
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        return L"";
    }
    std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read{};
    const bool ok = bytes.empty() ||
        (ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) && read == bytes.size());
    CloseHandle(file);
    if (!ok) return L"";
    const int required = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    std::wstring output(static_cast<size_t>(required), L'\0');
    if (required) MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), output.data(), required);
    return output;
}

bool GameRunning() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"isaac-ng.exe") == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

std::wstring DetectGameDirectory() {
    const std::wstring parent = ief::DirectoryOf(g_managerDirectory);
    if (ief::FileExists(ief::Join(parent, L"isaac-ng.exe"))) return parent;
    if (ief::FileExists(ief::Join(g_managerDirectory, L"isaac-ng.exe"))) return g_managerDirectory;
    const std::wstring managerIni = ief::Join(g_stateDirectory, L"manager.ini");
    const std::wstring saved = ReadIni(managerIni, L"manager", L"game_path");
    if (ief::FileExists(ief::Join(saved, L"isaac-ng.exe"))) return saved;
    return L"";
}

std::wstring HashOrStatus(const std::wstring& path) {
    if (!ief::FileExists(path)) return L"<不存在>";
    std::wstring hash;
    return ief::Sha256File(path, hash) ? hash : L"<读取失败>";
}

void SetStatus(const std::wstring& text) {
    SetWindowTextW(g_status, text.c_str());
}

std::wstring ConfigPath() {
    return ief::Join(g_stateDirectory, L"config.ini");
}

void RefreshStatus() {
    std::wstringstream output;
    output << L"Isaac Emote Wheel Fix — Pre-release manager\r\n\r\n";
    if (g_gameDirectory.empty()) {
        output << L"未找到游戏。请把整个 IsaacEmoteWheelFix 文件夹放在游戏目录中。\r\n";
        SetStatus(output.str());
        return;
    }
    const std::wstring exe = ief::Join(g_gameDirectory, L"isaac-ng.exe");
    const std::wstring proxy = ief::Join(g_gameDirectory, L"userenv.dll");
    const std::wstring hook = ief::Join(g_gameDirectory, L"emote_input_hook.dll");
    std::wstring exeHash;
    const bool hashOk = ief::Sha256File(exe, exeHash);
    output << L"游戏目录：" << g_gameDirectory << L"\r\n";
    output << L"游戏进程：" << (GameRunning() ? L"运行中（所有变更已锁定）" : L"未运行") << L"\r\n";
    output << L"EXE：" << (ief::IsPe32Executable(exe) ? L"PE32/x86" : L"不支持") << L"\r\n";
    output << L"EXE SHA-256：" << (hashOk ? exeHash : L"<读取失败>") << L"\r\n\r\n";
    output << L"根目录 userenv.dll：" << HashOrStatus(proxy) << L"\r\n";
    output << L"根目录 Hook：" << HashOrStatus(hook) << L"\r\n\r\n";
    const std::wstring mode = ReadIni(ConfigPath(), L"hook", L"mode", L"diagnostic");
    const std::wstring approved = ReadIni(ConfigPath(), L"hook", L"approved_exe_sha256");
    output << L"配置模式：" << mode << L"\r\n";
    output << L"授权哈希：" << (approved.empty() ? L"<无>" : approved) << L"\r\n\r\n";
    const std::wstring diagnostics = ReadText(ief::Join(g_stateDirectory, L"diagnostics.json"));
    if (diagnostics.empty()) {
        output << L"尚无运行时诊断。安装后通过 Steam 启动一次游戏，再返回此处刷新。\r\n";
    } else {
        output << L"最近一次诊断：\r\n" << diagnostics;
    }
    SetStatus(output.str());
}

bool CopyAtomic(const std::wstring& source, const std::wstring& target) {
    const std::wstring temporary = target + L".ief-new";
    DeleteFileW(temporary.c_str());
    if (!CopyFileW(source.c_str(), temporary.c_str(), TRUE)) return false;
    if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

bool OwnedOrAbsent(const std::wstring& target, const std::wstring& manifest,
    const wchar_t* key, const std::wstring& payloadHash) {
    if (!ief::FileExists(target)) return true;
    std::wstring current;
    if (!ief::Sha256File(target, current)) return false;
    if (_wcsicmp(current.c_str(), payloadHash.c_str()) == 0) return true;
    const std::wstring recorded = ReadIni(manifest, L"install", key);
    return !recorded.empty() && _wcsicmp(current.c_str(), recorded.c_str()) == 0;
}

int ElevatedInstall(const std::wstring& gameDirectory) {
    if (GameRunning()) return 10;
    const std::wstring exe = ief::Join(gameDirectory, L"isaac-ng.exe");
    if (!ief::IsPe32Executable(exe)) return 11;
    const std::wstring payloadProxy = ief::Join(g_managerDirectory, L"payload\\userenv.proxy.dll");
    const std::wstring payloadHook = ief::Join(g_managerDirectory, L"payload\\emote_input_hook.dll");
    if (!ief::FileExists(payloadProxy) || !ief::FileExists(payloadHook)) return 14;
    std::wstring proxyHash;
    std::wstring hookHash;
    if (!ief::Sha256File(payloadProxy, proxyHash) || !ief::Sha256File(payloadHook, hookHash)) return 14;
    const std::wstring manifest = ief::Join(g_stateDirectory, L"install-manifest.ini");
    const std::wstring targetProxy = ief::Join(gameDirectory, L"userenv.dll");
    const std::wstring targetHook = ief::Join(gameDirectory, L"emote_input_hook.dll");
    if (!OwnedOrAbsent(targetProxy, manifest, L"userenv_sha256", proxyHash)) return 12;
    if (!OwnedOrAbsent(targetHook, manifest, L"hook_sha256", hookHash)) return 13;
    if (!CopyAtomic(payloadHook, targetHook) || !CopyAtomic(payloadProxy, targetProxy)) return 15;
    WriteIni(manifest, L"install", L"game_path", gameDirectory);
    WriteIni(manifest, L"install", L"userenv_sha256", proxyHash);
    WriteIni(manifest, L"install", L"hook_sha256", hookHash);
    return 0;
}

int ElevatedUninstall(const std::wstring& gameDirectory) {
    if (GameRunning()) return 10;
    const std::wstring manifest = ief::Join(g_stateDirectory, L"install-manifest.ini");
    const std::wstring targetProxy = ief::Join(gameDirectory, L"userenv.dll");
    const std::wstring targetHook = ief::Join(gameDirectory, L"emote_input_hook.dll");
    const std::wstring recordedProxy = ReadIni(manifest, L"install", L"userenv_sha256");
    const std::wstring recordedHook = ReadIni(manifest, L"install", L"hook_sha256");
    for (const auto& item : {std::pair<std::wstring, std::wstring>{targetProxy, recordedProxy},
            std::pair<std::wstring, std::wstring>{targetHook, recordedHook}}) {
        if (!ief::FileExists(item.first)) continue;
        std::wstring current;
        if (item.second.empty() || !ief::Sha256File(item.first, current) ||
            _wcsicmp(current.c_str(), item.second.c_str()) != 0) return 20;
    }
    if (ief::FileExists(targetProxy) && !DeleteFileW(targetProxy.c_str())) return 21;
    if (ief::FileExists(targetHook) && !DeleteFileW(targetHook.c_str())) return 22;
    DeleteFileW(manifest.c_str());
    return 0;
}

std::wstring ResultMessage(DWORD code) {
    switch (code) {
    case 0: return L"操作成功。";
    case 10: return L"游戏仍在运行。请正常退出后重试。";
    case 11: return L"目标不是受支持的 PE32 isaac-ng.exe。";
    case 12: return L"发现不属于本项目的 userenv.dll，已拒绝覆盖。";
    case 13: return L"发现不属于本项目的 emote_input_hook.dll，已拒绝覆盖。";
    case 14: return L"发布包 payload 文件缺失或无法读取。";
    case 15: return L"复制文件失败。请检查权限和安全软件。";
    case 20: return L"已安装文件哈希与清单不同，已拒绝卸载。";
    case 21: return L"无法删除项目代理 DLL。";
    case 22: return L"无法删除项目 Hook DLL。";
    default: return L"操作失败，退出代码：" + std::to_wstring(code);
    }
}

DWORD RunElevated(const wchar_t* action) {
    std::wstring parameters = std::wstring(action) + L" \"" + g_gameDirectory + L"\"";
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.lpVerb = L"runas";
    const std::wstring executable = ief::ModulePath();
    info.lpFile = executable.c_str();
    info.lpParameters = parameters.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info)) return GetLastError();
    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD code{};
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    return code;
}

bool DiagnosticsApproveCurrentHash(std::wstring& hash) {
    const std::wstring exe = ief::Join(g_gameDirectory, L"isaac-ng.exe");
    if (!ief::Sha256File(exe, hash)) return false;
    const std::wstring diagnostics = ReadText(ief::Join(g_stateDirectory, L"diagnostics.json"));
    if (diagnostics.empty()) return false;
    const std::wstring hashField = L"\"exe_sha256\": \"" + hash + L"\"";
    return diagnostics.find(hashField) != std::wstring::npos &&
        diagnostics.find(L"\"structural_validation\": true") != std::wstring::npos &&
        diagnostics.find(L"\"compatible_available\": true") != std::wstring::npos;
}

void RequireStopped(HWND window, const wchar_t* operation) {
    std::wstring message = std::wstring(operation) + L"要求游戏完全退出。";
    MessageBoxW(window, message.c_str(), L"Isaac Emote Wheel Fix", MB_OK | MB_ICONWARNING);
}

void HandleCommand(HWND window, int id) {
    if (id == kRefresh) {
        RefreshStatus();
        return;
    }
    if (id == kOpenLogs) {
        ShellExecuteW(window, L"open", g_stateDirectory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    if (g_gameDirectory.empty()) {
        MessageBoxW(window, L"未找到游戏目录。", L"Isaac Emote Wheel Fix", MB_OK | MB_ICONERROR);
        return;
    }
    if (GameRunning()) {
        RequireStopped(window, L"此操作");
        return;
    }
    if (id == kInstall) {
        const DWORD code = RunElevated(L"--elevated-install");
        if (code == 0) {
            WriteIni(ief::Join(g_stateDirectory, L"manager.ini"), L"manager", L"game_path", g_gameDirectory);
            std::wstring currentHash;
            const bool hashRead = ief::Sha256File(
                ief::Join(g_gameDirectory, L"isaac-ng.exe"), currentHash);
            std::wstring diagnosedHash;
            const bool verified = hashRead &&
                _wcsicmp(currentHash.c_str(), kVerifiedJ460Hash) == 0;
            const bool diagnosed = DiagnosticsApproveCurrentHash(diagnosedHash);
            if (verified || diagnosed) {
                const std::wstring approvedHash = verified ? currentHash : diagnosedHash;
                WriteIni(ConfigPath(), L"hook", L"approved_exe_sha256", approvedHash);
                WriteIni(ConfigPath(), L"hook", L"mode", L"compatible");
                MessageBoxW(window,
                    L"修复已安装并为当前游戏版本启用。下次启动游戏时生效；游戏更新后授权会自动失效。",
                    L"安装并启用修复", MB_OK | MB_ICONINFORMATION);
            } else {
                WriteIni(ConfigPath(), L"hook", L"mode", L"diagnostic");
                WriteIni(ConfigPath(), L"hook", L"approved_exe_sha256", L"");
                MessageBoxW(window,
                    L"已安装安全诊断。当前版本尚未验证：请启动并正常退出一次游戏，然后再次点击同一个“安装并启用修复”按钮。",
                    L"需要一次诊断", MB_OK | MB_ICONINFORMATION);
            }
        } else {
            const std::wstring message = ResultMessage(code);
            MessageBoxW(window, message.c_str(), L"安装并启用修复",
                MB_OK | MB_ICONERROR);
        }
    } else if (id == kDisable) {
        WriteIni(ConfigPath(), L"hook", L"mode", L"diagnostic");
        WriteIni(ConfigPath(), L"hook", L"approved_exe_sha256", L"");
        MessageBoxW(window, L"修复已暂停。文件仍保留，但下次启动只进行安全诊断，不修改轮盘输入。",
            L"暂停修复", MB_OK | MB_ICONINFORMATION);
    } else if (id == kUninstall) {
        const DWORD code = RunElevated(L"--elevated-uninstall");
        const std::wstring message = ResultMessage(code);
        MessageBoxW(window, message.c_str(), L"卸载", MB_OK | (code ? MB_ICONERROR : MB_ICONINFORMATION));
    }
    RefreshStatus();
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        CreateWindowW(L"STATIC", L"Steam 原版 J460 表情轮右摇杆修复（Pre-release）",
            WS_CHILD | WS_VISIBLE, 16, 14, 740, 24, window, nullptr, g_instance, nullptr);
        g_status = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
                ES_AUTOVSCROLL | ES_READONLY,
            16, 44, 752, 440, window, reinterpret_cast<HMENU>(kStatus), g_instance, nullptr);
        const struct Button { int id; const wchar_t* text; int x; int width; } buttons[] = {
            {kRefresh, L"刷新", 16, 80}, {kInstall, L"安装并启用修复", 104, 176},
            {kDisable, L"暂停修复", 288, 104}, {kUninstall, L"卸载", 400, 80},
            {kOpenLogs, L"打开日志目录", 488, 140}
        };
        for (const auto& button : buttons) {
            CreateWindowW(L"BUTTON", button.text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                button.x, 500, button.width, 32, window, reinterpret_cast<HMENU>(button.id),
                g_instance, nullptr);
        }
        RefreshStatus();
        return 0;
    }
    case WM_COMMAND:
        HandleCommand(window, LOWORD(wParam));
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_instance = instance;
    g_managerDirectory = ief::DirectoryOf(ief::ModulePath());
    g_stateDirectory = ief::LocalStateDirectory();
    if (g_managerDirectory.empty() || g_stateDirectory.empty()) return 2;

    int argumentCount{};
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments && argumentCount == 3) {
        const std::wstring action = arguments[1];
        const std::wstring game = arguments[2];
        LocalFree(arguments);
        if (action == L"--elevated-install") return ElevatedInstall(game);
        if (action == L"--elevated-uninstall") return ElevatedUninstall(game);
        return 3;
    }
    if (arguments) LocalFree(arguments);

    g_gameDirectory = DetectGameDirectory();
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.lpszClassName = kWindowClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassExW(&windowClass)) return 4;
    HWND window = CreateWindowExW(0, kWindowClass, L"Isaac Emote Wheel Fix Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 580, nullptr, nullptr, instance, nullptr);
    if (!window) return 5;
    ShowWindow(window, show);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
