#pragma once
#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

// Хранит имя устройства и его ключ реестра
struct DeviceEntry {
    std::wstring displayName;
    std::wstring registryKey;
};

// Зеркало mmddk.h JOYREGHWCONFIG — структура, которой закодирован параметр
// OEMData. Полный размер по mmddk.h = 112 байт, но в реестре Windows для
// современных HID-устройств часто хранятся только первые 8 байт (структура
// hws) — остальное берётся из дефолтов и калибровки драйвера.
#pragma pack(push, 1)
struct JOYREGHWSETTINGS {
    DWORD dwFlags;       // флаги возможностей устройства (см. JOY_HWS_* ниже)
    DWORD dwNumButtons;  // количество кнопок
};
struct JOYPOS_OEM {
    DWORD dwX, dwY, dwZ, dwR, dwU, dwV;
};
struct JOYRANGE_OEM {
    JOYPOS_OEM jpMin, jpMax, jpCenter;
};
struct JOYREGHWVALUES {
    JOYRANGE_OEM jrvHardware;        // 72 байта
    DWORD        dwPOVValues[4];     // POV в каждом из 4 направлений
    DWORD        dwCalFlags;         // флаги калибровки
};
struct JOYREGHWCONFIG {
    JOYREGHWSETTINGS hws;             // [0..7]   8 байт
    DWORD            dwUsageSettings; // [8..11]
    JOYREGHWVALUES   hwv;             // [12..103] 92 байт
    DWORD            dwType;          // [104..107]
    DWORD            dwReserved;      // [108..111]
};
#pragma pack(pop)

// Биты JOY_HWS_* из mmddk.h. Дублируем константы здесь, чтобы не тащить
// mmsystem.h в managed-код UI.
namespace JoyHws {
    constexpr DWORD HASZ              = 0x00000001;  // bit 0 — Z-ось
    constexpr DWORD HASPOV            = 0x00000002;  // bit 1 — POV
    constexpr DWORD POVISBUTTONCOMBOS = 0x00000004;  // bit 2
    constexpr DWORD POVISPOLL         = 0x00000008;  // bit 3
    constexpr DWORD ISYOKE            = 0x00000010;  // bit 4 — штурвал
    constexpr DWORD ISGAMEPAD         = 0x00000020;  // bit 5 — геймпад
    constexpr DWORD ISCARCTRL         = 0x00000040;  // bit 6 — руль
    constexpr DWORD HASR              = 0x00080000;  // bit 19 — 4-я ось
    constexpr DWORD HASU              = 0x00800000;  // bit 23 — 5-я ось
    constexpr DWORD HASV              = 0x01000000;  // bit 24 — 6-я ось

    // Маска «типа устройства» — три бита взаимоисключающие.
    constexpr DWORD DEVICE_TYPE_MASK = ISYOKE | ISGAMEPAD | ISCARCTRL;

    // Маппинг оси X (по умолчанию X = J1 X, все биты = 0)
    constexpr DWORD XISJ1Y      = 0x00000080;  // X берётся с J1 Y
    constexpr DWORD XISJ2X      = 0x00000100;  // X берётся с J2 X
    constexpr DWORD XISJ2Y      = 0x00000200;  // X берётся с J2 Y
    constexpr DWORD X_AXIS_MASK = XISJ1Y | XISJ2X | XISJ2Y;

    // Маппинг оси Y (по умолчанию Y = J1 Y, все биты = 0)
    constexpr DWORD YISJ1X      = 0x00000400;  // Y берётся с J1 X
    constexpr DWORD YISJ2X      = 0x00000800;  // Y берётся с J2 X
    constexpr DWORD YISJ2Y      = 0x00001000;  // Y берётся с J2 Y
    constexpr DWORD Y_AXIS_MASK = YISJ1X | YISJ2X | YISJ2Y;
}

// Полный набор данных, считанных для одного устройства из реестра
struct DeviceData {
    std::wstring        oemName;       // значение OEMName (REG_SZ)
    std::vector<BYTE>   oemDataRaw;    // сырые байты OEMData (REG_BINARY)
    bool                hasOemData = false;
    JOYREGHWCONFIG      hwConfig   = {};
    std::wstring        errorMessage;
};

namespace RegistryEngine {

    static const wchar_t* JOYSTICK_REG_PATH =
        L"System\\CurrentControlSet\\Control\\MediaProperties"
        L"\\PrivateProperties\\Joystick\\OEM";

    // Возвращает имена OEM-подразделов (например, "VID_046D&PID_C29B"),
    // соответствующих физически подключённым в данный момент устройствам.
    //
    // Используется DirectInput8 (EnumDevices с DI8DEVCLASS_GAMECTRL): в отличие
    // от legacy winmm joystick API, корректно поддерживает hot-plug и видит
    // устройства, подключённые после старта процесса. VID/PID извлекаются из
    // DIDEVICEINSTANCE.guidProduct: первые 4 байта GUID кодируют PID:VID
    // (Data1 = (PID << 16) | VID).
    inline std::vector<std::wstring> GetConnectedOemKeys()
    {
        struct EnumCtx {
            std::vector<std::wstring> keys;
        } ctx;

        IDirectInput8W* di = nullptr;
        HRESULT hr = DirectInput8Create(GetModuleHandleW(nullptr),
            DIRECTINPUT_VERSION, IID_IDirectInput8W,
            reinterpret_cast<void**>(&di), nullptr);
        if (FAILED(hr) || !di) return ctx.keys;

        auto cb = [](LPCDIDEVICEINSTANCEW inst, LPVOID pv) -> BOOL {
            auto* c = static_cast<EnumCtx*>(pv);
            const DWORD raw = inst->guidProduct.Data1;
            const WORD vid = static_cast<WORD>(raw & 0xFFFF);
            const WORD pid = static_cast<WORD>((raw >> 16) & 0xFFFF);
            if (vid == 0 && pid == 0) return DIENUM_CONTINUE;

            wchar_t oemKey[32] = {};
            swprintf_s(oemKey, L"VID_%04X&PID_%04X", vid, pid);
            c->keys.emplace_back(oemKey);
            return DIENUM_CONTINUE;
        };

        di->EnumDevices(DI8DEVCLASS_GAMECTRL, cb, &ctx, DIEDFL_ATTACHEDONLY);
        di->Release();
        return ctx.keys;
    }

    // Возвращает список джойстиков из HKCU.
    // Если onlyConnected = true (по умолчанию), оставляет только те,
    // что физически подключены в данный момент.
    inline std::vector<DeviceEntry> ScanDevices(bool onlyConnected = true)
    {
        std::vector<DeviceEntry> devices;

        HKEY hRoot = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, JOYSTICK_REG_PATH,
                          0, KEY_READ, &hRoot) != ERROR_SUCCESS)
            return devices;

        std::vector<std::wstring> connected;
        if (onlyConnected) connected = GetConnectedOemKeys();

        DWORD index = 0;
        WCHAR subKeyName[256];
        DWORD subKeyLen;

        while (true) {
            subKeyLen = 256;
            LSTATUS st = RegEnumKeyExW(hRoot, index++,
                subKeyName, &subKeyLen,
                nullptr, nullptr, nullptr, nullptr);

            if (st == ERROR_NO_MORE_ITEMS) break;
            if (st != ERROR_SUCCESS) continue;

            // Фильтрация по списку подключённых.
            // Сравнение: подключ совпадает с активной записью без учёта регистра
            // ИЛИ начинается с неё (на случай суффиксов вида "&IG_00").
            if (onlyConnected) {
                bool alive = std::any_of(connected.begin(), connected.end(),
                    [&](const std::wstring& c) {
                        return _wcsnicmp(c.c_str(), subKeyName, c.length()) == 0;
                    });
                if (!alive) continue;
            }

            HKEY hSub = nullptr;
            if (RegOpenKeyExW(hRoot, subKeyName, 0, KEY_READ, &hSub) != ERROR_SUCCESS)
                continue;

            WCHAR oemName[256] = {};
            DWORD oemLen  = sizeof(oemName);
            DWORD valType = 0;

            LSTATUS qst = RegQueryValueExW(hSub, L"OEMName", nullptr, &valType,
                reinterpret_cast<LPBYTE>(oemName), &oemLen);

            DeviceEntry entry;
            entry.registryKey = subKeyName;
            entry.displayName = (qst == ERROR_SUCCESS && valType == REG_SZ && oemLen > sizeof(WCHAR))
                ? oemName
                : subKeyName;

            devices.push_back(std::move(entry));
            RegCloseKey(hSub);
        }

        RegCloseKey(hRoot);
        return devices;
    }

    // Читает OEMName и OEMData для конкретного устройства (по имени OEM-подраздела,
    // например "VID_046D&PID_C29B"). Десериализует OEMData в JOYREGHWCONFIG.
    inline DeviceData ReadDeviceData(const std::wstring& oemKey)
    {
        DeviceData result;

        std::wstring fullPath = std::wstring(JOYSTICK_REG_PATH) + L"\\" + oemKey;

        HKEY hKey = nullptr;
        LSTATUS st = RegOpenKeyExW(HKEY_CURRENT_USER, fullPath.c_str(),
                                   0, KEY_READ, &hKey);
        if (st != ERROR_SUCCESS) {
            result.errorMessage = L"Не удалось открыть ключ реестра (код: "
                                  + std::to_wstring(st) + L")";
            return result;
        }

        // --- OEMName (REG_SZ) ---
        WCHAR name[256] = {};
        DWORD nameLen = sizeof(name), valType = 0;
        if (RegQueryValueExW(hKey, L"OEMName", nullptr, &valType,
                             reinterpret_cast<LPBYTE>(name), &nameLen) == ERROR_SUCCESS
            && valType == REG_SZ)
        {
            result.oemName = name;
        }

        // --- OEMData (REG_BINARY): двухэтапное чтение ---
        // Сначала запрашиваем размер, потом сами байты. На текущем этапе
        // ограничиваемся сырыми данными; десериализация в JOYREGHWCONFIG —
        // следующим шагом, после ручной сверки байтов с RegEdit.
        DWORD dataSize = 0;
        st = RegQueryValueExW(hKey, L"OEMData", nullptr, &valType, nullptr, &dataSize);
        if (st == ERROR_SUCCESS && valType == REG_BINARY && dataSize > 0) {
            result.oemDataRaw.resize(dataSize);
            st = RegQueryValueExW(hKey, L"OEMData", nullptr, &valType,
                                  result.oemDataRaw.data(), &dataSize);
            if (st == ERROR_SUCCESS) {
                result.hasOemData = true;
                // Парсинг в JOYREGHWCONFIG возможен только если размер
                // достаточен. Иначе hwConfig останется нулевой.
                if (dataSize >= sizeof(JOYREGHWCONFIG)) {
                    result.hwConfig =
                        *reinterpret_cast<const JOYREGHWCONFIG*>(result.oemDataRaw.data());
                }
            } else {
                result.oemDataRaw.clear();
            }
        }

        RegCloseKey(hKey);
        return result;
    }

    // Создаёт папку для бэкапов и возвращает путь к ней (%APPDATA%\WinJoyTweaker\backups).
    // Возвращает пустую строку при ошибке.
    inline std::wstring EnsureBackupDir()
    {
        wchar_t appData[MAX_PATH] = {};
        if (!GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH))
            return L"";

        std::wstring dir = std::wstring(appData) + L"\\WinJoyTweaker\\backups";

        // Создаём обе части пути (WinJoyTweaker и backups) через два вызова.
        std::wstring parent = std::wstring(appData) + L"\\WinJoyTweaker";
        CreateDirectoryW(parent.c_str(), nullptr);   // ошибку игнорируем — папка может уже существовать
        CreateDirectoryW(dir.c_str(), nullptr);

        // Проверяем, что папка реально существует.
        DWORD attr = GetFileAttributesW(dir.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
            return L"";

        return dir;
    }

    // Сохраняет OEMName и OEMData конкретного устройства в файл формата
    // «Windows Registry Editor Version 5.00». Имя файла строится автоматически:
    // <oemKey>_<дата>_<время>.reg (например VID_046D&PID_C29B_20260517_143022.reg).
    //
    // Параметры:
    //   oemKey   — имя OEM-подраздела (например "VID_046D&PID_C29B")
    //   oemName  — текущее значение OEMName (REG_SZ)
    //   rawBytes — сырые байты OEMData (REG_BINARY)
    //   outDir   — директория для сохранения; если пусто — используется EnsureBackupDir()
    //
    // Возвращает полный путь к созданному файлу или пустую строку при ошибке.
    inline std::wstring WriteBackup(const std::wstring& oemKey,
                                    const std::wstring& oemName,
                                    const std::vector<BYTE>& rawBytes,
                                    const std::wstring& outDir = L"")
    {
        // Определяем директорию.
        std::wstring dir = outDir.empty() ? EnsureBackupDir() : outDir;
        if (dir.empty()) return L"";

        // Формируем временну́ю метку: YYYYMMDD_HHMMSS.
        SYSTEMTIME st = {};
        GetLocalTime(&st);
        wchar_t ts[20] = {};
        swprintf_s(ts, L"%04d%02d%02d_%02d%02d%02d",
                   st.wYear, st.wMonth, st.wDay,
                   st.wHour, st.wMinute, st.wSecond);

        // Имя файла: заменяем '&' на '_', чтобы избежать проблем в проводнике.
        std::wstring safeKey = oemKey;
        for (wchar_t& ch : safeKey) if (ch == L'&') ch = L'_';
        std::wstring filePath = dir + L"\\" + safeKey + L"_" + ts + L".reg";

        // Открываем файл на запись (UTF-16 LE с BOM — стандарт .reg Windows).
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return L"";

        // Вспомогательная лямбда для записи wide-строки.
        auto writeW = [&](const std::wstring& s) {
            DWORD written = 0;
            WriteFile(hFile, s.c_str(),
                      static_cast<DWORD>(s.size() * sizeof(wchar_t)),
                      &written, nullptr);
        };

        // BOM (0xFEFF).
        const wchar_t bom = L'\xFEFF';
        DWORD written = 0;
        WriteFile(hFile, &bom, sizeof(wchar_t), &written, nullptr);

        // Заголовок .reg файла.
        writeW(L"Windows Registry Editor Version 5.00\r\n\r\n");

        // Полный путь к ключу в реестре (HKCU).
        std::wstring fullRegPath =
            L"[HKEY_CURRENT_USER\\System\\CurrentControlSet\\Control\\"
            L"MediaProperties\\PrivateProperties\\Joystick\\OEM\\" + oemKey + L"]\r\n";
        writeW(fullRegPath);

        // OEMName (REG_SZ): экранируем обратные слэши и кавычки внутри строки.
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

        // Ротация: оставляем не более 15 .reg файлов в папке бэкапов.
        // Собираем все файлы с их временем последней записи, сортируем по возрастанию,
        // удаляем самые старые, пока файлов не станет <= 15.
        {
            constexpr size_t MAX_BACKUPS = 15;
            std::vector<std::pair<FILETIME, std::wstring>> files; // {время, путь}

            std::wstring pattern = dir + L"\\*.reg";
            WIN32_FIND_DATAW fd = {};
            HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                        files.push_back({ fd.ftLastWriteTime, dir + L"\\" + fd.cFileName });
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }

            if (files.size() > MAX_BACKUPS) {
                // Сортируем по времени: старые — первыми.
                std::sort(files.begin(), files.end(),
                    [](const auto& a, const auto& b) {
                        if (a.first.dwHighDateTime != b.first.dwHighDateTime)
                            return a.first.dwHighDateTime < b.first.dwHighDateTime;
                        return a.first.dwLowDateTime < b.first.dwLowDateTime;
                    });
                // Удаляем лишние (с начала отсортированного списка).
                size_t toDelete = files.size() - MAX_BACKUPS;
                for (size_t i = 0; i < toDelete; ++i)
                    DeleteFileW(files[i].second.c_str());
            }
        }

        return filePath;
    }

    // Записывает изменённые байты OEMData обратно в реестр (HKCU).
    // Возвращает ERROR_SUCCESS при успехе или код ошибки Win32.
    inline LSTATUS WriteDeviceData(const std::wstring& oemKey,
                                   const std::vector<BYTE>& rawBytes)
    {
        std::wstring fullPath = std::wstring(JOYSTICK_REG_PATH) + L"\\" + oemKey;

        HKEY hKey = nullptr;
        LSTATUS st = RegOpenKeyExW(HKEY_CURRENT_USER, fullPath.c_str(),
                                   0, KEY_SET_VALUE, &hKey);
        if (st != ERROR_SUCCESS) return st;

        st = RegSetValueExW(hKey, L"OEMData", 0, REG_BINARY,
                            rawBytes.data(),
                            static_cast<DWORD>(rawBytes.size()));
        RegCloseKey(hKey);
        return st;
    }

    // Записывает изменённое OEMName обратно в реестр (HKCU).
    // Возвращает ERROR_SUCCESS при успехе или код ошибки Win32.
    inline LSTATUS WriteOemName(const std::wstring& oemKey,
                                const std::wstring& oemName)
    {
        std::wstring fullPath = std::wstring(JOYSTICK_REG_PATH) + L"\\" + oemKey;

        HKEY hKey = nullptr;
        LSTATUS st = RegOpenKeyExW(HKEY_CURRENT_USER, fullPath.c_str(),
                                   0, KEY_SET_VALUE, &hKey);
        if (st != ERROR_SUCCESS) return st;

        // Размер включает завершающий нулевой символ.
        DWORD byteSize = static_cast<DWORD>((oemName.size() + 1) * sizeof(wchar_t));
        st = RegSetValueExW(hKey, L"OEMName", 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(oemName.c_str()),
                            byteSize);
        RegCloseKey(hKey);
        return st;
    }
}
