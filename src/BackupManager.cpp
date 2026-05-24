#include "BackupManager.h"
#include <algorithm>
#include <cwctype>
#include <cstdlib>

namespace {
    // Корневой путь к ветке джойстиков. Дублирует константу из RegistryEngine,
    // чтобы не вытаскивать её в публичный заголовок.
    constexpr const wchar_t* JOYSTICK_REG_PATH =
        L"System\\CurrentControlSet\\Control\\MediaProperties"
        L"\\PrivateProperties\\Joystick\\OEM";

    constexpr const wchar_t* JOYSTICK_REG_HKCU_PREFIX =
        L"HKEY_CURRENT_USER\\System\\CurrentControlSet\\Control\\MediaProperties"
        L"\\PrivateProperties\\Joystick\\OEM";
}

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

// Проверяет, что имя файла — бэкап ИМЕННО этого устройства: префикс
// "<safeKey>_" + хвост ровно "YYYYMMDD_HHMMSS.reg" (8 цифр, '_', 6 цифр, .reg).
// Строгая проверка хвоста нужна, чтобы устройство без суффикса
// (VID_xxxx_PID_yyyy) не «цепляло» файлы суффиксного варианта
// (VID_xxxx_PID_yyyy_IG_00), у которого имя начинается с того же префикса.
static bool IsDeviceBackupName(const std::wstring& fileName,
                               const std::wstring& safeKey)
{
    const std::wstring prefix = safeKey + L"_";
    const size_t tailLen = 19;  // "YYYYMMDD_HHMMSS.reg"
    if (fileName.size() != prefix.size() + tailLen) return false;
    if (fileName.compare(0, prefix.size(), prefix) != 0) return false;

    const std::wstring tail = fileName.substr(prefix.size());
    for (int i = 0; i < 8; ++i)  if (!iswdigit(tail[i]))  return false;
    if (tail[8] != L'_') return false;
    for (int i = 9; i < 15; ++i) if (!iswdigit(tail[i]))  return false;
    return tail.compare(15, 4, L".reg") == 0;
}

// Ротация: оставляем не более MAX_BACKUPS .reg файлов ДЛЯ КОНКРЕТНОГО
// устройства (по префиксу safeKey). Бэкапы других устройств не трогаем —
// иначе новый бэкап одного устройства мог бы вытеснить бэкапы другого.
// Сортируем по времени последней записи, удаляем самые старые.
static void RotateBackups(const std::wstring& dir, const std::wstring& safeKey)
{
    std::vector<std::pair<FILETIME, std::wstring>> files;

    std::wstring pattern = dir + L"\\*.reg";
    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            && IsDeviceBackupName(fd.cFileName, safeKey))
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

// =====================================================================
// Хелперы записи .reg-файла
// =====================================================================

// Экранирование строкового значения REG_SZ (\ и " → \\ и \").
static std::wstring EscapeRegString(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size() + 4);
    for (wchar_t c : s) {
        if      (c == L'\\') out += L"\\\\";
        else if (c == L'"')  out += L"\\\"";
        else out += c;
    }
    return out;
}

// Обёртка над WriteFile, отслеживающая ошибки записи. Любой сбой WriteFile
// или неполная запись (например, переполнение диска C:) переводит ok в false,
// после чего все последующие write() — no-op. По завершении WriteBackup
// проверяет ok: если запись не удалась, неполный .reg удаляется, а функция
// возвращает "" — вызывающий код не станет менять реестр без валидного бэкапа.
struct RegFileWriter {
    HANDLE h;
    bool   ok = true;

    void write(const std::wstring& s) {
        if (!ok) return;
        const DWORD want = static_cast<DWORD>(s.size() * sizeof(wchar_t));
        DWORD got = 0;
        if (!WriteFile(h, s.c_str(), want, &got, nullptr) || got != want)
            ok = false;
    }

    void writeRaw(const void* data, DWORD bytes) {
        if (!ok) return;
        DWORD got = 0;
        if (!WriteFile(h, data, bytes, &got, nullptr) || got != bytes)
            ok = false;
    }
};

// Сериализация бинарного блока в формате hex:XX,XX,...,XX. Перенос длинных
// строк через '\' для совместимости с regedit (>1024 символов в строке
// regedit не любит; делим по 16 байт).
static void WriteHexBlob(RegFileWriter& w, const BYTE* data, DWORD size)
{
    auto writeW = [&](const std::wstring& s) { w.write(s); };

    for (DWORD i = 0; i < size; ++i) {
        wchar_t hex[4] = {};
        swprintf_s(hex, L"%02x", data[i]);
        writeW(hex);
        if (i + 1 < size) {
            // Перенос каждые 25 байт: hex:NN,NN,...\<CRLF>  NN,NN,...
            if ((i + 1) % 25 == 0) writeW(L",\\\r\n  ");
            else                   writeW(L",");
        }
    }
}

// Дамп одного ключа: заголовок [полный\путь] + все значения. Подключи
// обходятся снаружи рекурсивно.
static void DumpKeyValues(RegFileWriter& fw, HKEY hKey,
                          const std::wstring& fullDisplayPath)
{
    auto writeW = [&](const std::wstring& s) { fw.write(s); };

    writeW(L"[" + fullDisplayPath + L"]\r\n");

    DWORD index = 0;
    while (true) {
        WCHAR valueName[512];
        DWORD nameLen = 512;
        DWORD type = 0;
        DWORD dataSize = 0;
        LSTATUS s = RegEnumValueW(hKey, index++, valueName, &nameLen,
                                  nullptr, &type, nullptr, &dataSize);
        if (s == ERROR_NO_MORE_ITEMS) break;
        if (s != ERROR_SUCCESS && s != ERROR_MORE_DATA) continue;

        // Перечитываем сразу с правильным размером буфера.
        std::vector<BYTE> buffer(dataSize ? dataSize : 1);
        nameLen = 512;
        s = RegEnumValueW(hKey, index - 1, valueName, &nameLen,
                          nullptr, &type, buffer.data(), &dataSize);
        if (s != ERROR_SUCCESS) continue;

        // Имя значения: пустая строка → "@" (значение по умолчанию).
        std::wstring nameTok = (nameLen == 0)
            ? std::wstring(L"@")
            : (L"\"" + EscapeRegString(valueName) + L"\"");

        switch (type) {
        case REG_SZ:
        case REG_EXPAND_SZ: {
            // dataSize включает терминирующий нуль; отрезаем его.
            size_t chars = dataSize / sizeof(wchar_t);
            if (chars > 0 && reinterpret_cast<wchar_t*>(buffer.data())[chars - 1] == 0)
                --chars;
            std::wstring v(reinterpret_cast<wchar_t*>(buffer.data()), chars);
            writeW(nameTok + L"=\"" + EscapeRegString(v) + L"\"\r\n");
            break;
        }
        case REG_DWORD: {
            DWORD dw = 0;
            if (dataSize >= sizeof(DWORD))
                dw = *reinterpret_cast<DWORD*>(buffer.data());
            wchar_t hex[16] = {};
            swprintf_s(hex, L"%08x", dw);
            writeW(nameTok + L"=dword:" + hex + L"\r\n");
            break;
        }
        case REG_BINARY: {
            writeW(nameTok + L"=hex:");
            WriteHexBlob(fw, buffer.data(), dataSize);
            writeW(L"\r\n");
            break;
        }
        default: {
            // Прочие типы (MULTI_SZ, QWORD, ...) — hex(<type>):...
            wchar_t tp[8] = {};
            swprintf_s(tp, L"%x", type);
            writeW(nameTok + L"=hex(" + tp + L"):");
            WriteHexBlob(fw, buffer.data(), dataSize);
            writeW(L"\r\n");
            break;
        }
        }
    }
}

// Рекурсивный обход поддерева. После каждого ключа выводим пустую строку
// — таков обычный формат regedit-экспорта.
static void DumpSubtreeRec(RegFileWriter& fw, HKEY hKey,
                           const std::wstring& fullDisplayPath)
{
    DumpKeyValues(fw, hKey, fullDisplayPath);
    fw.write(L"\r\n");

    // Сначала собираем все имена подключей, потом обходим — на случай если
    // в процессе чтения индексы поедут (теоретически — не должны на HKCU).
    std::vector<std::wstring> subkeys;
    DWORD index = 0;
    while (true) {
        WCHAR sub[256];
        DWORD subLen = 256;
        LSTATUS s = RegEnumKeyExW(hKey, index++, sub, &subLen,
                                  nullptr, nullptr, nullptr, nullptr);
        if (s == ERROR_NO_MORE_ITEMS) break;
        if (s != ERROR_SUCCESS) continue;
        subkeys.emplace_back(sub);
    }

    for (const auto& sub : subkeys) {
        HKEY hChild = nullptr;
        if (RegOpenKeyExW(hKey, sub.c_str(), 0, KEY_READ, &hChild) != ERROR_SUCCESS)
            continue;
        DumpSubtreeRec(fw, hChild, fullDisplayPath + L"\\" + sub);
        RegCloseKey(hChild);
    }
}

// =====================================================================
// Основной API
// =====================================================================

std::wstring WriteBackup(const std::wstring& oemKey,
                         const std::wstring& outDir)
{
    std::wstring dir = outDir.empty() ? EnsureBackupDir() : outDir;
    if (dir.empty()) return L"";

    std::wstring deviceRegPath = std::wstring(JOYSTICK_REG_PATH) + L"\\" + oemKey;
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, deviceRegPath.c_str(),
                      0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return L"";

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
    if (hFile == INVALID_HANDLE_VALUE) {
        RegCloseKey(hKey);
        return L"";
    }

    RegFileWriter fw{ hFile };

    // BOM (0xFEFF) — обязателен для формата «Version 5.00».
    const wchar_t bom = L'\xFEFF';
    fw.writeRaw(&bom, sizeof(wchar_t));
    fw.write(L"Windows Registry Editor Version 5.00\r\n\r\n");

    std::wstring rootDisplay = std::wstring(JOYSTICK_REG_HKCU_PREFIX) + L"\\" + oemKey;
    DumpSubtreeRec(fw, hKey, rootDisplay);

    // Финальный flush на диск — чтобы переполнение/ошибка проявились здесь,
    // а не позже. Если что-то не записалось (например, кончилось место на C:),
    // удаляем неполный файл и сигнализируем провал пустой строкой.
    if (fw.ok && !FlushFileBuffers(hFile))
        fw.ok = false;

    CloseHandle(hFile);
    RegCloseKey(hKey);

    if (!fw.ok) {
        DeleteFileW(filePath.c_str());
        return L"";
    }

    RotateBackups(dir, safeKey);
    return filePath;
}

// =====================================================================
// Разбор .reg-файла
// =====================================================================

// Парсит REG_SZ-значение начиная с позиции после открывающей кавычки.
// Возвращает строку до закрывающей " и сдвигает p за неё.
static std::wstring ParseRegString(const std::wstring& text, size_t& p)
{
    std::wstring out;
    while (p < text.size()) {
        wchar_t c = text[p++];
        if (c == L'\\' && p < text.size()) {
            wchar_t e = text[p++];
            if      (e == L'\\') out += L'\\';
            else if (e == L'"')  out += L'"';
            else { out += L'\\'; out += e; }
        } else if (c == L'"') {
            return out;
        } else {
            out += c;
        }
    }
    return out;
}

// Парсит hex:XX,XX,... возможно с переносом через '\' в конце строки.
static std::vector<BYTE> ParseHexBlob(const std::wstring& text, size_t& p)
{
    std::wstring blob;
    while (p < text.size()) {
        wchar_t c = text[p++];
        if (c == L'\\') {
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

    std::vector<BYTE> bytes;
    for (size_t i = 0; i + 1 < blob.size(); ) {
        while (i < blob.size() && !iswxdigit(blob[i])) ++i;
        if (i + 1 >= blob.size()) break;
        int v1 = hexVal(blob[i]);
        int v2 = hexVal(blob[i + 1]);
        if (v1 < 0 || v2 < 0) break;
        bytes.push_back(static_cast<BYTE>((v1 << 4) | v2));
        i += 2;
        if (i < blob.size() && blob[i] == L',') ++i;
    }
    return bytes;
}

BackupParseResult ParseBackupFile(const std::wstring& filePath)
{
    BackupParseResult r;

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        r.errorCode = BackupError::OpenFileFailed;
        return r;
    }

    LARGE_INTEGER sz = {};
    // Лимит подняли до 1 МБ — поддерево с FF-эффектами заметно больше
    // одного OEMData, но реалистично не превышает сотен килобайт.
    if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (1024 * 1024)) {
        CloseHandle(hFile);
        r.errorCode = BackupError::InvalidSize;
        return r;
    }

    std::vector<BYTE> bytes(static_cast<size_t>(sz.QuadPart));
    DWORD read = 0;
    if (!ReadFile(hFile, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr)) {
        CloseHandle(hFile);
        r.errorCode = BackupError::ReadFailed;
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

    // Проходим по файлу построчно, помня текущий контекст:
    // currentScope = "root"   — мы внутри [...\<oemKey>]
    //                "axis_N" — внутри [...\<oemKey>\Axes\N]
    //                "btn_N"  — внутри [...\<oemKey>\Buttons\N]
    //                ""       — посторонний ключ, игнорируем.
    enum class Scope { Other, Root, Axis, Button };
    Scope scope = Scope::Other;
    int   scopeIndex = 0;

    size_t pos = 0;
    while (pos < text.size()) {
        // Пропуск пробелов и переводов строк.
        while (pos < text.size() &&
               (text[pos] == L'\r' || text[pos] == L'\n'))
            ++pos;
        if (pos >= text.size()) break;

        // Заголовок ключа.
        if (text[pos] == L'[') {
            ++pos;
            size_t end = text.find(L']', pos);
            if (end == std::wstring::npos) break;
            std::wstring header(text, pos, end - pos);
            pos = end + 1;

            // Определяем scope по хвосту заголовка после "\OEM\<oemKey>".
            // Ищем токены "\Axes\N", "\Buttons\N" в конце.
            scope = Scope::Other;
            auto endsWithIndex = [&](const wchar_t* mid, Scope sc) {
                std::wstring marker = std::wstring(L"\\") + mid + L"\\";
                size_t p = header.rfind(marker);
                if (p == std::wstring::npos) return false;
                std::wstring tail = header.substr(p + marker.size());
                wchar_t* e = nullptr;
                long v = wcstol(tail.c_str(), &e, 10);
                if (e == tail.c_str() || *e != L'\0') return false;
                scope = sc;
                scopeIndex = static_cast<int>(v);
                return true;
            };

            if (!endsWithIndex(L"Axes", Scope::Axis) &&
                !endsWithIndex(L"Buttons", Scope::Button))
            {
                // Это корневой ключ устройства, если в конце нет ещё одного '\'.
                size_t lastSlash = header.rfind(L'\\');
                if (lastSlash != std::wstring::npos) {
                    std::wstring tail = header.substr(lastSlash + 1);
                    // Эвристика: VID_xxxx&PID_xxxx — корневой ключ устройства.
                    if (tail.rfind(L"VID_", 0) == 0) {
                        scope = Scope::Root;
                        r.deviceKey = tail;
                    }
                }
            }
            continue;
        }

        // Строка значения. Читаем до конца строки.
        size_t lineEnd = text.find(L'\n', pos);
        if (lineEnd == std::wstring::npos) lineEnd = text.size();
        std::wstring line(text, pos, lineEnd - pos);
        if (!line.empty() && line.back() == L'\r') line.pop_back();

        // Достаём имя значения и сам токен значения.
        // Форматы: "Name"=type:data  или  @=type:data
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) {
            pos = lineEnd + 1;
            continue;
        }
        std::wstring lhs = line.substr(0, eq);

        // Какое именно значение перед нами?
        bool isDefault = (lhs == L"@");
        std::wstring valueName;
        if (!isDefault && lhs.size() >= 2 && lhs.front() == L'"' && lhs.back() == L'"')
            valueName = lhs.substr(1, lhs.size() - 2);

        // Точка чтения внутри text — сразу после '=' исходной позиции pos.
        size_t valPos = pos + eq + 1;

        // OEMName в корневом ключе.
        if (scope == Scope::Root && valueName == L"OEMName"
            && valPos < text.size() && text[valPos] == L'"')
        {
            ++valPos;
            r.oemName = ParseRegString(text, valPos);
        }
        // OEMData (REG_BINARY): валидный префикс — "hex:" или "hex(3):".
        else if (scope == Scope::Root && valueName == L"OEMData")
        {
            const std::wstring p1 = L"hex:";
            const std::wstring p2 = L"hex(3):";
            if (text.compare(valPos, p1.size(), p1) == 0) {
                valPos += p1.size();
                r.oemDataRaw = ParseHexBlob(text, valPos);
            } else if (text.compare(valPos, p2.size(), p2) == 0) {
                valPos += p2.size();
                r.oemDataRaw = ParseHexBlob(text, valPos);
            }
        }
        // @ имени оси/кнопки.
        else if (isDefault && (scope == Scope::Axis || scope == Scope::Button)
                 && valPos < text.size() && text[valPos] == L'"')
        {
            ++valPos;
            NamedEntry e;
            e.index = scopeIndex;
            e.name  = ParseRegString(text, valPos);
            if (scope == Scope::Axis) r.axes.push_back(std::move(e));
            else                      r.buttons.push_back(std::move(e));
        }

        pos = lineEnd + 1;
    }

    // Файл валиден, если найден заголовок корневого ключа устройства
    // ([...\OEM\VID_..&PID_..]) — даже без значений. Пустой бэкап (снятый с
    // устройства, у которого ещё не было OEMData/OEMName) нужен как валидная
    // цель отката: восстановление такого снимка УДАЛЯЕТ параметры, возвращая
    // устройство к заводскому виду. Если ключа устройства нет — это не наш
    // бэкап.
    if (r.deviceKey.empty()) {
        r.errorCode = BackupError::NoDeviceKey;
        return r;
    }

    std::sort(r.axes.begin(), r.axes.end(),
        [](const NamedEntry& a, const NamedEntry& b) { return a.index < b.index; });
    std::sort(r.buttons.begin(), r.buttons.end(),
        [](const NamedEntry& a, const NamedEntry& b) { return a.index < b.index; });

    r.valid = true;
    return r;
}

} // namespace BackupManager
