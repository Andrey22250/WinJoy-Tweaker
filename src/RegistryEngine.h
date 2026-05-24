#pragma once
#include <Windows.h>
#include <string>
#include <vector>

// =====================================================================
// RegistryEngine — модуль чтения/записи параметров игровых контроллеров
// в реестре Windows (HKCU). Структуры данных — зеркало mmddk.h.
//
// В этом заголовке только декларации и POD-структуры. Реализация —
// в RegistryEngine.cpp.
// =====================================================================

// Запись о джойстике, найденном в реестре.
struct DeviceEntry {
    std::wstring displayName;   // OEMName или сам ключ, если имя не задано
    std::wstring registryKey;   // имя OEM-подраздела, например "VID_046D&PID_C29B"
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

// Биты JOY_HWS_* из mmddk.h. Дублируем здесь, чтобы не тащить mmsystem.h
// в managed-код UI.
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
    constexpr DWORD XISJ1Y      = 0x00000080;
    constexpr DWORD XISJ2X      = 0x00000100;
    constexpr DWORD XISJ2Y      = 0x00000200;
    constexpr DWORD X_AXIS_MASK = XISJ1Y | XISJ2X | XISJ2Y;

    // Маппинг оси Y (по умолчанию Y = J1 Y, все биты = 0)
    constexpr DWORD YISJ1X      = 0x00000400;
    constexpr DWORD YISJ2X      = 0x00000800;
    constexpr DWORD YISJ2Y      = 0x00001000;
    constexpr DWORD Y_AXIS_MASK = YISJ1X | YISJ2X | YISJ2Y;
}

// Одна запись подраздела Axes\<N> или Buttons\<N>.
// Индекс — порядковый номер из имени подключа ("0", "1", "10", ...).
// Имя — значение по умолчанию (@, REG_SZ), отображаемое DirectInput-играми
// и панелью «Свойства игрового устройства».
struct NamedEntry {
    int             index = 0;
    std::wstring    name;
};

// Код ошибки чтения. Текст сообщения формирует managed-слой (Localization)
// — нативный модуль не знает текущего языка UI.
enum class RegError {
    None = 0,
    OpenKeyFailed,   // не удалось открыть ключ реестра (errorDetail = LSTATUS)
};

// Полный набор данных, считанных для одного устройства из реестра.
struct DeviceData {
    std::wstring             oemName;       // значение OEMName (REG_SZ)
    std::vector<BYTE>        oemDataRaw;    // сырые байты OEMData (REG_BINARY)
    bool                     hasOemData = false;
    JOYREGHWCONFIG           hwConfig   = {};
    std::vector<NamedEntry>  axes;          // содержимое подключа Axes\<N>\@
    std::vector<NamedEntry>  buttons;       // содержимое подключа Buttons\<N>\@
    RegError                 errorCode  = RegError::None;
    LSTATUS                  errorDetail = 0; // системный код для подстановки в сообщение
};

namespace RegistryEngine {

    // --- Сканирование и чтение --------------------------------------------

    // Список OEM-ключей физически подключённых устройств (через DirectInput8).
    std::vector<std::wstring> GetConnectedOemKeys();

    // Список джойстиков из HKCU. По умолчанию — только подключённые в данный момент.
    std::vector<DeviceEntry> ScanDevices(bool onlyConnected = true);

    // Чтение OEMName + OEMData для конкретного устройства.
    DeviceData ReadDeviceData(const std::wstring& oemKey);

    // --- Запись -----------------------------------------------------------

    // Запись изменённых байт OEMData (REG_BINARY) в HKCU.
    LSTATUS WriteDeviceData(const std::wstring& oemKey,
                            const std::vector<BYTE>& rawBytes);

    // Запись изменённого OEMName (REG_SZ) в HKCU.
    LSTATUS WriteOemName(const std::wstring& oemKey,
                         const std::wstring& oemName);

    // Удаление значения OEMData / OEMName (для отката к «пустому» состоянию
    // при восстановлении бэкапа, в котором этих значений нет). Отсутствие
    // значения (ERROR_FILE_NOT_FOUND) трактуется как успех.
    LSTATUS DeleteOemData(const std::wstring& oemKey);
    LSTATUS DeleteOemName(const std::wstring& oemKey);

    // Запись имени оси или кнопки (значение @, REG_SZ) в HKCU.
    // Подключ должен уже существовать — функция НЕ создаёт оси/кнопки,
    // которых нет физически (это могло бы сломать перечисление в драйвере).
    LSTATUS WriteAxisName(const std::wstring& oemKey,
                          int index, const std::wstring& name);
    LSTATUS WriteButtonName(const std::wstring& oemKey,
                            int index, const std::wstring& name);
}
