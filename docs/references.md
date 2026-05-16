# Справочник API и библиотек проекта WinJoy-Tweaker

Сводка ссылок на официальную документацию Microsoft по всем API,
которые используются (или планируются к использованию) в проекте.
Полезно для дипломной записки и для самостоятельного углубления.

## 1. Работа с реестром (Advapi32.lib)

Заголовок: `<windows.h>` / `<winreg.h>`. Куст: `HKEY_CURRENT_USER`.

- [RegOpenKeyExW](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regopenkeyexw) — открытие подраздела реестра.
- [RegEnumKeyExW](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regenumkeyexw) — перебор подразделов (для Scanner Module).
- [RegQueryValueExW](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regqueryvalueexw) — чтение значения (`OEMName`, `OEMData`).
- [RegSetValueExW](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regsetvalueexw) — запись значения.
- [RegCloseKey](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regclosekey) — закрытие хэндла.
- [Registry Value Types (REG_BINARY, REG_SZ и т.д.)](https://learn.microsoft.com/en-us/windows/win32/sysinfo/registry-value-types).
- [LSTATUS / системные коды ошибок](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes).
- [.reg-файлы (формат Registry Editor 5.00)](https://learn.microsoft.com/en-us/troubleshoot/windows-server/performance/windows-registry-advanced-users) — для Safety Manager (бэкапы).

## 2. Джойстики и игровые контроллеры (winmm)

- [Joystick Reference (winmm, joyGetDevCaps и др.)](https://learn.microsoft.com/en-us/windows/win32/multimedia/joystick-reference).
- [JOYREGHWCONFIG structure (mmddk.h)](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/mmddk/ns-mmddk-joyreghwconfig) — главная структура для парсинга `OEMData`.
- [Reading joystick configuration from registry (концепция)](https://learn.microsoft.com/en-us/windows/win32/multimedia/setting-joystick-properties).
- [DirectInput overview](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416842(v=vs.85)) — устаревший, но контекстно важный для понимания, как игры читают оси.

## 3. Уведомления о подключении устройств

- [WM_DEVICECHANGE message](https://learn.microsoft.com/en-us/windows/win32/devio/wm-devicechange) — главное сообщение для авто-обновления списка.
- [DBT_DEVNODES_CHANGED](https://learn.microsoft.com/en-us/windows/win32/devio/dbt-devnodes-changed) — событие изменения дерева устройств (используем).
- [DBT_DEVICEARRIVAL](https://learn.microsoft.com/en-us/windows/win32/devio/dbt-devicearrival) / [DBT_DEVICEREMOVECOMPLETE](https://learn.microsoft.com/en-us/windows/win32/devio/dbt-deviceremovecomplete) — точечные события (требуют регистрации).
- [RegisterDeviceNotificationW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerdevicenotificationw) — фильтр по классу устройства (HID GUID).
- [DEV_BROADCAST_DEVICEINTERFACE_W](https://learn.microsoft.com/en-us/windows/win32/api/dbt/ns-dbt-dev_broadcast_deviceinterface_w) — структура с GUID интерфейса.
- [HID Class GUID](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/hid-architecture) — `{4D1E55B2-F16F-11CF-88CB-001111000030}`.

## 4. Windows Forms (.NET, managed UI)

- [System.Windows.Forms namespace](https://learn.microsoft.com/en-us/dotnet/api/system.windows.forms).
- [Form class](https://learn.microsoft.com/en-us/dotnet/api/system.windows.forms.form).
- [Form.WndProc(Message)](https://learn.microsoft.com/en-us/dotnet/api/system.windows.forms.form.wndproc) — переопределение для обработки `WM_DEVICECHANGE`.
- [Message struct](https://learn.microsoft.com/en-us/dotnet/api/system.windows.forms.message).
- [System.Windows.Forms.Timer](https://learn.microsoft.com/en-us/dotnet/api/system.windows.forms.timer) — debounce-таймер (UI-поток).
- [TableLayoutPanel](https://learn.microsoft.com/en-us/dotnet/api/system.windows.forms.tablelayoutpanel) — раскладка формы.
- [ComboBox](https://learn.microsoft.com/en-us/dotnet/api/system.windows.forms.combobox), [Button](https://learn.microsoft.com/en-us/dotnet/api/system.windows.forms.button), [Label](https://learn.microsoft.com/en-us/dotnet/api/system.windows.forms.label).

## 5. C++/CLI (мост между managed и native)

- [C++/CLI Language Reference](https://learn.microsoft.com/en-us/cpp/extensions/component-extensions-for-runtime-platforms).
- [Handle to Object Operator (^)](https://learn.microsoft.com/en-us/cpp/extensions/handle-to-object-operator-hat-cpp-component-extensions) — managed-дескриптор.
- [gcnew](https://learn.microsoft.com/en-us/cpp/extensions/ref-new-gcnew-cpp-component-extensions) — выделение в managed-куче.
- [ref class / ref struct](https://learn.microsoft.com/en-us/cpp/extensions/classes-and-structs-cpp-component-extensions).
- [msclr::interop::marshal_as](https://learn.microsoft.com/en-us/cpp/dotnet/marshal-as) — конвертация `String^` ↔ `std::wstring`.
- [Mixed (Native and Managed) Assemblies](https://learn.microsoft.com/en-us/cpp/dotnet/mixed-native-and-managed-assemblies).

## 6. Стандартный C++ (native-ядро)

- [\<string\>](https://en.cppreference.com/w/cpp/header/string) — `std::wstring` для путей и имён.
- [\<vector\>](https://en.cppreference.com/w/cpp/header/vector) — буфер `OEMData`.
- [\<memory\>](https://en.cppreference.com/w/cpp/header/memory) — `std::unique_ptr` для RAII.
- [reinterpret_cast](https://en.cppreference.com/w/cpp/language/reinterpret_cast) — кастинг байтового буфера в `JOYREGHWCONFIG`.
- [#pragma pack](https://learn.microsoft.com/en-us/cpp/preprocessor/pack) — выравнивание полей структуры.

## 7. Инструментарий

- [Visual Studio C++ docs](https://learn.microsoft.com/en-us/cpp/).
- [Linker input (Advapi32.lib, Winmm.lib, User32.lib)](https://learn.microsoft.com/en-us/cpp/build/reference/additional-dependencies).
- [Spy++ (просмотр Windows-сообщений)](https://learn.microsoft.com/en-us/visualstudio/debugger/introducing-spy-increment) — для отладки `WM_DEVICECHANGE`.
- [RegEdit / Registry Editor](https://learn.microsoft.com/en-us/windows/win32/sysinfo/registry) — ручная проверка ветки `HKCU\System\CurrentControlSet\Control\MediaProperties\PrivateProperties\Joystick\OEM`.

## 8. Полезное чтение по теме ВКР

- [Game Controllers (Microsoft Hardware docs)](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/top-level-collections-opened-by-windows-for-system-use).
- [HID Usages — Game Controls page](https://usb.org/sites/default/files/hut1_22.pdf) — официальная спецификация HID Usage Tables (PDF, USB-IF).
- [Raymond Chen — The Old New Thing](https://devblogs.microsoft.com/oldnewthing/) — поиск по `WM_DEVICECHANGE`, много нюансов.
