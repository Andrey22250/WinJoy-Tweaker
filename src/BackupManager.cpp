#include "BackupManager.h"
#include <algorithm>
#include <cwctype>

namespace BackupManager {

std::wstring EnsureBackupDir()
{
    wchar_t appData[MAX_PATH] = {};
    if (!GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH))
        return L"";

    std::wstring dir = std::wstring(appData) + L"\\WinJoyTweaker\\backups";

    // Создаём обе части пути; ошибки от уже существующих папок игнорируем.
    std::wstring parent = std::wstring(appData) + L"\\WinJoyTweaker";
    CreateDirectoryW(parent.c_str(), nullptr);
    CreateDirectoryW(dir.c_str(), nullptr);

    DWORD attr = GetFileAttributesW(dir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
        return L"";

    return dir;
}

// Ротация: оставляем не более MAX_BACKUPS .reg файлов в папке бэкапов.
// Сортируем по времени последней записи, удаляем самые старые.
static void RotateBackups(const std::wstring& dir)
{
    std::vector<std::pair<FILETIME, std::wstring>> files;

    std::wstring pattern = dir + L"\\*.reg";
    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            files.push_back({ fd.ftLastWriteTime, dir + L"\\" + fd.cFileName });
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    if (files.size() <= MAX_BACKUPS) return;

    std::sort(files.begin(), files.end(),
        [](const auto& a, const auto& b) {
            if (a.first.dwHighDateTime != b.first.dwHighDateTime)
                return a.first.dwHighDateTime < b.first.dwHighDateTime;
            return a.first.dwLowDateTime < b.first.dwLowDateTime;
        });

    size_t toDelete = files.size() - MAX_BACKUPS;
    for (size_t i = 0; i < toDelete; ++i)
        DeleteFileW(files[i].second.c_str());
}

std::wstring WriteBackup(const std::wstring& oemKey,
                         const std::wstring& oemName,
                         const std::vector<BYTE>& rawBytes,
                         const std::wstring& outDir)
{
    std::wstring dir = outDir.empty() ? EnsureBackupDir() : outDir;
    if (dir.empty()) return L"";

    // Временна́я метка: YYYYMMDD_HHMMSS.
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    wchar_t ts[20] = {};
    swprintf_s(ts, L"%04d%02d%02d_%02d%02d%02d",
               st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond);

    // Имя файла: '&' → '_', чтобы избежать проблем в проводнике.
    std::wstring safeKey = oemKey;
    for (wchar_t& ch : safeKey) if (ch == L'&') ch = L'_';
    std::wstring filePath = dir + L"\\" + safeKey + L"_" + ts + L".reg";

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return L"";

    auto writeW = [&](const std::wstring& s) {
        DWORD written = 0;
        WriteFile(hFile, s.c_str(),
                  static_cast<DWORD>(s.size() * sizeof(wchar_t)),
                  &written, nullptr);
    };

    // BOM (0xFEFF) — обязателен для формата «Version 5.00».
    const wchar_t bom = L'\xFEFF';
    DWORD written = 0;
    WriteFile(hFile, &bom, sizeof(wchar_t), &written, nullptr);

    writeW(L"Windows Registry Editor Version 5.00\r\n\r\n");

    std::wstring fullRegPath =
        L"[HKEY_CURRENT_USER\\System\\CurrentControlSet\\Control\\"
        L"MediaProperties\\PrivateProperties\\Joystick\\OEM\\" + oemKey + L"]\r\n";
    writeW(fullRegPath);

    // OEMName (REG_SZ): экранируем обратные слэши и кавычки.
    {
        std::wstring escaped;
        escaped.reserve(oemName.size());
        for (wchar_t c : oemName) {
            if (c == L'\\') escaped += L"\\\\";
            else if (c == L'"') escaped += L"\\\"";
            else escaped += c;
        }
        writeW(L"\"OEMName\"=\"" + escaped + L"\"\r\n");
    }

    // OEMData (REG_BINARY): hex:XX,XX,XX,...
    if (!rawBytes.empty()) {
        writeW(L"\"OEMData\"=hex:");
        for (size_t i = 0; i < rawBytes.size(); ++i) {
            wchar_t hex[4] = {};
            swprintf_s(hex, L"%02x", rawBytes[i]);
            writeW(hex);
            if (i + 1 < rawBytes.size()) writeW(L",");
        }
        writeW(L"\r\n");
    }

    writeW(L"\r\n");
    CloseHandle(hFile);

    RotateBackups(dir);
    return filePath;
}

BackupParseResult ParseBackupFile(const std::wstring& filePath)
{
    BackupParseResult r;

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        r.errorMessage = L"Не удалось открыть файл бэкапа.";
        return r;
    }

    LARGE_INTEGER sz = {};
    if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (64 * 1024)) {
        CloseHandle(hFile);
        r.errorMessage = L"Некорректный размер .reg-файла.";
        return r;
    }

    std::vector<BYTE> bytes(static_cast<size_t>(sz.QuadPart));
    DWORD read = 0;
    if (!ReadFile(hFile, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr)) {
        CloseHandle(hFile);
        r.errorMessage = L"Ошибка чтения файла.";
        return r;
    }
    CloseHandle(hFile);

    // Декодируем в std::wstring: либо UTF-16 LE (BOM FF FE), либо ANSI/ASCII.
    std::wstring text;
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        const wchar_t* w = reinterpret_cast<const wchar_t*>(bytes.data() + 2);
        size_t count = (bytes.size() - 2) / sizeof(wchar_t);
        text.assign(w, count);
    } else {
        text.reserve(bytes.size());
        for (BYTE b : bytes) text.push_back(static_cast<wchar_t>(b));
    }

    // --- OEMName: "OEMName"="..." с экранированием \\ и \" ---
    const std::wstring keyName = L"\"OEMName\"=\"";
    size_t kn = text.find(keyName);
    if (kn != std::wstring::npos) {
        size_t p = kn + keyName.size();
        std::wstring name;
        while (p < text.size()) {
            wchar_t c = text[p++];
            if (c == L'\\' && p < text.size()) {
                wchar_t e = text[p++];
                if      (e == L'\\') name += L'\\';
                else if (e == L'"')  name += L'"';
                else { name += L'\\'; name += e; }
            } else if (c == L'"') {
                break;
            } else {
                name += c;
            }
        }
        r.oemName = name;
    }

    // --- OEMData: "OEMData"=hex:XX,XX,... (с возможным переносом через '\') ---
    const std::wstring keyData = L"\"OEMData\"=hex:";
    size_t kd = text.find(keyData);
    if (kd != std::wstring::npos) {
        size_t p = kd + keyData.size();
        std::wstring blob;
        while (p < text.size()) {
            wchar_t c = text[p++];
            if (c == L'\\') {
                // Перенос строки: пропускаем до конца строки и ведущие пробелы.
                while (p < text.size() && text[p] != L'\n') ++p;
                if (p < text.size()) ++p;
                while (p < text.size() && (text[p] == L' ' || text[p] == L'\t')) ++p;
            } else if (c == L'\r' || c == L'\n') {
                break;
            } else if (c != L' ' && c != L'\t') {
                blob += c;
            }
        }

        auto hexVal = [](wchar_t c) -> int {
            if (c >= L'0' && c <= L'9') return c - L'0';
            if (c >= L'a' && c <= L'f') return c - L'a' + 10;
            if (c >= L'A' && c <= L'F') return c - L'A' + 10;
            return -1;
        };

        for (size_t i = 0; i + 1 < blob.size(); ) {
            while (i < blob.size() && !iswxdigit(blob[i])) ++i;
            if (i + 1 >= blob.size()) break;
            int v1 = hexVal(blob[i]);
            int v2 = hexVal(blob[i + 1]);
            if (v1 < 0 || v2 < 0) break;
            r.oemDataRaw.push_back(static_cast<BYTE>((v1 << 4) | v2));
            i += 2;
            if (i < blob.size() && blob[i] == L',') ++i;
        }
    }

    if (r.oemDataRaw.empty()) {
        r.errorMessage = L"В файле не найдены данные OEMData.";
        return r;
    }
    r.valid = true;
    return r;
}

} // namespace BackupManager
