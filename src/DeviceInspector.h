#pragma once
#include <Windows.h>
#include <string>
#include <vector>

// DeviceInspector — чтение возможностей устройства через DirectInput8.
//
// Только ЧТЕНИЕ конфигурации (GetCapabilities / GetDeviceInfo / GetProperty /
// EnumObjects / EnumEffects). Устройство НЕ захватывается (без Acquire) и сила
// обратной связи НЕ подаётся — режим строго неэксклюзивный. Результат — дерево
// узлов «подпись → дочерние», которое UI рисует в TreeView.
//
// DirectInput видит только ПОДКЛЮЧЁННЫЕ устройства, поэтому для отключённого
// вернётся InspectError::NotConnected.

// Узел дерева сведений. Подпись локализуется в managed-слое: если labelKey
// не пуст — текст узла = Localization::T(labelKey) [+ ": " + value]; если пуст
// — value выводится как есть (для технических терминов и значений от драйвера).
struct InspectorNode {
    std::wstring               labelKey;   // ключ локализации подписи (или пусто)
    std::wstring               value;      // значение либо весь текст узла
    std::vector<InspectorNode> children;
};

// Код ошибки опроса. Текст формирует managed-слой (Localization).
enum class InspectError {
    None = 0,
    NotConnected,   // устройство не найдено среди подключённых (DI видит только attached)
    QueryFailed,    // не удалось создать устройство или прочитать его свойства
};

struct InspectorResult {
    bool          ok = false;
    InspectError  errorCode = InspectError::None;
    InspectorNode root;        // корневой узел устройства с вложенными разделами
};

namespace DeviceInspector {

    // Открывает устройство по oemKey ("VID_xxxx&PID_yyyy") через DirectInput8 и
    // читает его возможности. ownerWindow нужен для SetCooperativeLevel
    // (неэксклюзивный фоновый режим). Возвращает дерево InspectorResult.
    InspectorResult Inspect(const std::wstring& oemKey, HWND ownerWindow);
}
