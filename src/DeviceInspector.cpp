#include "DeviceInspector.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <cwchar>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace {

// Форматирование значений

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

// Узел с локализуемой подписью: "T(labelKey): value".
void AddLeaf(InspectorNode& parent, const std::wstring& labelKey, const std::wstring& value)
{
    InspectorNode n;
    n.labelKey = labelKey;
    n.value    = value;
    parent.children.push_back(std::move(n));
}

// Узел с готовым (нелокализуемым) текстом — техн. термины и значения драйвера.
void AddText(InspectorNode& parent, const std::wstring& text)
{
    InspectorNode n;
    n.value = text;   // labelKey пуст
    parent.children.push_back(std::move(n));
}

// Поиск guidInstance по VID/PID среди подключённых

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

// Перечисление осей

struct AxisEnumCtx {
    LPDIRECTINPUTDEVICE8W dev = nullptr;
    InspectorNode*        axesParent = nullptr;
};

BOOL CALLBACK EnumAxesCb(LPCDIDEVICEOBJECTINSTANCEW obj, LPVOID pv)
{
    AxisEnumCtx* c = static_cast<AxisEnumCtx*>(pv);

    InspectorNode axis;
    axis.value = obj->tszName;   // имя оси от драйвера — не локализуем

    AddLeaf(axis, L"info.axisType", AxisTypeName(obj->guidType));
    AddLeaf(axis, L"info.axisOfs", std::to_wstring(obj->dwOfs));

    if (obj->dwType & DIDFT_FFACTUATOR) {
        AddLeaf(axis, L"info.axisFfbMaxForce", std::to_wstring(obj->dwFFMaxForce));
        AddLeaf(axis, L"info.axisFfbForceRes", std::to_wstring(obj->dwFFForceResolution));
    }

    // Диапазон / мёртвая зона / насыщение / гранулярность — по ID объекта.
    DIPROPRANGE range = {};
    range.diph.dwSize       = sizeof(DIPROPRANGE);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwHow        = DIPH_BYID;
    range.diph.dwObj        = obj->dwType;
    if (SUCCEEDED(c->dev->GetProperty(DIPROP_RANGE, &range.diph)))
        AddLeaf(axis, L"info.axisRange",
            std::to_wstring(range.lMin) + L" … " + std::to_wstring(range.lMax));

    auto readDword = [&](const GUID& prop, const std::wstring& key) {
        DIPROPDWORD dw = {};
        dw.diph.dwSize       = sizeof(DIPROPDWORD);
        dw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dw.diph.dwHow        = DIPH_BYID;
        dw.diph.dwObj        = obj->dwType;
        if (SUCCEEDED(c->dev->GetProperty(prop, &dw.diph)))
            AddLeaf(axis, key, std::to_wstring(dw.dwData));
    };
    readDword(DIPROP_DEADZONE,    L"info.axisDeadZone");
    readDword(DIPROP_SATURATION,  L"info.axisSaturation");
    readDword(DIPROP_GRANULARITY, L"info.axisGranularity");

    AddText(axis, L"dwType: " + Hex32(obj->dwType));
    AddText(axis, L"dwFlags: " + Hex32(obj->dwFlags));

    c->axesParent->children.push_back(std::move(axis));
    return DIENUM_CONTINUE;
}

// Перечисление поддерживаемых эффектов

BOOL CALLBACK EnumEffectsCb(LPCDIEFFECTINFOW info, LPVOID pv)
{
    InspectorNode* parent = static_cast<InspectorNode*>(pv);
    AddText(*parent, info->tszName);   // имя эффекта от драйвера — не локализуем
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

    // Корневой узел устройства
    InspectorNode& root = result.root;
    root.value = inst.tszProductName[0] ? std::wstring(inst.tszProductName) : oemKey;

    // Технические термины (GUID/VID/PID/Usage) — вербатим, одинаково в RU/EN.
    AddText(root, L"Instance GUID: " + GuidStr(inst.guidInstance));
    AddText(root, L"Product GUID: "  + GuidStr(inst.guidProduct));
    {
        wchar_t vp[16];
        swprintf_s(vp, L"VID_%04X", find.vid);  AddText(root, std::wstring(L"VID: ") + vp);
        swprintf_s(vp, L"PID_%04X", find.pid);  AddText(root, std::wstring(L"PID: ") + vp);
    }
    AddLeaf(root, L"info.devType",        Hex32(caps.dwDevType));
    AddText(root, L"Instance name: " + std::wstring(inst.tszInstanceName));
    AddText(root, L"Product name: "  + std::wstring(inst.tszProductName));
    AddText(root, L"FFB Driver GUID: " + GuidStr(inst.guidFFDriver));
    AddText(root, L"Usage page: " + Hex32(inst.wUsagePage));
    AddText(root, L"Usage: "      + Hex32(inst.wUsage));
    AddLeaf(root, L"info.axesCount",      std::to_wstring(caps.dwAxes));
    AddLeaf(root, L"info.buttonsCount",   std::to_wstring(caps.dwButtons));
    AddLeaf(root, L"info.povCount",       std::to_wstring(caps.dwPOVs));
    AddLeaf(root, L"info.firmwareRev",    std::to_wstring(caps.dwFirmwareRevision));
    AddLeaf(root, L"info.hardwareRev",    std::to_wstring(caps.dwHardwareRevision));
    AddLeaf(root, L"info.ffbDriverVer",   std::to_wstring(caps.dwFFDriverVersion));
    AddLeaf(root, L"info.deviceFlags",    Hex32(caps.dwFlags));

    DWORD tmp = 0;
    if (ReadDeviceDword(dev, DIPROP_AUTOCENTER, tmp))
        AddLeaf(root, tmp ? L"info.autoCenterOn" : L"info.autoCenterOff", L"");
    if (ReadDeviceDword(dev, DIPROP_AXISMODE, tmp))
        AddLeaf(root, tmp == DIPROPAXISMODE_REL ? L"info.axisModeRel" : L"info.axisModeAbs", L"");
    if (ReadDeviceDword(dev, DIPROP_BUFFERSIZE, tmp))
        AddLeaf(root, L"info.bufferSize", std::to_wstring(tmp));

    // Раздел FFB
    if (caps.dwFlags & DIDC_FORCEFEEDBACK) {
        InspectorNode ffb;
        ffb.value = L"Force Feedback";   // термин — вербатим
        DWORD gain = 0;
        if (ReadDeviceDword(dev, DIPROP_FFGAIN, gain))
            AddText(ffb, L"Gain: " + std::to_wstring(gain));
        AddLeaf(ffb, L"info.ffbSamplePeriod", std::to_wstring(caps.dwFFSamplePeriod));
        AddLeaf(ffb, L"info.ffbMinTime", std::to_wstring(caps.dwFFMinTimeResolution));

        InspectorNode effects;
        effects.labelKey = L"info.sectionEffects";
        dev->EnumEffects(EnumEffectsCb, &effects, DIEFT_ALL);
        if (!effects.children.empty())
            ffb.children.push_back(std::move(effects));

        root.children.push_back(std::move(ffb));
    }

    // Раздел осей
    InspectorNode axesNode;
    axesNode.labelKey = L"info.sectionAxes";
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
