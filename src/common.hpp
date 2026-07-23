#pragma once

#include <windows.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <array>
#include <string>
#include <vector>

namespace ief {

inline std::wstring Join(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) return right;
    if (left.back() == L'\\' || left.back() == L'/') return left + right;
    return left + L"\\" + right;
}

inline std::wstring DirectoryOf(const std::wstring& path) {
    const size_t position = path.find_last_of(L"\\/");
    return position == std::wstring::npos ? L"" : path.substr(0, position);
}

inline bool FileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

inline std::wstring ModulePath(HMODULE module = nullptr) {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length >= buffer.size()) return L"";
    return std::wstring(buffer.data(), length);
}

inline std::wstring LocalStateDirectory() {
    wchar_t path[32768]{};
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, static_cast<DWORD>(_countof(path)));
    if (!length || length >= _countof(path)) {
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
                nullptr, SHGFP_TYPE_CURRENT, path))) {
            return L"";
        }
    }
    std::wstring result = Join(path, L"IsaacEmoteWheelFix");
    if (!CreateDirectoryW(result.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        return L"";
    }
    return result;
}

inline std::wstring Hex(const BYTE* bytes, size_t length) {
    static constexpr wchar_t digits[] = L"0123456789ABCDEF";
    std::wstring output(length * 2, L'0');
    for (size_t i = 0; i < length; ++i) {
        output[i * 2] = digits[bytes[i] >> 4];
        output[i * 2 + 1] = digits[bytes[i] & 0x0F];
    }
    return output;
}

inline bool Sha256File(const std::wstring& path, std::wstring& output) {
    output.clear();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    DWORD objectLength{};
    DWORD resultLength{};
    std::vector<BYTE> object;
    std::array<BYTE, 32> digest{};
    bool success = false;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &resultLength, 0) >= 0) {
        object.resize(objectLength);
        if (BCryptCreateHash(algorithm, &hash, object.data(), objectLength, nullptr, 0, 0) >= 0) {
            std::array<BYTE, 64 * 1024> buffer{};
            DWORD read{};
            success = true;
            while (true) {
                if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
                    success = false;
                    break;
                }
                if (!read) break;
                if (BCryptHashData(hash, buffer.data(), read, 0) < 0) {
                    success = false;
                    break;
                }
            }
            if (success && BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0) {
                output = Hex(digest.data(), digest.size());
            } else {
                success = false;
            }
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    return success && !output.empty();
}

inline bool IsPe32Executable(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    IMAGE_DOS_HEADER dos{};
    DWORD read{};
    bool valid = ReadFile(file, &dos, sizeof(dos), &read, nullptr) && read == sizeof(dos) &&
        dos.e_magic == IMAGE_DOS_SIGNATURE;
    IMAGE_NT_HEADERS32 nt{};
    if (valid) {
        LARGE_INTEGER offset{};
        offset.QuadPart = dos.e_lfanew;
        valid = SetFilePointerEx(file, offset, nullptr, FILE_BEGIN) &&
            ReadFile(file, &nt, sizeof(nt), &read, nullptr) && read == sizeof(nt) &&
            nt.Signature == IMAGE_NT_SIGNATURE &&
            nt.FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
            nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    }
    CloseHandle(file);
    return valid;
}

inline bool WriteTextAtomic(const std::wstring& path, const std::string& text) {
    const std::wstring temporary = path + L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written{};
    const bool success = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
        written == text.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!success) {
        DeleteFileW(temporary.c_str());
        return false;
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

inline std::string NarrowAscii(const std::wstring& value) {
    std::string result;
    result.reserve(value.size());
    for (wchar_t character : value) result.push_back(character <= 0x7F ? static_cast<char>(character) : '?');
    return result;
}

} // namespace ief
