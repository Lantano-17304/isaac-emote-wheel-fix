#include "common.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

HMODULE g_module{};
BYTE* g_image{};
DWORD g_imageSize{};
BYTE* g_text{};
DWORD g_textSize{};
DWORD g_timestamp{};
std::wstring g_stateDirectory;
std::wstring g_logPath;

struct Analysis {
    bool imageValid{};
    size_t matches{};
    bool contextValid{};
    DWORD signatureRva{};
    DWORD selectorRva{};
    DWORD callTargetRva{};
    BYTE selectorValue{0xFF};
    std::string reason{"not analyzed"};
};

DWORD Rva(const void* pointer) {
    return static_cast<DWORD>(static_cast<const BYTE*>(pointer) - g_image);
}

bool IsTextPointer(const BYTE* pointer) {
    return pointer >= g_text && pointer < g_text + g_textSize;
}

void Log(const char* format, ...) {
    if (g_logPath.empty()) return;
    HANDLE file = CreateFileW(g_logPath.c_str(), FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME time{};
    GetLocalTime(&time);
    char message[2048]{};
    int prefix = _snprintf_s(message, _countof(message), _TRUNCATE,
        "[%02u:%02u:%02u.%03u] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
    va_list arguments;
    va_start(arguments, format);
    vsnprintf_s(message + prefix, _countof(message) - prefix, _TRUNCATE, format, arguments);
    va_end(arguments);
    strcat_s(message, "\r\n");
    DWORD written{};
    WriteFile(file, message, static_cast<DWORD>(strlen(message)), &written, nullptr);
    CloseHandle(file);
}

bool ParseImage() {
    g_image = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    if (!g_image) return false;
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(g_image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return false;
    g_imageSize = nt->OptionalHeader.SizeOfImage;
    g_timestamp = nt->FileHeader.TimeDateStamp;
    const auto section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (memcmp(section[i].Name, ".text", 5) == 0 &&
            (section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) {
            if (g_text) return false;
            g_text = g_image + section[i].VirtualAddress;
            g_textSize = section[i].Misc.VirtualSize;
        }
    }
    return g_text && g_textSize;
}

bool MatchMasked(const BYTE* candidate, const BYTE* signature, const char* mask, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (mask[i] == 'x' && candidate[i] != signature[i]) return false;
    }
    return true;
}

Analysis Analyze() {
    Analysis result{};
    result.imageValid = ParseImage();
    if (!result.imageValid) {
        result.reason = "unsupported PE image";
        return result;
    }
    // J460 EmoteWheel::Update context. Offset 40 is the immediate byte of
    // `push 0`, selecting the native left-stick group. The call at offset 45
    // is the game's deadzone-aware stick-vector reader.
    static constexpr BYTE signature[] = {
        0x53, 0x8D, 0x45, 0xE8, 0xC7, 0x87, 0x34, 0x08, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0x50, 0xB9, 0, 0, 0, 0, 0xE8, 0, 0, 0, 0,
        0xC7, 0x45, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x4D, 0xE8, 0x85,
        0xC9, 0x74, 0x0F, 0x6A, 0x00, 0x8D, 0x45, 0xE0, 0x50, 0xE8,
        0, 0, 0, 0, 0x8B, 0xF0
    };
    static constexpr char mask[] = "xxxxxxxxxxxxxxxx????x????xxxxxxxxxxxxxxxxxxxxx????xx";
    static_assert(sizeof(signature) == sizeof(mask) - 1, "signature mask mismatch");

    BYTE* match{};
    for (DWORD i = 0; i + sizeof(signature) <= g_textSize; ++i) {
        BYTE* candidate = g_text + i;
        if (MatchMasked(candidate, signature, mask, sizeof(signature))) {
            ++result.matches;
            match = candidate;
        }
    }
    if (result.matches != 1 || !match) {
        result.reason = result.matches ? "signature is not unique" : "signature not found";
        return result;
    }

    BYTE* selector = match + 40;
    BYTE* call = match + 45;
    if (selector[-1] != 0x6A || selector[0] != 0x00 || call[0] != 0xE8) {
        result.reason = "selector opcode validation failed";
        return result;
    }
    const int32_t displacement = *reinterpret_cast<const int32_t*>(call + 1);
    BYTE* callTarget = call + 5 + displacement;
    if (!IsTextPointer(match) || !IsTextPointer(selector) || !IsTextPointer(callTarget)) {
        result.reason = "validated target leaves executable text section";
        return result;
    }
    result.contextValid = true;
    result.signatureRva = Rva(match);
    result.selectorRva = Rva(selector);
    result.callTargetRva = Rva(callTarget);
    result.selectorValue = selector[0];
    result.reason = "validated";
    return result;
}

std::string JsonEscape(const std::string& value) {
    std::string output;
    for (char character : value) {
        if (character == '\\' || character == '"') output.push_back('\\');
        if (character == '\r') output += "\\r";
        else if (character == '\n') output += "\\n";
        else output.push_back(character);
    }
    return output;
}

void WriteDiagnostics(const std::wstring& hash, const std::wstring& mode,
    const Analysis& analysis, bool approved, bool patched, const char* status) {
    char buffer[4096]{};
    _snprintf_s(buffer, _countof(buffer), _TRUNCATE,
        "{\r\n"
        "  \"schema\": 1,\r\n"
        "  \"exe_sha256\": \"%s\",\r\n"
        "  \"pe32\": %s,\r\n"
        "  \"timestamp\": \"%08lX\",\r\n"
        "  \"mode\": \"%s\",\r\n"
        "  \"signature_matches\": %zu,\r\n"
        "  \"structural_validation\": %s,\r\n"
        "  \"compatible_available\": %s,\r\n"
        "  \"hash_approved\": %s,\r\n"
        "  \"patch_installed\": %s,\r\n"
        "  \"signature_rva\": \"%08lX\",\r\n"
        "  \"selector_rva\": \"%08lX\",\r\n"
        "  \"stick_reader_rva\": \"%08lX\",\r\n"
        "  \"reason\": \"%s\",\r\n"
        "  \"status\": \"%s\"\r\n"
        "}\r\n",
        ief::NarrowAscii(hash).c_str(), analysis.imageValid ? "true" : "false",
        g_timestamp, ief::NarrowAscii(mode).c_str(), analysis.matches,
        analysis.contextValid ? "true" : "false",
        analysis.contextValid ? "true" : "false", approved ? "true" : "false",
        patched ? "true" : "false", analysis.signatureRva, analysis.selectorRva,
        analysis.callTargetRva, JsonEscape(analysis.reason).c_str(), status);
    ief::WriteTextAtomic(ief::Join(g_stateDirectory, L"diagnostics.json"), buffer);
}

bool PatchSelector(const Analysis& analysis) {
    BYTE* selector = g_image + analysis.selectorRva;
    if (selector[0] != 0) return false;
    DWORD oldProtect{};
    if (!VirtualProtect(selector, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    selector[0] = 1;
    FlushInstructionCache(GetCurrentProcess(), selector, 1);
    DWORD ignored{};
    VirtualProtect(selector, 1, oldProtect, &ignored);
    return selector[0] == 1;
}

DWORD WINAPI Initialize(void*) {
    g_stateDirectory = ief::LocalStateDirectory();
    if (g_stateDirectory.empty()) return 1;
    g_logPath = ief::Join(g_stateDirectory, L"hook.log");
    Log("=== Isaac Emote Wheel Fix public hook ===");

    const std::wstring executable = ief::ModulePath(nullptr);
    std::wstring hash;
    if (executable.empty() || !ief::Sha256File(executable, hash)) {
        Log("fail-closed: cannot hash executable");
        Analysis empty{};
        empty.reason = "cannot hash executable";
        WriteDiagnostics(L"", L"diagnostic", empty, false, false, "disabled");
        return 2;
    }

    const std::wstring config = ief::Join(g_stateDirectory, L"config.ini");
    wchar_t modeBuffer[32]{};
    wchar_t approvedBuffer[96]{};
    GetPrivateProfileStringW(L"hook", L"mode", L"diagnostic", modeBuffer,
        static_cast<DWORD>(_countof(modeBuffer)), config.c_str());
    GetPrivateProfileStringW(L"hook", L"approved_exe_sha256", L"", approvedBuffer,
        static_cast<DWORD>(_countof(approvedBuffer)), config.c_str());
    const std::wstring mode(modeBuffer);
    const bool approved = _wcsicmp(mode.c_str(), L"compatible") == 0 &&
        approvedBuffer[0] && _wcsicmp(hash.c_str(), approvedBuffer) == 0;

    Analysis analysis = Analyze();
    Log("exe_sha256=%ls mode=%ls approved=%u matches=%zu context=%u reason=%s",
        hash.c_str(), mode.c_str(), approved ? 1u : 0u, analysis.matches,
        analysis.contextValid ? 1u : 0u, analysis.reason.c_str());

    bool patched = false;
    const char* status = "diagnostic-only";
    if (approved && analysis.contextValid) {
        patched = PatchSelector(analysis);
        status = patched ? "compatible-active" : "patch-failed";
        Log(patched
            ? "active patch installed: EmoteWheel stick group left(0) -> right(1)"
            : "fail-closed: selector patch failed");
    } else if (_wcsicmp(mode.c_str(), L"compatible") == 0 && !approved) {
        status = "approval-hash-mismatch";
        Log("fail-closed: compatible approval does not match current executable hash");
    } else if (!analysis.contextValid) {
        status = "unsupported";
        Log("fail-closed: structural validation failed");
    } else {
        Log("diagnostic mode: no code modified");
    }
    WriteDiagnostics(hash, mode, analysis, approved, patched, status);
    return patched || !approved ? 0 : 3;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
        HANDLE worker = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr);
        if (worker) CloseHandle(worker);
    }
    return TRUE;
}
