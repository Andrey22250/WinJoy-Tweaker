#include "RegistryEngine.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <algorithm>
#include <cstdlib>   // wcstol

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace {
    // Корневой путь к ветке джойстиков в HKCU.
    constexpr const wchar_t* JOYSTICK_REG_PATH =
        L"System\\CurrentControlSet\\Control\\MediaProperties"
        L"\\PrivateProperties\\Joystick\\OEM";
}

namespace RegistryEngine {

// =====================================================================
// Сканирование и чтение
// =====================================================================

// Возвращает имена OEM-подразделов (например, "VID_046D&PID_C29B"),
// соответствующих физически подключённым в данный момент устройствам.
//
// Используется DirectInput8 (EnumDevices с DI8DEVCLASS_GAMECTRL): в отличие
// от legacy winmm joystick API, корректно поддерживает hot-plug и видит
// устройства, подключённые после старта процесса. VID/PID извлекаются из
// DIDEVICEINSTANCE.guidProduct: первые 4 байта GUID кодируют PID:VID
// (Data1 = (PID << 16) | VID).
std::vector<std::wstring> GetConnectedOemKeys()
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

std::vector<DeviceEntry> ScanDevices(bool onlyConnected)
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

DeviceData ReadDeviceData(const std::wstring& oemKey)
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
    DWORD dataSize = 0;
    st = RegQueryValueExW(hKey, L"OEMData", nullptr, &valType, nullptr, &dataSize);
    if (st == ERROR_SUCCESS && valType == REG_BINARY && dataSize > 0) {
        result.oemDataRaw.resize(dataSize);
        st = RegQueryValueExW(hKey, L"OEMData", nullptr, &valType,
                              result.oemDataRaw.data(), &dataSize);
        if (st == ERROR_SUCCESS) {
            result.hasOemData = true;
            // Парсинг в JOYREGHWCONFIG возможен только если размер достаточен.
            if (dataSize >= sizeof(JOYREGHWCONFIG)) {
                result.hwConfig =
                    *reinterpret_cast<const JOYREGHWCONFIG*>(result.oemDataRaw.data());
            }
        } else {
            result.oemDataRaw.clear();
        }
    }

    // --- Подразделы Axes\<N> и Buttons\<N> (опциональны) ---
    auto readNamedSubkeys = [&](const wchar_t* container,
                                std::vector<NamedEntry>& out)
    {
        HKEY hContainer = nullptr;
        std::wstring containerPath = fullPath + L"\\" + container;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, containerPath.c_str(),
                          0, KEY_READ, &hContainer) != ERROR_SUCCESS)
            return;

        DWORD idx = 0;
        WCHAR sub[64];
        while (true) {
            DWORD subLen = 64;
            LSTATUS s = RegEnumKeyExW(hContainer, idx++,
                sub, &subLen, nullptr, nullptr, nullptr, nullptr);
            if (s == ERROR_NO_MORE_ITEMS) break;
            if (s != ERROR_SUCCESS) continue;

            // Имя подключа — десятичный индекс ("0", "1", "10", ...).
            // Парсим вручную: wcstol допускает ведущие пробелы/знак, но
            // реестр их не содержит — лишних проверок не делаем.
            wchar_t* end = nullptr;
            long parsed = wcstol(sub, &end, 10);
            if (end == sub || *end != L'\0') continue;

            HKEY hEntry = nullptr;
            if (RegOpenKeyExW(hContainer, sub, 0, KEY_READ, &hEntry) != ERROR_SUCCESS)
                continue;

            WCHAR nm[256] = {};
            DWORD nmLen = sizeof(nm), tp = 0;
            // Значение по умолчанию — name == nullptr или L"".
            LSTATUS qs = RegQueryValueExW(hEntry, nullptr, nullptr, &tp,
                reinterpret_cast<LPBYTE>(nm), &nmLen);

            NamedEntry e;
            e.index = static_cast<int>(parsed);
            if (qs == ERROR_SUCCESS && tp == REG_SZ && nmLen > sizeof(WCHAR))
                e.name = nm;
            out.push_back(std::move(e));
            RegCloseKey(hEntry);
        }
        RegCloseKey(hContainer);

        // Реестр перечисляет подключи в алфавитном порядке ("0","1","10","2",…)
        // — для UI логичнее числовая сортировка по индексу.
        std::sort(out.begin(), out.end(),
            [](const NamedEntry& a, const NamedEntry& b) {
                return a.index < b.index;
            });
    };

    readNamedSubkeys(L"Axes",    result.axes);
    readNamedSubkeys(L"Buttons", result.buttons);

    RegCloseKey(hKey);
    return result;
}

// =====================================================================
// Запись в реестр
// =====================================================================

LSTATUS WriteDeviceData(const std::wstring& oemKey,
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

LSTATUS WriteOemName(const std::wstring& oemKey,
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

// Общая реализация записи имени для Axes\<N> и Buttons\<N>: значение @ типа
// REG_SZ. Подключ не создаём — если он отсутствует, возвращаем ошибку открытия,
// чтобы не плодить «фантомные» оси/кнопки, которых нет в драйвере.
static LSTATUS WriteNamedEntry(const std::wstring& oemKey,
                               const wchar_t* container,
                               int index,
                               const std::wstring& name)
{
    std::wstring fullPath = std::wstring(JOYSTICK_REG_PATH) + L"\\" + oemKey
                          + L"\\" + container + L"\\" + std::to_wstring(index);

    HKEY hKey = nullptr;
    LSTATUS st = RegOpenKeyExW(HKEY_CURRENT_USER, fullPath.c_str(),
                               0, KEY_SET_VALUE, &hKey);
    if (st != ERROR_SUCCESS) return st;

    DWORD byteSize = static_cast<DWORD>((name.size() + 1) * sizeof(wchar_t));
    st = RegSetValueExW(hKey, nullptr, 0, REG_SZ,
                        reinterpret_cast<const BYTE*>(name.c_str()),
                        byteSize);
    RegCloseKey(hKey);
    return st;
}

LSTATUS WriteAxisName(const std::wstring& oemKey,
                      int index, const std::wstring& name)
{
    return WriteNamedEntry(oemKey, L"Axes", index, name);
}

LSTATUS WriteButtonName(const std::wstring& oemKey,
                        int index, const std::wstring& name)
{
    return WriteNamedEntry(oemKey, L"Buttons", index, name);
}

} // namespace RegistryEngine
