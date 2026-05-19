#include "MainForm.h"
#include "RegistryEngine.h"
#include "BackupManager.h"
#include <msclr/marshal.h>
#include <msclr/marshal_cppstd.h>
#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")

using namespace WinJoytweaker;
using namespace System;
using namespace System::Windows::Forms;
using namespace System::Drawing;

// -----------------------------------------------------------------------
// Managed-обёртка над DeviceEntry для отображения в ComboBox
// -----------------------------------------------------------------------
ref struct DeviceInfo {
    String^ DisplayName;
    String^ RegistryKey;

    DeviceInfo(String^ dn, String^ rk) : DisplayName(dn), RegistryKey(rk) {}
    virtual String^ ToString() override { return DisplayName; }
};

// -----------------------------------------------------------------------
// Тёмная тема
// -----------------------------------------------------------------------
void MainForm::ApplyDarkTheme()
{
    Color bgDark  = Color::FromArgb(30,  30,  30);
    Color bgPanel = Color::FromArgb(45,  45,  48);
    Color textOn  = Color::FromArgb(220, 220, 220);
    Color textOff = Color::FromArgb(140, 140, 140);
    Color border  = Color::FromArgb(63,  63,  70);

    this->BackColor = bgDark;
    this->ForeColor = textOn;

    rootLayout->BackColor       = bgDark;
    innerLayoutFlags->BackColor = bgDark;

    // Метки
    labelDevice->ForeColor          = textOn;
    labelStatus->ForeColor          = textOff;
    labelOemNameCaption->ForeColor  = textOn;
    labelOemDataCaption->ForeColor  = textOn;
    labelDwNumButtons->ForeColor    = textOn;
    labelRawData->ForeColor          = textOn;
    labelDeviceTypeHeader->ForeColor = textOn;
    labelAxesHeader->ForeColor       = textOn;

    // ComboBox
    comboBoxDevice->BackColor = bgPanel;
    comboBoxDevice->ForeColor = textOn;
    comboBoxDevice->FlatStyle = FlatStyle::Flat;

    // Кнопка
    buttonRefresh->BackColor = bgPanel;
    buttonRefresh->ForeColor = textOn;
    buttonRefresh->FlatStyle = FlatStyle::Flat;
    buttonRefresh->FlatAppearance->BorderColor        = border;
    buttonRefresh->FlatAppearance->MouseOverBackColor = Color::FromArgb(62, 62, 64);
    buttonRefresh->FlatAppearance->MouseDownBackColor = Color::FromArgb(0, 122, 204);

    panelButtons->BackColor = bgDark;

    buttonOpenBackups->BackColor = bgPanel;
    buttonOpenBackups->ForeColor = textOn;
    buttonOpenBackups->FlatStyle = FlatStyle::Flat;
    buttonOpenBackups->FlatAppearance->BorderColor        = border;
    buttonOpenBackups->FlatAppearance->MouseOverBackColor = Color::FromArgb(62, 62, 64);
    buttonOpenBackups->FlatAppearance->MouseDownBackColor = Color::FromArgb(0, 122, 204);

    buttonBackup->BackColor = bgPanel;
    buttonBackup->ForeColor = textOn;
    buttonBackup->FlatStyle = FlatStyle::Flat;
    buttonBackup->FlatAppearance->BorderColor        = border;
    buttonBackup->FlatAppearance->MouseOverBackColor = Color::FromArgb(62, 62, 64);
    buttonBackup->FlatAppearance->MouseDownBackColor = Color::FromArgb(0, 122, 204);

    buttonRestore->BackColor = bgPanel;
    buttonRestore->ForeColor = textOn;
    buttonRestore->FlatStyle = FlatStyle::Flat;
    buttonRestore->FlatAppearance->BorderColor        = border;
    buttonRestore->FlatAppearance->MouseOverBackColor = Color::FromArgb(62, 62, 64);
    buttonRestore->FlatAppearance->MouseDownBackColor = Color::FromArgb(0, 122, 204);

    // «Применить» — акцентная синяя заливка для различимости
    buttonApply->BackColor = Color::FromArgb(0, 99, 177);
    buttonApply->ForeColor = textOn;
    buttonApply->FlatStyle = FlatStyle::Flat;
    buttonApply->FlatAppearance->BorderColor        = border;
    buttonApply->FlatAppearance->MouseOverBackColor = Color::FromArgb(0, 122, 204);
    buttonApply->FlatAppearance->MouseDownBackColor = Color::FromArgb(0, 80, 150);

    // TextBox'ы — единый стиль для всех (включая OEMName и оба read-only поля)
    array<TextBox^>^ textBoxes = gcnew array<TextBox^> {
        textBoxOemName, textBoxDwNumButtons, textBoxRawData, textBoxPreviewData
    };
    for each (TextBox^ tb in textBoxes) {
        tb->BackColor   = bgPanel;
        tb->ForeColor   = textOn;
        tb->BorderStyle = BorderStyle::FixedSingle;
    }

    // GroupBox: рамку Windows перекрасить штатно нельзя (она рисуется ОС),
    // только заголовок. Для контента используем тот же bgDark, чтобы
    // визуально интегрировалось с rootLayout.
    groupBoxFlags->BackColor = bgDark;
    groupBoxFlags->ForeColor = textOn;

    // RadioButton'ы и CheckBox'ы
    array<Control^>^ choices = gcnew array<Control^> {
        radioGeneric, radioYoke, radioGamepad, radioWheel,
        checkHasZ,    checkHasPov, checkPovIsButtonCombos, checkPovIsPoll,
        checkHasR, checkHasU, checkHasV,
        radioXDefault, radioXJ1Y, radioXJ2X, radioXJ2Y,
        radioYDefault, radioYJ1X, radioYJ2X, radioYJ2Y
    };
    for each (Control^ c in choices) {
        c->BackColor = bgDark;
        c->ForeColor = textOn;
    }

    panelXAxis->BackColor = bgDark;
    panelYAxis->BackColor = bgDark;
    labelXAxisHeader->ForeColor = textOn;
    labelYAxisHeader->ForeColor = textOn;

    comboBoxLanguage->BackColor = bgPanel;
    comboBoxLanguage->ForeColor = textOn;
}

// -----------------------------------------------------------------------
// Применение локализованных текстов ко всем UI-элементам.
// Вызывается в конструкторе и после переключения языка.
// -----------------------------------------------------------------------
void MainForm::ApplyLocalization()
{
    this->Text = Localization::T(L"form.title");

    labelDevice->Text         = Localization::T(L"label.device");
    buttonRefresh->Text       = Localization::T(L"button.refresh");
    labelOemNameCaption->Text = Localization::T(L"label.oemNameCaption");
    labelOemDataCaption->Text = Localization::T(L"label.oemDataCaption");
    labelDwNumButtons->Text   = Localization::T(L"label.dwNumButtons");
    groupBoxFlags->Text       = Localization::T(L"label.flagsGroup");
    labelDeviceTypeHeader->Text = Localization::T(L"label.deviceType");
    labelAxesHeader->Text     = Localization::T(L"label.axes");
    labelXAxisHeader->Text    = Localization::T(L"label.xAxis");
    labelYAxisHeader->Text    = Localization::T(L"label.yAxis");
    labelRawData->Text        = Localization::T(L"label.rawData");
    labelPreviewData->Text    = Localization::T(L"label.previewData");

    radioGeneric->Text  = Localization::T(L"device.generic");
    radioYoke->Text     = Localization::T(L"device.yoke");
    radioGamepad->Text  = Localization::T(L"device.gamepad");
    radioWheel->Text    = Localization::T(L"device.wheel");

    checkHasZ->Text                = Localization::T(L"axis.z");
    checkHasR->Text                = Localization::T(L"axis.r");
    checkHasU->Text                = Localization::T(L"axis.u");
    checkHasV->Text                = Localization::T(L"axis.v");
    checkHasPov->Text              = Localization::T(L"pov.label");
    checkPovIsButtonCombos->Text   = Localization::T(L"pov.buttonCombos");
    checkPovIsPoll->Text           = Localization::T(L"pov.poll");

    radioXDefault->Text = Localization::T(L"xAxis.default");
    radioXJ1Y->Text     = Localization::T(L"xAxis.j1y");
    radioXJ2X->Text     = Localization::T(L"xAxis.j2x");
    radioXJ2Y->Text     = Localization::T(L"xAxis.j2y");

    radioYDefault->Text = Localization::T(L"yAxis.default");
    radioYJ1X->Text     = Localization::T(L"yAxis.j1x");
    radioYJ2X->Text     = Localization::T(L"yAxis.j2x");
    radioYJ2Y->Text     = Localization::T(L"yAxis.j2y");

    buttonBackup->Text      = Localization::T(L"button.backup");
    buttonApply->Text       = Localization::T(L"button.apply");
    buttonOpenBackups->Text = Localization::T(L"button.openBackups");
    buttonRestore->Text     = Localization::T(L"button.restore");

    toolTipOemName->SetToolTip(textBoxOemName, Localization::T(L"tooltip.oemName"));
}

// Смена языка из combobox-а: сохраняем в settings.ini, применяем культуру,
// перезагружаем словарь строк, переименовываем все контролы. Перезапуск не нужен.
void MainForm::comboBoxLanguage_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e)
{
    Localization::Language pref = (Localization::Language)comboBoxLanguage->SelectedIndex;
    Localization::SavePreference(pref);
    Localization::ApplyPreference(pref);
    ApplyLocalization();
    // labelStatus («Подключено устройств: N» / «Устройств не найдено» / ошибки
    // сканирования) формируется в RefreshDeviceList и не охватывается
    // ApplyLocalization. Перевызываем сканирование, чтобы статус тоже
    // перевёлся на новый язык.
    RefreshDeviceList();
}

// -----------------------------------------------------------------------
// Сканирование реестра и заполнение ComboBox
// -----------------------------------------------------------------------
void MainForm::RefreshDeviceList()
{
    comboBoxDevice->BeginUpdate();
    comboBoxDevice->Items->Clear();

    try {
        auto devices = RegistryEngine::ScanDevices();

        for (const auto& d : devices) {
            String^ dn = gcnew String(d.displayName.c_str());
            String^ rk = gcnew String(d.registryKey.c_str());
            comboBoxDevice->Items->Add(gcnew DeviceInfo(dn, rk));
        }

        if (!devices.empty()) {
            comboBoxDevice->SelectedIndex = 0;
            // LoadDeviceData() вызовется автоматически через SelectedIndexChanged
            labelStatus->Text = Localization::T(L"status.devicesConnected", (int)devices.size());
        } else {
            textBoxOemName->Text = String::Empty;
            ClearFlagControls();
            labelStatus->Text = Localization::T(L"status.noDevices");
        }
    }
    catch (System::Exception^ ex) {
        labelStatus->Text = Localization::T(L"status.scanError", ex->Message);
    }
    catch (...) {
        labelStatus->Text = Localization::T(L"status.scanInternalError");
    }
    finally {
        comboBoxDevice->EndUpdate();
    }
}

void MainForm::buttonRefresh_Click(System::Object^ sender, System::EventArgs^ e)
{
    RefreshDeviceList();
}

// -----------------------------------------------------------------------
// Очистка контролов настройки (когда нет выбранного устройства)
// -----------------------------------------------------------------------
void MainForm::ClearFlagControls()
{
    textBoxDwNumButtons->Text = String::Empty;
    textBoxRawData->Text      = String::Empty;

    radioGeneric->Checked = false;
    radioYoke->Checked    = false;
    radioGamepad->Checked = false;
    radioWheel->Checked   = false;

    checkHasZ->Checked              = false;
    checkHasPov->Checked            = false;
    checkPovIsButtonCombos->Checked = false;
    checkPovIsButtonCombos->Visible = false;
    checkPovIsPoll->Checked         = false;
    checkPovIsPoll->Visible         = false;
    checkHasR->Checked              = false;
    checkHasU->Checked   = false;
    checkHasV->Checked   = false;

    radioXDefault->Checked = false;
    radioXJ1Y->Checked    = false;
    radioXJ2X->Checked    = false;
    radioXJ2Y->Checked    = false;

    radioYDefault->Checked = false;
    radioYJ1X->Checked    = false;
    radioYJ2X->Checked    = false;
    radioYJ2Y->Checked    = false;

    _oemDataSnapshot = nullptr;
    textBoxPreviewData->Text = String::Empty;

    groupBoxFlags->Enabled = false;
}

// -----------------------------------------------------------------------
// Загрузка и отображение данных выбранного устройства
// -----------------------------------------------------------------------
void MainForm::LoadDeviceData()
{
    DeviceInfo^ sel = dynamic_cast<DeviceInfo^>(comboBoxDevice->SelectedItem);
    if (sel == nullptr) {
        textBoxOemName->Text = String::Empty;
        ClearFlagControls();
        return;
    }

    msclr::interop::marshal_context ctx;
    std::wstring regKey = ctx.marshal_as<std::wstring>(sel->RegistryKey);

    DeviceData data = RegistryEngine::ReadDeviceData(regKey);

    if (!data.errorMessage.empty()) {
        labelStatus->Text = gcnew String(data.errorMessage.c_str());
        ClearFlagControls();
        return;
    }

    textBoxOemName->Text = gcnew String(data.oemName.c_str());

    // Снимок для предпросмотра изменений
    _oemDataSnapshot = gcnew array<System::Byte>((int)data.oemDataRaw.size());
    for (int i = 0; i < (int)data.oemDataRaw.size(); ++i)
        _oemDataSnapshot[i] = data.oemDataRaw[i];

    // Число кнопок — десятичное, без 0x-префикса
    if (data.hasOemData && data.oemDataRaw.size() >= 8) {
        DWORD numButtons = 0;
        CopyMemory(&numButtons, &data.oemDataRaw[4], sizeof(DWORD));
        textBoxDwNumButtons->Text = String::Format(L"{0}", numButtons);
    } else {
        textBoxDwNumButtons->Text = Localization::T(L"common.noDataPlaceholder");
    }

    // Сырые байты — хекс-строка вида "43 00 80 11 10 00 00 00"
    if (data.hasOemData && !data.oemDataRaw.empty()) {
        System::Text::StringBuilder^ sb = gcnew System::Text::StringBuilder(64);
        for (size_t i = 0; i < data.oemDataRaw.size(); ++i) {
            if (i > 0) sb->Append(L' ');
            sb->AppendFormat(L"{0:X2}", data.oemDataRaw[i]);
        }
        textBoxRawData->Text = sb->ToString();
    } else {
        textBoxRawData->Text = String::Empty;
    }

    // ── Парсинг dwFlags для UI-контролов ─────────────────────────────────
    // Если данных меньше 4 байт — отключаем блок настроек: нечего показывать.
    if (!data.hasOemData || data.oemDataRaw.size() < sizeof(DWORD)) {
        radioGeneric->Checked = false;
        radioYoke->Checked    = false;
        radioGamepad->Checked = false;
        radioWheel->Checked   = false;
        checkHasZ->Checked    = false;
        checkHasR->Checked    = false;
        checkHasU->Checked    = false;
        checkHasV->Checked    = false;
        groupBoxFlags->Enabled = false;
        return;
    }

    DWORD dwFlags = 0;
    CopyMemory(&dwFlags, data.oemDataRaw.data(), sizeof(DWORD));

    // Тип устройства — три бита взаимоисключающие; если ни один не стоит,
    // это «обычный джойстик».
    const DWORD typeBits = dwFlags & JoyHws::DEVICE_TYPE_MASK;
    radioYoke->Checked    = (typeBits == JoyHws::ISYOKE);
    radioGamepad->Checked = (typeBits == JoyHws::ISGAMEPAD);
    radioWheel->Checked   = (typeBits == JoyHws::ISCARCTRL);
    radioGeneric->Checked = (typeBits == 0);

    // Доступные оси — каждый бит независим.
    checkHasZ->Checked              = (dwFlags & JoyHws::HASZ)            != 0;
    checkHasPov->Checked            = (dwFlags & JoyHws::HASPOV)          != 0;
    checkPovIsButtonCombos->Checked = (dwFlags & JoyHws::POVISBUTTONCOMBOS) != 0;
    checkPovIsPoll->Checked         = (dwFlags & JoyHws::POVISPOLL)       != 0;
    checkPovIsButtonCombos->Visible = checkHasPov->Checked;
    checkPovIsPoll->Visible         = checkHasPov->Checked;
    checkHasR->Checked              = (dwFlags & JoyHws::HASR)            != 0;
    checkHasU->Checked   = (dwFlags & JoyHws::HASU)   != 0;
    checkHasV->Checked   = (dwFlags & JoyHws::HASV)   != 0;

    // Маппинг оси X — три бита взаимоисключающие; нет бит = J1 X по умолчанию.
    const DWORD xBits = dwFlags & JoyHws::X_AXIS_MASK;
    radioXDefault->Checked = (xBits == 0);
    radioXJ1Y->Checked     = (xBits == JoyHws::XISJ1Y);
    radioXJ2X->Checked     = (xBits == JoyHws::XISJ2X);
    radioXJ2Y->Checked     = (xBits == JoyHws::XISJ2Y);

    // Маппинг оси Y — три бита взаимоисключающие; нет бит = J1 Y по умолчанию.
    const DWORD yBits = dwFlags & JoyHws::Y_AXIS_MASK;
    radioYDefault->Checked = (yBits == 0);
    radioYJ1X->Checked     = (yBits == JoyHws::YISJ1X);
    radioYJ2X->Checked     = (yBits == JoyHws::YISJ2X);
    radioYJ2Y->Checked     = (yBits == JoyHws::YISJ2Y);

    groupBoxFlags->Enabled = true;
    UpdatePreviewData();
}

// -----------------------------------------------------------------------
// Пересчёт предпросмотра байт OEMData по текущему состоянию UI.
// Патчит только первые 4 байта (dwFlags) — остальные байты не трогает.
// -----------------------------------------------------------------------
void MainForm::UpdatePreviewData()
{
    if (_oemDataSnapshot == nullptr || _oemDataSnapshot->Length < 4) {
        textBoxPreviewData->Text = String::Empty;
        return;
    }

    // Копия снимка
    array<System::Byte>^ preview = (array<System::Byte>^)_oemDataSnapshot->Clone();

    // Читаем оригинальный dwFlags (little-endian)
    DWORD newFlags = (DWORD)preview[0]
                   | ((DWORD)preview[1] << 8)
                   | ((DWORD)preview[2] << 16)
                   | ((DWORD)preview[3] << 24);

    // Сбрасываем все биты, которые управляются UI
    newFlags &= ~(JoyHws::DEVICE_TYPE_MASK
                | JoyHws::HASZ | JoyHws::HASPOV | JoyHws::POVISBUTTONCOMBOS | JoyHws::POVISPOLL
                | JoyHws::HASR | JoyHws::HASU | JoyHws::HASV
                | JoyHws::X_AXIS_MASK | JoyHws::Y_AXIS_MASK);

    // Тип устройства
    if (radioYoke->Checked)    newFlags |= JoyHws::ISYOKE;
    if (radioGamepad->Checked) newFlags |= JoyHws::ISGAMEPAD;
    if (radioWheel->Checked)   newFlags |= JoyHws::ISCARCTRL;

    // Наличие осей
    if (checkHasZ->Checked)   newFlags |= JoyHws::HASZ;
    if (checkHasPov->Checked) {
        newFlags |= JoyHws::HASPOV;
        if (checkPovIsButtonCombos->Checked) newFlags |= JoyHws::POVISBUTTONCOMBOS;
        if (checkPovIsPoll->Checked)         newFlags |= JoyHws::POVISPOLL;
    }
    if (checkHasR->Checked)   newFlags |= JoyHws::HASR;
    if (checkHasU->Checked) newFlags |= JoyHws::HASU;
    if (checkHasV->Checked) newFlags |= JoyHws::HASV;

    // Маппинг X
    if (radioXJ1Y->Checked) newFlags |= JoyHws::XISJ1Y;
    if (radioXJ2X->Checked) newFlags |= JoyHws::XISJ2X;
    if (radioXJ2Y->Checked) newFlags |= JoyHws::XISJ2Y;

    // Маппинг Y
    if (radioYJ1X->Checked) newFlags |= JoyHws::YISJ1X;
    if (radioYJ2X->Checked) newFlags |= JoyHws::YISJ2X;
    if (radioYJ2Y->Checked) newFlags |= JoyHws::YISJ2Y;

    // Пишем dwFlags обратно в копию (little-endian)
    preview[0] = (System::Byte)( newFlags        & 0xFF);
    preview[1] = (System::Byte)((newFlags >>  8) & 0xFF);
    preview[2] = (System::Byte)((newFlags >> 16) & 0xFF);
    preview[3] = (System::Byte)((newFlags >> 24) & 0xFF);

    // Патчим dwNumButtons (байты 4-7) если есть место и значение валидно
    if (preview->Length >= 8) {
        int nb;
        if (int::TryParse(textBoxDwNumButtons->Text, nb) && nb >= 0 && nb <= 64) {
            DWORD dwNB = (DWORD)nb;
            preview[4] = (System::Byte)( dwNB        & 0xFF);
            preview[5] = (System::Byte)((dwNB >>  8) & 0xFF);
            preview[6] = (System::Byte)((dwNB >> 16) & 0xFF);
            preview[7] = (System::Byte)((dwNB >> 24) & 0xFF);
        }
    }

    // Hex-строка
    System::Text::StringBuilder^ sb = gcnew System::Text::StringBuilder(64);
    for (int i = 0; i < preview->Length; ++i) {
        if (i > 0) sb->Append(L' ');
        sb->AppendFormat(L"{0:X2}", preview[i]);
    }
    textBoxPreviewData->Text = sb->ToString();
}

void MainForm::flagsControl_Changed(System::Object^ sender, System::EventArgs^ e)
{
    UpdatePreviewData();
}

void MainForm::checkHasPov_CheckedChanged(System::Object^ sender, System::EventArgs^ e)
{
    bool hasPov = checkHasPov->Checked;
    checkPovIsButtonCombos->Visible = hasPov;
    checkPovIsPoll->Visible         = hasPov;
    if (!hasPov) {
        checkPovIsButtonCombos->Checked = false;
        checkPovIsPoll->Checked         = false;
    }
    UpdatePreviewData();
}

void MainForm::comboBoxDevice_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e)
{
    LoadDeviceData();
}

// Срабатывает один раз через 250 мс после последнего WM_DEVICECHANGE —
// серия системных уведомлений сворачивается в один реальный скан.
void MainForm::refreshDebounceTimer_Tick(System::Object^ sender, System::EventArgs^ e)
{
    refreshDebounceTimer->Stop();
    RefreshDeviceList();
}

// -----------------------------------------------------------------------
// Валидация ввода: только цифры, итоговое значение 0-64
// -----------------------------------------------------------------------
void MainForm::textBoxDwNumButtons_KeyPress(System::Object^ sender, System::Windows::Forms::KeyPressEventArgs^ e)
{
    if (e->KeyChar == '\b') return;
    if (!Char::IsDigit(e->KeyChar)) { e->Handled = true; return; }

    TextBox^ tb = safe_cast<TextBox^>(sender);
    String^ draft = tb->Text->Remove(tb->SelectionStart, tb->SelectionLength)
                             ->Insert(tb->SelectionStart, e->KeyChar.ToString());
    int val;
    if (int::TryParse(draft, val) && val > 64)
        e->Handled = true;
}

// Валидация ввода: только латинские буквы и пробел, длина до 128 (MaxLength)
void MainForm::textBoxOemName_KeyPress(System::Object^ sender, System::Windows::Forms::KeyPressEventArgs^ e)
{
    if (e->KeyChar == '\b') return;
    wchar_t c = e->KeyChar;
    if (!((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || c == L' '))
        e->Handled = true;
}

// -----------------------------------------------------------------------
// Вспомогательная функция: возвращает registryKey выбранного устройства
// или пустую строку, если ничего не выбрано.
// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// Кнопка «Сделать бэкап» — явный бэкап текущего состояния реестра
// -----------------------------------------------------------------------
void MainForm::buttonBackup_Click(System::Object^ sender, System::EventArgs^ e)
{
    DeviceInfo^ sel = dynamic_cast<DeviceInfo^>(comboBoxDevice->SelectedItem);
    if (sel == nullptr) { labelStatus->Text = Localization::T(L"status.noSelection"); return; }
    msclr::interop::marshal_context ctx;
    std::wstring oemKey = ctx.marshal_as<std::wstring>(sel->RegistryKey);
    if (oemKey.empty()) {
        labelStatus->Text = Localization::T(L"status.noSelection");
        return;
    }

    DeviceData data = RegistryEngine::ReadDeviceData(oemKey);
    if (!data.hasOemData) {
        labelStatus->Text = Localization::T(L"status.oemDataMissingBackup");
        return;
    }

    std::wstring backupPath = BackupManager::WriteBackup(oemKey, data.oemName, data.oemDataRaw);
    if (backupPath.empty()) {
        labelStatus->Text = Localization::T(L"status.backupFailed");
        return;
    }

    labelStatus->Text = Localization::T(L"status.backupSaved", gcnew String(backupPath.c_str()));
}

// -----------------------------------------------------------------------
// Кнопка «Применить» — автобэкап, затем запись изменений в реестр
// -----------------------------------------------------------------------
void MainForm::buttonApply_Click(System::Object^ sender, System::EventArgs^ e)
{
    DeviceInfo^ sel = dynamic_cast<DeviceInfo^>(comboBoxDevice->SelectedItem);
    if (sel == nullptr) { labelStatus->Text = Localization::T(L"status.noSelection"); return; }
    msclr::interop::marshal_context ctxKey;
    std::wstring oemKey = ctxKey.marshal_as<std::wstring>(sel->RegistryKey);

    // Читаем текущее состояние реестра для бэкапа (до изменений).
    DeviceData current = RegistryEngine::ReadDeviceData(oemKey);
    if (!current.hasOemData) {
        labelStatus->Text = Localization::T(L"status.oemDataMissingApply");
        return;
    }

    // Автоматический тихий бэкап перед любой записью.
    std::wstring backupPath = BackupManager::WriteBackup(oemKey, current.oemName, current.oemDataRaw);
    if (backupPath.empty()) {
        labelStatus->Text = Localization::T(L"status.backupBeforeApplyFailed");
        return;
    }

    // Валидация: поле количества кнопок не должно быть пустым.
    if (textBoxDwNumButtons->Text->Trim()->Length == 0) {
        labelStatus->Text = Localization::T(L"status.numButtonsEmpty");
        return;
    }

    // Валидация: OEMName не может состоять только из пробелов.
    String^ rawName = textBoxOemName->Text;
    if (rawName->Length > 0 && rawName->Trim()->Length == 0) {
        labelStatus->Text = Localization::T(L"status.oemNameOnlySpaces");
        return;
    }

    // Собираем новые байты из предпросмотра (они уже отражают текущий UI).
    if (textBoxPreviewData->Text->Length == 0) {
        labelStatus->Text = Localization::T(L"status.noDataToWrite");
        return;
    }

    // Конвертируем hex-строку предпросмотра обратно в байты.
    // Формат: "43 00 08 10 15 00 00 00" (пробелы как разделители).
    array<String^>^ parts = textBoxPreviewData->Text->Split(' ');
    std::vector<BYTE> newBytes;
    newBytes.reserve(parts->Length);
    for each (String^ tok in parts) {
        tok = tok->Trim();
        if (tok->Length == 0) continue;
        try {
            newBytes.push_back(static_cast<BYTE>(Convert::ToByte(tok, 16)));
        } catch (...) {
            labelStatus->Text = Localization::T(L"status.previewParseError");
            return;
        }
    }

    if (newBytes.empty()) {
        labelStatus->Text = Localization::T(L"status.noDataToWrite");
        return;
    }

    // Записываем OEMData.
    LSTATUS st = RegistryEngine::WriteDeviceData(oemKey, newBytes);
    if (st != ERROR_SUCCESS) {
        labelStatus->Text = Localization::T(L"status.writeOemDataError", st);
        return;
    }

    // Записываем OEMName, если поле непустое (rawName проверен выше).
    String^ newName = rawName->Trim();
    if (newName->Length > 0) {
        msclr::interop::marshal_context ctxName;
        std::wstring wName = ctxName.marshal_as<std::wstring>(newName);
        st = RegistryEngine::WriteOemName(oemKey, wName);
        if (st != ERROR_SUCCESS) {
            labelStatus->Text = Localization::T(L"status.writeOemNameAfterDataError", st);
            return;
        }
    }

    // Перечитываем устройство, чтобы снимок совпал с тем, что теперь в реестре.
    LoadDeviceData();

    labelStatus->Text = Localization::T(L"status.applied", gcnew String(backupPath.c_str()));
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// Кнопка «Папка бэкапов» — открывает папку в Проводнике
// -----------------------------------------------------------------------
void MainForm::buttonOpenBackups_Click(System::Object^ sender, System::EventArgs^ e)
{
    std::wstring dir = BackupManager::EnsureBackupDir();
    if (dir.empty()) {
        labelStatus->Text = Localization::T(L"status.backupDirFailed");
        return;
    }
    // ShellExecute открывает папку в Проводнике без запроса UAC.
    HINSTANCE result = ShellExecuteW(nullptr, L"explore", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
        labelStatus->Text = Localization::T(L"status.openBackupsFailed");
}

// -----------------------------------------------------------------------
// Кнопка «Восстановить» — выбор .reg-бэкапа и запись его в реестр.
// Перед записью делается автобэкап текущего состояния.
// -----------------------------------------------------------------------
void MainForm::buttonRestore_Click(System::Object^ sender, System::EventArgs^ e)
{
    DeviceInfo^ sel = dynamic_cast<DeviceInfo^>(comboBoxDevice->SelectedItem);
    if (sel == nullptr) {
        labelStatus->Text = Localization::T(L"status.noSelection");
        return;
    }

    msclr::interop::marshal_context ctx;
    std::wstring oemKey = ctx.marshal_as<std::wstring>(sel->RegistryKey);

    // Диалог выбора .reg-файла, по умолчанию открываем папку бэкапов.
    OpenFileDialog^ dlg = gcnew OpenFileDialog();
    dlg->Title  = Localization::T(L"restore.fileDialogTitle");
    dlg->Filter = Localization::T(L"restore.fileFilter");
    std::wstring backupDir = BackupManager::EnsureBackupDir();
    if (!backupDir.empty())
        dlg->InitialDirectory = gcnew String(backupDir.c_str());

    if (dlg->ShowDialog() != System::Windows::Forms::DialogResult::OK)
        return;

    std::wstring filePath = ctx.marshal_as<std::wstring>(dlg->FileName);
    auto parsed = BackupManager::ParseBackupFile(filePath);
    if (!parsed.valid) {
        labelStatus->Text = Localization::T(L"restore.parseError",
            gcnew String(parsed.errorMessage.c_str()));
        return;
    }

    // Собираем hex-строку байт OEMData из бэкапа для показа в диалоге.
    System::Text::StringBuilder^ hexBuilder = gcnew System::Text::StringBuilder((int)parsed.oemDataRaw.size() * 3);
    for (size_t i = 0; i < parsed.oemDataRaw.size(); ++i) {
        if (i > 0) hexBuilder->Append(L' ');
        hexBuilder->AppendFormat(L"{0:X2}", parsed.oemDataRaw[i]);
    }

    // Подтверждение: восстановление перезапишет текущие настройки устройства.
    String^ msg = Localization::T(L"restore.confirmText",
        gcnew String(parsed.oemName.c_str()),
        hexBuilder->ToString());

    if (MessageBox::Show(this, msg, Localization::T(L"restore.confirmTitle"),
            MessageBoxButtons::OKCancel, MessageBoxIcon::Question)
        != System::Windows::Forms::DialogResult::OK)
        return;

    // Пишем OEMData из бэкапа.
    LSTATUS st = RegistryEngine::WriteDeviceData(oemKey, parsed.oemDataRaw);
    if (st != ERROR_SUCCESS) {
        labelStatus->Text = Localization::T(L"status.writeOemDataError", st);
        return;
    }

    // Пишем OEMName, если он был в файле.
    if (!parsed.oemName.empty()) {
        st = RegistryEngine::WriteOemName(oemKey, parsed.oemName);
        if (st != ERROR_SUCCESS) {
            labelStatus->Text = Localization::T(L"restore.writeOemNameError", st);
            return;
        }
    }

    LoadDeviceData();
    labelStatus->Text = Localization::T(L"restore.success", dlg->SafeFileName);
}

// -----------------------------------------------------------------------
// Точка входа
// -----------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false);

    // Применяем сохранённое предпочтение языка до создания формы — чтобы
    // CurrentUICulture была корректной к моменту первого InitializeComponent.
    // Если settings.ini отсутствует (первый запуск) — Localization вернёт Auto,
    // а ApplyPreference(Auto) выставит InstalledUICulture (язык Windows).
    Localization::ApplyPreference(Localization::LoadPreference());

    Application::Run(gcnew MainForm);
    return 0;
}
