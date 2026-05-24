#include "DeviceInspector.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <cwchar>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace {

// --- Форматирование значений -----------------------------------------

std::wstring GuidStr(const GUID& g)
{
    wchar_t buf[48];
    swprintf_s(buf, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        g.Data1, g.Data2, g.Data3,
        g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
        g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return buf;
}

std::wstring Hex32(DWORD v)
{
    wchar_t buf[16];
    swprintf_s(buf, L"0x%08lX", v);
    return buf;
}

// Имя оси по её guidType (X/Y/Z/Rx/Ry/Rz/Slider).
const wchar_t* AxisTypeName(const GUID& g)
{
    if (g == GUID_XAxis)  return L"X";
    if (g == GUID_YAxis)  return L"Y";
    if (g == GUID_ZAxis)  return L"Z";
    if (g == GUID_RxAxis) return L"Rx";
    if (g == GUID_RyAxis) return L"Ry";
    if (g == GUID_RzAxis) return L"Rz";
    if (g == GUID_Slider) return L"Slider";
    return L"?";
}

// Удобный helper добавления узла "подпись: значение".
void AddLeaf(InspectorNode& parent, const std::wstring& label, const std::wstring& value)
{
    InspectorNode n;
    n.text = label + L": " + value;
    parent.children.push_back(std::move(n));
}

// --- Поиск guidInstance по VID/PID среди подключённых -----------------

struct FindCtx {
    WORD vid = 0, pid = 0;
    GUID guid = GUID_NULL;
    bool found = false;
};

BOOL CALLBACK FindDeviceCb(LPCDIDEVICEINSTANCEW inst, LPVOID pv)
{
    FindCtx* c = static_cast<FindCtx*>(pv);
    const DWORD raw = inst->guidProduct.Data1;
    const WORD vid = static_cast<WORD>(raw & 0xFFFF);
    const WORD pid = static_cast<WORD>((raw >> 16) & 0xFFFF);
    if (vid == c->vid && pid == c->pid) {
        c->guid  = inst->guidInstance;
        c->found = true;
        return DIENUM_STOP;
    }
    return DIENUM_CONTINUE;
}

// --- Перечисление осей -------------------------------------------------

struct AxisEnumCtx {
    LPDIRECTINPUTDEVICE8W dev = nullptr;
    InspectorNode*        axesParent = nullptr;
};

BOOL CALLBACK EnumAxesCb(LPCDIDEVICEOBJECTINSTANCEW obj, LPVOID pv)
{
    AxisEnumCtx* c = static_cast<AxisEnumCtx*>(pv);

    InspectorNode axis;
    axis.text = obj->tszName;

    AddLeaf(axis, L"Тип", AxisTypeName(obj->guidType));
    AddLeaf(axis, L"Смещение (dwOfs)", std::to_wstring(obj->dwOfs));

    if (obj->dwType & DIDFT_FFACTUATOR) {
        AddLeaf(axis, L"Макс. сила FFB", std::to_wstring(obj->dwFFMaxForce));
        AddLeaf(axis, L"Разрешение силы FFB", std::to_wstring(obj->dwFFForceResolution));
    }

    // Диапазон / мёртвая зона / насыщение / гранулярность — по ID объекта.
    DIPROPRANGE range = {};
    range.diph.dwSize       = sizeof(DIPROPRANGE);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwHow        = DIPH_BYID;
    range.diph.dwObj        = obj->dwType;
    if (SUCCEEDED(c->dev->GetProperty(DIPROP_RANGE, &range.diph)))
        AddLeaf(axis, L"Логический диапазон",
            std::to_wstring(range.lMin) + L" … " + std::to_wstring(range.lMax));

    auto readDword = [&](const GUID& prop, const std::wstring& label) {
        DIPROPDWORD dw = {};
        dw.diph.dwSize       = sizeof(DIPROPDWORD);
        dw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dw.diph.dwHow        = DIPH_BYID;
        dw.diph.dwObj        = obj->dwType;
        if (SUCCEEDED(c->dev->GetProperty(prop, &dw.diph)))
            AddLeaf(axis, label, std::to_wstring(dw.dwData));
    };
    readDword(DIPROP_DEADZONE,    L"Мёртвая зона");
    readDword(DIPROP_SATURATION,  L"Зона насыщения");
    readDword(DIPROP_GRANULARITY, L"Гранулярность");

    AddLeaf(axis, L"dwType", Hex32(obj->dwType));
    AddLeaf(axis, L"dwFlags", Hex32(obj->dwFlags));

    c->axesParent->children.push_back(std::move(axis));
    return DIENUM_CONTINUE;
}

// --- Перечисление поддерживаемых эффектов -----------------------------

BOOL CALLBACK EnumEffectsCb(LPCDIEFFECTINFOW info, LPVOID pv)
{
    InspectorNode* parent = static_cast<InspectorNode*>(pv);
    AddLeaf(*parent, L"Эффект", info->tszName);
    return DIENUM_CONTINUE;
}

// Чтение DWORD-свойства уровня устройства.
bool ReadDeviceDword(LPDIRECTINPUTDEVICE8W dev, const GUID& prop, DWORD& out)
{
    DIPROPDWORD dw = {};
    dw.diph.dwSize       = sizeof(DIPROPDWORD);
    dw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dw.diph.dwHow        = DIPH_DEVICE;
    if (SUCCEEDED(dev->GetProperty(prop, &dw.diph))) { out = dw.dwData; return true; }
    return false;
}

} // namespace

namespace DeviceInspector {

InspectorResult Inspect(const std::wstring& oemKey, HWND ownerWindow)
{
    InspectorResult result;

    // Разбираем "VID_xxxx&PID_yyyy".
    FindCtx find;
    {
        size_t vpos = oemKey.find(L"VID_");
        size_t ppos = oemKey.find(L"PID_");
        if (vpos == std::wstring::npos || ppos == std::wstring::npos) {
            result.errorCode = InspectError::QueryFailed;
            return result;
        }
        find.vid = static_cast<WORD>(wcstoul(oemKey.c_str() + vpos + 4, nullptr, 16));
        find.pid = static_cast<WORD>(wcstoul(oemKey.c_str() + ppos + 4, nullptr, 16));
    }

    IDirectInput8W* di = nullptr;
    if (FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION,
            IID_IDirectInput8W, reinterpret_cast<void**>(&di), nullptr)) || !di) {
        result.errorCode = InspectError::QueryFailed;
        return result;
    }

    di->EnumDevices(DI8DEVCLASS_GAMECTRL, FindDeviceCb, &find, DIEDFL_ATTACHEDONLY);
    if (!find.found) {
        di->Release();
        result.errorCode = InspectError::NotConnected;   // DI видит только подключённые
        return result;
    }

    LPDIRECTINPUTDEVICE8W dev = nullptr;
    if (FAILED(di->CreateDevice(find.guid, &dev, nullptr)) || !dev) {
        di->Release();
        result.errorCode = InspectError::QueryFailed;
        return result;
    }

    // Формат данных нужен, чтобы GetProperty по осям (BYID) работал.
    dev->SetDataFormat(&c_dfDIJoystick2);
    // Неэксклюзивный фоновый режим: только читаем, ничего не захватываем.
    if (ownerWindow)
        dev->SetCooperativeLevel(ownerWindow, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);

    DIDEVCAPS caps = {};
    caps.dwSize = sizeof(DIDEVCAPS);
    dev->GetCapabilities(&caps);

    DIDEVICEINSTANCEW inst = {};
    inst.dwSize = sizeof(DIDEVICEINSTANCEW);
    dev->GetDeviceInfo(&inst);

    // --- Корневой узел устройства ---
    InspectorNode& root = result.root;
    root.text = inst.tszProductName[0] ? std::wstring(inst.tszProductName) : oemKey;

    AddLeaf(root, L"Instance GUID",  GuidStr(inst.guidInstance));
    AddLeaf(root, L"Product GUID",   GuidStr(inst.guidProduct));
    {
        wchar_t vp[16];
        swprintf_s(vp, L"VID_%04X", find.vid);  AddLeaf(root, L"VID", vp);
        swprintf_s(vp, L"PID_%04X", find.pid);  AddLeaf(root, L"PID", vp);
    }
    AddLeaf(root, L"Тип устройства",     Hex32(caps.dwDevType));
    AddLeaf(root, L"Instance name",      inst.tszInstanceName);
    AddLeaf(root, L"Product name",       inst.tszProductName);
    AddLeaf(root, L"FFB Driver GUID",    GuidStr(inst.guidFFDriver));
    AddLeaf(root, L"Usage page",         Hex32(inst.wUsagePage));
    AddLeaf(root, L"Usage",              Hex32(inst.wUsage));
    AddLeaf(root, L"Осей",               std::to_wstring(caps.dwAxes));
    AddLeaf(root, L"Кнопок",             std::to_wstring(caps.dwButtons));
    AddLeaf(root, L"POV",                std::to_wstring(caps.dwPOVs));
    AddLeaf(root, L"Версия прошивки",    std::to_wstring(caps.dwFirmwareRevision));
    AddLeaf(root, L"Версия железа",      std::to_wstring(caps.dwHardwareRevision));
    AddLeaf(root, L"Версия FFB-драйвера", std::to_wstring(caps.dwFFDriverVersion));
    AddLeaf(root, L"Флаги устройства",   Hex32(caps.dwFlags));

    DWORD tmp = 0;
    if (ReadDeviceDword(dev, DIPROP_AUTOCENTER, tmp))
        AddLeaf(root, L"Автоцентр", tmp ? L"Вкл" : L"Выкл");
    if (ReadDeviceDword(dev, DIPROP_AXISMODE, tmp))
        AddLeaf(root, L"Режим осей", tmp == DIPROPAXISMODE_REL ? L"Относительный" : L"Абсолютный");
    if (ReadDeviceDword(dev, DIPROP_BUFFERSIZE, tmp))
        AddLeaf(root, L"Размер буфера чтения", std::to_wstring(tmp));

    // --- Раздел FFB ---
    if (caps.dwFlags & DIDC_FORCEFEEDBACK) {
        InspectorNode ffb;
        ffb.text = L"Force Feedback";
        DWORD gain = 0;
        if (ReadDeviceDword(dev, DIPROP_FFGAIN, gain))
            AddLeaf(ffb, L"Gain", std::to_wstring(gain));
        AddLeaf(ffb, L"Период сэмпла", std::to_wstring(caps.dwFFSamplePeriod));
        AddLeaf(ffb, L"Мин. время", std::to_wstring(caps.dwFFMinTimeResolution));

        InspectorNode effects;
        effects.text = L"Поддерживаемые эффекты";
        dev->EnumEffects(EnumEffectsCb, &effects, DIEFT_ALL);
        if (!effects.children.empty())
            ffb.children.push_back(std::move(effects));

        root.children.push_back(std::move(ffb));
    }

    // --- Раздел осей ---
    InspectorNode axesNode;
    axesNode.text = L"Оси";
    AxisEnumCtx actx{ dev, &axesNode };
    dev->EnumObjects(EnumAxesCb, &actx, DIDFT_AXIS);
    if (!axesNode.children.empty())
        root.children.push_back(std::move(axesNode));

    dev->Release();
    di->Release();

    result.ok = true;
    return result;
}

} // namespace DeviceInspector
