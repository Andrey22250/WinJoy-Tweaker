#include "MainForm.h"
#include "FlatTabControl.h"
#include "DataFieldDeselectFilter.h"
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

// Типы сообщения статуса (параметр kind в SetStatus).
static const int StatusNeutral = 0;  // серый
static const int StatusSuccess = 1;  // зелёный
static const int StatusError   = 2;  // красный

// Перевод кодов ошибок нативных модулей в локализованный текст. Сами модули
// языка UI не знают — текст выбирается здесь через Localization::T.
static String^ RegErrorText(RegError code, LSTATUS detail)
{
    switch (code) {
        case RegError::OpenKeyFailed:
            return Localization::T(L"error.openRegistryKey", (int)detail);
        default:
            return String::Empty;
    }
}

static String^ BackupErrorText(BackupError code)
{
    switch (code) {
        case BackupError::OpenFileFailed: return Localization::T(L"error.backupOpenFile");
        case BackupError::InvalidSize:    return Localization::T(L"error.backupInvalidSize");
        case BackupError::ReadFailed:     return Localization::T(L"error.backupReadFile");
        case BackupError::NoDeviceKey:    return Localization::T(L"error.backupNoDeviceKey");
        default:                          return String::Empty;
    }
}

// Допустимые символы имени оси/кнопки: латиница, цифры, пробел и ()-_/.
// Единый предикат — чтобы фильтр ввода (nameCell_KeyPress) и страховка при
// записи (SanitizeName) не разошлись.
static bool IsAllowedNameChar(wchar_t c)
{
    return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z')
        || (c >= L'0' && c <= L'9') || c == L' '
        || c == L'(' || c == L')' || c == L'-' || c == L'_' || c == L'/';
}

// Страховка на этапе записи: KeyPress блокирует ввод с клавиатуры, но вставку
// (Ctrl+V) и программную подстановку не ловит. Здесь отбрасываем недопустимые
// символы и режем длину до 32 — чтобы в реестр не попало ничего постороннего.
static const size_t kMaxNameLen = 32;
static std::wstring SanitizeName(const std::wstring& in)
{
    std::wstring out;
    for (wchar_t c : in) {
        if (out.size() >= kMaxNameLen) break;
        if (IsAllowedNameChar(c)) out.push_back(c);
    }
    return out;
}

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
// Подмена стандартного tabMain на FlatTabControl.
// InitializeComponent создаёт обычный TabControl (чтобы дизайнер открывался);
// здесь мы переносим в FlatTabControl все вкладки и ключевые свойства и
// ставим его на место исходного в дереве контролов.
// -----------------------------------------------------------------------
void MainForm::SwapTabControlToFlat()
{
    if (tabMain == nullptr) return;

    FlatTabControl^ flat = gcnew FlatTabControl();
    flat->Name          = tabMain->Name;
    flat->Dock          = tabMain->Dock;
    flat->Location      = tabMain->Location;
    flat->Size          = tabMain->Size;
    flat->TabIndex      = tabMain->TabIndex;
    flat->Font          = tabMain->Font;

    // Переносим страницы из старого TabControl в новый.
    int pageCount = tabMain->TabPages->Count;
    array<System::Windows::Forms::TabPage^>^ pages =
        gcnew array<System::Windows::Forms::TabPage^>(pageCount);
    for (int i = 0; i < pageCount; ++i)
        pages[i] = tabMain->TabPages[i];
    tabMain->TabPages->Clear();
    flat->TabPages->AddRange(pages);

    // Ставим flat на ту же позицию в коллекции контролов родителя.
    System::Windows::Forms::Control^ parent = tabMain->Parent;
    int idx = parent->Controls->GetChildIndex(tabMain);
    parent->Controls->Remove(tabMain);
    parent->Controls->Add(flat);
    parent->Controls->SetChildIndex(flat, idx);

    delete tabMain;
    tabMain = flat;

    if (flat->TabPages->Count > 0)
        flat->SelectedIndex = 0;

    // Переключатель языка должен оставаться поверх вкладок.
    comboBoxLanguage->BringToFront();
}

// -----------------------------------------------------------------------
// Поля сырых байт (textBoxRawData) и предпросмотра (textBoxPreviewData) —
// только для чтения и копирования. Они уже ReadOnly (печатать нельзя), но
// в них ставится мигающая каретка ввода. Здесь убираем каретку и I-beam,
// исключаем из Tab-обхода и вешаем контекстное меню «Копировать / Выделить всё».
// -----------------------------------------------------------------------
void MainForm::SetupReadOnlyDataFields()
{
    array<TextBox^>^ fields = gcnew array<TextBox^> { textBoxRawData, textBoxPreviewData };
    for each (TextBox^ tb in fields) {
        tb->TabStop = false;                                  // не получает фокус по Tab
        tb->Cursor  = System::Windows::Forms::Cursors::Arrow; // стрелка вместо I-beam

        // Контекстное меню: копирование выделенного/всего текста.
        System::Windows::Forms::ContextMenuStrip^ menu = gcnew System::Windows::Forms::ContextMenuStrip();
        ToolStripMenuItem^ copyItem = gcnew ToolStripMenuItem(Localization::T(L"ctx.copy"));
        copyItem->Click += gcnew System::EventHandler(this, &MainForm::dataFieldCopy_Click);
        ToolStripMenuItem^ selectAllItem = gcnew ToolStripMenuItem(Localization::T(L"ctx.selectAll"));
        selectAllItem->Click += gcnew System::EventHandler(this, &MainForm::dataFieldSelectAll_Click);
        menu->Items->Add(copyItem);
        menu->Items->Add(selectAllItem);
        tb->ContextMenuStrip = menu;

        // Скрываем каретку при получении фокуса и при кликах мышью.
        tb->GotFocus  += gcnew System::EventHandler(this, &MainForm::dataField_GotFocus);
        tb->MouseDown += gcnew System::Windows::Forms::MouseEventHandler(this, &MainForm::dataField_MouseDown);
        tb->MouseUp   += gcnew System::Windows::Forms::MouseEventHandler(this, &MainForm::dataField_MouseDown);
        // Сбрасываем выделение, когда фокус уходит из поля (клик вне поля).
        tb->Leave     += gcnew System::EventHandler(this, &MainForm::dataField_Leave);
    }

    // Глобальный фильтр: снимает выделение при клике мышью в любом месте окна
    // вне поля (метки, панели, пустое место — фокус там не меняется, поэтому
    // Leave не срабатывает). Application держит ссылку на фильтр сам.
    Application::AddMessageFilter(gcnew DataFieldDeselectFilter(fields));
}

void MainForm::dataField_GotFocus(System::Object^ sender, System::EventArgs^ e)
{
    TextBox^ tb = dynamic_cast<TextBox^>(sender);
    if (tb != nullptr && tb->IsHandleCreated)
        ::HideCaret(reinterpret_cast<HWND>(tb->Handle.ToPointer()));
}

void MainForm::dataField_MouseDown(System::Object^ sender, System::Windows::Forms::MouseEventArgs^ e)
{
    TextBox^ tb = dynamic_cast<TextBox^>(sender);
    if (tb != nullptr && tb->IsHandleCreated)
        ::HideCaret(reinterpret_cast<HWND>(tb->Handle.ToPointer()));
}

void MainForm::dataField_Leave(System::Object^ sender, System::EventArgs^ e)
{
    TextBox^ tb = dynamic_cast<TextBox^>(sender);
    if (tb != nullptr)
        tb->SelectionLength = 0;  // снимаем выделение при потере фокуса
}

void MainForm::dataFieldCopy_Click(System::Object^ sender, System::EventArgs^ e)
{
    ToolStripMenuItem^ item = dynamic_cast<ToolStripMenuItem^>(sender);
    System::Windows::Forms::ContextMenuStrip^ menu = (item != nullptr) ? dynamic_cast<System::Windows::Forms::ContextMenuStrip^>(item->Owner) : nullptr;
    TextBox^ tb = (menu != nullptr) ? dynamic_cast<TextBox^>(menu->SourceControl) : nullptr;
    if (tb == nullptr) return;

    String^ text = (tb->SelectionLength > 0) ? tb->SelectedText : tb->Text;
    if (!String::IsNullOrEmpty(text))
        Clipboard::SetText(text);
}

void MainForm::dataFieldSelectAll_Click(System::Object^ sender, System::EventArgs^ e)
{
    ToolStripMenuItem^ item = dynamic_cast<ToolStripMenuItem^>(sender);
    System::Windows::Forms::ContextMenuStrip^ menu = (item != nullptr) ? dynamic_cast<System::Windows::Forms::ContextMenuStrip^>(item->Owner) : nullptr;
    TextBox^ tb = (menu != nullptr) ? dynamic_cast<TextBox^>(menu->SourceControl) : nullptr;
    if (tb == nullptr) return;

    tb->SelectAll();
}

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
    checkOnlyConnected->ForeColor    = textOn;

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

    // ── Вкладки и редактор имён осей/кнопок ──────────────────────────────
    tabMain->BackColor       = bgDark;
    tabPageOem->BackColor    = bgDark;
    tabPageAxes->BackColor   = bgDark;
    layoutAxesTab->BackColor = bgDark;
    panelAxesButtons->BackColor = bgDark;

    labelAxesGridHeader->ForeColor    = textOn;
    labelButtonsGridHeader->ForeColor = textOn;

    array<DataGridView^>^ grids = gcnew array<DataGridView^> { dataGridAxes, dataGridButtons };
    for each (DataGridView^ g in grids) {
        g->BackgroundColor = bgPanel;
        g->GridColor       = border;
        g->ForeColor       = textOn;

        // Стиль заголовков столбцов
        DataGridViewCellStyle^ hdr = gcnew DataGridViewCellStyle();
        hdr->BackColor = Color::FromArgb(37, 37, 38);
        hdr->ForeColor = textOn;
        hdr->SelectionBackColor = hdr->BackColor;
        hdr->SelectionForeColor = textOn;
        hdr->Padding = System::Windows::Forms::Padding(6, 4, 6, 4);
        g->ColumnHeadersDefaultCellStyle = hdr;

        // Стиль ячеек: выделение — мягко-синее, чтобы не «слепить» в тёмной теме
        DataGridViewCellStyle^ cell = gcnew DataGridViewCellStyle();
        cell->BackColor = bgPanel;
        cell->ForeColor = textOn;
        cell->SelectionBackColor = Color::FromArgb(0, 99, 177);
        cell->SelectionForeColor = textOn;
        cell->Padding = System::Windows::Forms::Padding(6, 2, 6, 2);
        g->DefaultCellStyle = cell;

        // Валидация ввода имени: фильтр и ограничение длины вешаются на
        // встроенный редактор ячейки при входе в режим правки.
        g->EditingControlShowing += gcnew DataGridViewEditingControlShowingEventHandler(
            this, &MainForm::nameGrid_EditingControlShowing);
    }

    // Кнопки вкладки
    array<Button^>^ axesBtns = gcnew array<Button^> { buttonReloadNames, buttonApplyNames };
    for each (Button^ b in axesBtns) {
        b->BackColor = bgPanel;
        b->ForeColor = textOn;
        b->FlatStyle = FlatStyle::Flat;
        b->FlatAppearance->BorderColor        = border;
        b->FlatAppearance->MouseOverBackColor = Color::FromArgb(62, 62, 64);
        b->FlatAppearance->MouseDownBackColor = Color::FromArgb(0, 122, 204);
    }
    // «Применить имена» — акцентная синяя, как buttonApply
    buttonApplyNames->BackColor = Color::FromArgb(0, 99, 177);
    buttonApplyNames->FlatAppearance->MouseOverBackColor = Color::FromArgb(0, 122, 204);
    buttonApplyNames->FlatAppearance->MouseDownBackColor = Color::FromArgb(0, 80, 150);
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
    checkOnlyConnected->Text  = Localization::T(L"label.onlyConnected");
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

    // Контекстные меню полей сырых/предпросмотренных байт.
    array<TextBox^>^ dataFields = gcnew array<TextBox^> { textBoxRawData, textBoxPreviewData };
    for each (TextBox^ tb in dataFields) {
        System::Windows::Forms::ContextMenuStrip^ menu = tb->ContextMenuStrip;
        if (menu != nullptr && menu->Items->Count >= 2) {
            menu->Items[0]->Text = Localization::T(L"ctx.copy");
            menu->Items[1]->Text = Localization::T(L"ctx.selectAll");
        }
    }

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

    // Вкладки
    tabPageOem->Text  = Localization::T(L"tab.oemData");
    tabPageAxes->Text = Localization::T(L"tab.axesButtons");

    labelAxesGridHeader->Text    = Localization::T(L"axes.gridHeader");
    labelButtonsGridHeader->Text = Localization::T(L"buttons.gridHeader");

    buttonReloadNames->Text = Localization::T(L"axes.buttonReload");
    buttonApplyNames->Text  = Localization::T(L"axes.buttonApply");

    // Заголовки столбцов сеток.
    colAxesIndex->HeaderText    = Localization::T(L"axes.colIndex");
    colAxesName->HeaderText     = Localization::T(L"axes.colName");
    colButtonsIndex->HeaderText = Localization::T(L"axes.colIndex");
    colButtonsName->HeaderText  = Localization::T(L"axes.colName");
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
    _refreshing = true;

    try {
        auto devices = RegistryEngine::ScanDevices(checkOnlyConnected->Checked);

        for (const auto& d : devices) {
            String^ dn = gcnew String(d.displayName.c_str());
            String^ rk = gcnew String(d.registryKey.c_str());
            comboBoxDevice->Items->Add(gcnew DeviceInfo(dn, rk));
        }

        if (!devices.empty()) {
            SetControlsEnabled(true);
            comboBoxDevice->SelectedIndex = 0;
            // LoadDeviceData() вызовется автоматически через SelectedIndexChanged
            SetStatus(Localization::T(L"status.devicesConnected", (int)devices.size()), StatusSuccess);
        } else {
            textBoxOemName->Text = String::Empty;
            ClearFlagControls();
            SetControlsEnabled(false);
            SetStatus(Localization::T(L"status.noDevices"), StatusError);
        }
    }
    catch (System::Exception^ ex) {
        SetControlsEnabled(false);
        SetStatus(Localization::T(L"status.scanError", ex->Message), StatusError);
    }
    catch (...) {
        SetControlsEnabled(false);
        SetStatus(Localization::T(L"status.scanInternalError"), StatusError);
    }
    finally {
        _refreshing = false;
        comboBoxDevice->EndUpdate();
    }

    // Предложение создать стоковые параметры показываем ТОЛЬКО после полного
    // завершения обновления (вне BeginUpdate/EndUpdate и при _refreshing=false).
    // Иначе для единственного пустого устройства, которое выбирается программно
    // (SelectedIndex=0), диалог подавлялся бы вместе с программным выбором —
    // и пользователь так и не получал бы предложение создать параметры.
    if (OfferCreateStockData())
        LoadDeviceData();
}

void MainForm::buttonRefresh_Click(System::Object^ sender, System::EventArgs^ e)
{
    RefreshDeviceList();
}

// Переключение фильтра «только подключённые»: запоминаем выбор в settings.ini
// и перечитываем список (снятая галочка показывает все устройства из реестра —
// можно сделать бэкап устройства, которое сейчас не подключено).
void MainForm::checkOnlyConnected_CheckedChanged(System::Object^ sender, System::EventArgs^ e)
{
    Localization::SaveShowOnlyConnected(checkOnlyConnected->Checked);
    RefreshDeviceList();
}

// -----------------------------------------------------------------------
// Вывод сообщения в строку статуса с цветовой индикацией:
//   Success — зелёный (успешная операция, информация),
//   Error   — красный (ошибка, нет устройств, нечего записывать),
//   Neutral — серый (нейтральное состояние).
// -----------------------------------------------------------------------
void MainForm::SetStatus(System::String^ text, int kind)
{
    labelStatus->Text = text;
    switch (kind) {
    case StatusSuccess:
        labelStatus->ForeColor = System::Drawing::Color::FromArgb(80, 200, 120);
        break;
    case StatusError:
        labelStatus->ForeColor = System::Drawing::Color::FromArgb(235, 95, 95);
        break;
    default:
        labelStatus->ForeColor = System::Drawing::Color::FromArgb(160, 160, 160);
        break;
    }
}

// -----------------------------------------------------------------------
// Блокировка элементов редактирования, когда нет устройств для настройки.
// Кнопки «Обновить», «Папка бэкапов», выбор языка и сам список устройств
// остаются активными.
// -----------------------------------------------------------------------
void MainForm::SetControlsEnabled(bool enabled)
{
    textBoxOemName->Enabled      = enabled;
    textBoxDwNumButtons->Enabled = enabled;
    groupBoxFlags->Enabled       = enabled;
    buttonApply->Enabled         = enabled;
    buttonBackup->Enabled        = enabled;
    buttonRestore->Enabled       = enabled;
    buttonApplyNames->Enabled    = enabled;
    buttonReloadNames->Enabled   = enabled;
    dataGridAxes->Enabled        = enabled;
    dataGridButtons->Enabled     = enabled;
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

    // Сетки имён осей/кнопок — тоже чистим.
    dataGridAxes->Rows->Clear();
    dataGridButtons->Rows->Clear();
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

    if (data.errorCode != RegError::None) {
        SetStatus(RegErrorText(data.errorCode, data.errorDetail), StatusError);
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

    // Заполняем вкладку «Оси и кнопки» содержимым подключей Axes\<N>\@ и
    // Buttons\<N>\@. Если для устройства этих подключей нет (бывает у XInput
    // и Bluetooth-геймпадов) — сетки просто останутся пустыми.
    PopulateNamesGrids(data);
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
    // Предложение создать стоковые параметры показываем только при ЯВНОМ выборе
    // устройства пользователем — не внутри LoadDeviceData, который вызывается
    // также при авто-обновлении по WM_DEVICECHANGE (иначе модальное окно
    // выскакивало бы само при подключении/отключении устройств).
    OfferCreateStockData();
    LoadDeviceData();
}

// -----------------------------------------------------------------------
// Создание стоковых OEMName/OEMData для «пустого» устройства.
// У части рулей (например, Ardor) эти параметры отсутствуют в реестре из
// коробки, и без них блок настроек недоступен. По согласию пользователя
// записываем дефолтную конфигурацию, которую дальше можно отредактировать.
// -----------------------------------------------------------------------
bool MainForm::OfferCreateStockData()
{
    // Подавляем во время программного обновления списка (см. _refreshing).
    if (_refreshing) return false;

    DeviceInfo^ sel = dynamic_cast<DeviceInfo^>(comboBoxDevice->SelectedItem);
    if (sel == nullptr) return false;

    // Не предлагаем повторно тем устройствам, по которым уже был отказ в сессии.
    if (_declinedCreate->Contains(sel->RegistryKey)) return false;

    msclr::interop::marshal_context ctx;
    std::wstring oemKey = ctx.marshal_as<std::wstring>(sel->RegistryKey);

    DeviceData current = RegistryEngine::ReadDeviceData(oemKey);
    // Если ключ не открылся (ошибка) или OEMData уже есть — предлагать нечего.
    if (current.errorCode != RegError::None || current.hasOemData) return false;

    if (MessageBox::Show(this,
            Localization::T(L"create.offerText"),
            Localization::T(L"create.offerTitle"),
            MessageBoxButtons::OKCancel, MessageBoxIcon::Question)
        != System::Windows::Forms::DialogResult::OK)
    {
        _declinedCreate->Add(sel->RegistryKey);
        return false;
    }

    // Автобэкап перед записью — поддерево устройства (даже почти пустое).
    std::wstring backupPath = BackupManager::WriteBackup(oemKey);
    if (backupPath.empty()) {
        SetStatus(Localization::T(L"create.backupFailed"), StatusError);
        return false;
    }

    // Стоковый OEMData (8 байт, как у современных HID):
    //   dwFlags = HASZ | HASPOV | HASR | HASU | HASV = 0x01880003 (обычный джойстик)
    //   dwNumButtons = 32
    DWORD dwFlags = JoyHws::HASZ | JoyHws::HASPOV
                  | JoyHws::HASR | JoyHws::HASU | JoyHws::HASV;
    DWORD dwNumButtons = 32;

    std::vector<BYTE> stock(8);
    stock[0] = (BYTE)( dwFlags        & 0xFF);
    stock[1] = (BYTE)((dwFlags >>  8) & 0xFF);
    stock[2] = (BYTE)((dwFlags >> 16) & 0xFF);
    stock[3] = (BYTE)((dwFlags >> 24) & 0xFF);
    stock[4] = (BYTE)( dwNumButtons        & 0xFF);
    stock[5] = (BYTE)((dwNumButtons >>  8) & 0xFF);
    stock[6] = (BYTE)((dwNumButtons >> 16) & 0xFF);
    stock[7] = (BYTE)((dwNumButtons >> 24) & 0xFF);

    LSTATUS st = RegistryEngine::WriteDeviceData(oemKey, stock);
    if (st != ERROR_SUCCESS) {
        SetStatus(Localization::T(L"create.writeDataError", st), StatusError);
        return false;
    }

    // OEMName записываем, только если его ещё нет — чтобы не затирать имя,
    // если у устройства оно вдруг есть, а OEMData нет.
    if (current.oemName.empty()) {
        st = RegistryEngine::WriteOemName(oemKey, L"Controller");
        if (st != ERROR_SUCCESS) {
            SetStatus(Localization::T(L"create.writeNameError", st), StatusError);
            return true;
        }
    }

    SetStatus(Localization::T(L"create.success", gcnew String(backupPath.c_str())),
              StatusSuccess);
    return true;
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

// Вход в режим правки ячейки имени: ограничиваем длину и навешиваем фильтр
// символов на встроенный TextBox. Редактор переиспользуется между ячейками,
// поэтому сначала снимаем свой обработчик (иначе он накопится при повторах).
void MainForm::nameGrid_EditingControlShowing(System::Object^ sender,
    System::Windows::Forms::DataGridViewEditingControlShowingEventArgs^ e)
{
    TextBox^ tb = dynamic_cast<TextBox^>(e->Control);
    if (tb == nullptr) return;

    tb->MaxLength = 32;
    tb->KeyPress -= gcnew KeyPressEventHandler(this, &MainForm::nameCell_KeyPress);
    tb->KeyPress += gcnew KeyPressEventHandler(this, &MainForm::nameCell_KeyPress);
}

// Фильтр символов имени оси/кнопки: латиница, цифры, пробел и ()-_/.
// Длину ограничивает MaxLength=32 встроенного редактора.
void MainForm::nameCell_KeyPress(System::Object^ sender, System::Windows::Forms::KeyPressEventArgs^ e)
{
    if (e->KeyChar == '\b') return;
    if (!IsAllowedNameChar(e->KeyChar)) e->Handled = true;
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
    if (sel == nullptr) { SetStatus(Localization::T(L"status.noSelection"), StatusError); return; }
    msclr::interop::marshal_context ctx;
    std::wstring oemKey = ctx.marshal_as<std::wstring>(sel->RegistryKey);
    if (oemKey.empty()) {
        SetStatus(Localization::T(L"status.noSelection"), StatusError);
        return;
    }

    DeviceData data = RegistryEngine::ReadDeviceData(oemKey);
    if (!data.hasOemData) {
        SetStatus(Localization::T(L"status.oemDataMissingBackup"), StatusError);
        return;
    }

    // WriteBackup сам читает свежее поддерево HKCU\...\OEM\<oemKey> из
    // реестра — кэшированные data.oemName/data.oemDataRaw уже не нужны.
    std::wstring backupPath = BackupManager::WriteBackup(oemKey);
    if (backupPath.empty()) {
        SetStatus(Localization::T(L"status.backupFailed"), StatusError);
        return;
    }

    SetStatus(Localization::T(L"status.backupSaved", gcnew String(backupPath.c_str())), StatusSuccess);
}

// -----------------------------------------------------------------------
// Кнопка «Применить» — автобэкап, затем запись изменений в реестр
// -----------------------------------------------------------------------
void MainForm::buttonApply_Click(System::Object^ sender, System::EventArgs^ e)
{
    DeviceInfo^ sel = dynamic_cast<DeviceInfo^>(comboBoxDevice->SelectedItem);
    if (sel == nullptr) { SetStatus(Localization::T(L"status.noSelection"), StatusError); return; }
    msclr::interop::marshal_context ctxKey;
    std::wstring oemKey = ctxKey.marshal_as<std::wstring>(sel->RegistryKey);

    // Читаем текущее состояние реестра для бэкапа (до изменений).
    DeviceData current = RegistryEngine::ReadDeviceData(oemKey);
    if (!current.hasOemData) {
        SetStatus(Localization::T(L"status.oemDataMissingApply"), StatusError);
        return;
    }

    // Автоматический тихий бэкап перед любой записью — полное поддерево HKCU.
    std::wstring backupPath = BackupManager::WriteBackup(oemKey);
    if (backupPath.empty()) {
        SetStatus(Localization::T(L"status.backupBeforeApplyFailed"), StatusError);
        return;
    }

    // Валидация: поле количества кнопок не должно быть пустым.
    if (textBoxDwNumButtons->Text->Trim()->Length == 0) {
        SetStatus(Localization::T(L"status.numButtonsEmpty"), StatusError);
        return;
    }

    // Валидация: OEMName не может состоять только из пробелов.
    String^ rawName = textBoxOemName->Text;
    if (rawName->Length > 0 && rawName->Trim()->Length == 0) {
        SetStatus(Localization::T(L"status.oemNameOnlySpaces"), StatusError);
        return;
    }

    // Собираем новые байты из предпросмотра (они уже отражают текущий UI).
    if (textBoxPreviewData->Text->Length == 0) {
        SetStatus(Localization::T(L"status.noDataToWrite"), StatusError);
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
            SetStatus(Localization::T(L"status.previewParseError"), StatusError);
            return;
        }
    }

    if (newBytes.empty()) {
        SetStatus(Localization::T(L"status.noDataToWrite"), StatusError);
        return;
    }

    // Записываем OEMData.
    LSTATUS st = RegistryEngine::WriteDeviceData(oemKey, newBytes);
    if (st != ERROR_SUCCESS) {
        SetStatus(Localization::T(L"status.writeOemDataError", st), StatusError);
        return;
    }

    // Записываем OEMName, если поле непустое (rawName проверен выше).
    String^ newName = rawName->Trim();
    if (newName->Length > 0) {
        msclr::interop::marshal_context ctxName;
        std::wstring wName = ctxName.marshal_as<std::wstring>(newName);
        st = RegistryEngine::WriteOemName(oemKey, wName);
        if (st != ERROR_SUCCESS) {
            SetStatus(Localization::T(L"status.writeOemNameAfterDataError", st), StatusError);
            return;
        }
    }

    // Перечитываем устройство, чтобы снимок совпал с тем, что теперь в реестре.
    LoadDeviceData();

    SetStatus(Localization::T(L"status.applied", gcnew String(backupPath.c_str())), StatusSuccess);
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------
// Кнопка «Папка бэкапов» — открывает папку в Проводнике
// -----------------------------------------------------------------------
void MainForm::buttonOpenBackups_Click(System::Object^ sender, System::EventArgs^ e)
{
    std::wstring dir = BackupManager::EnsureBackupDir();
    if (dir.empty()) {
        SetStatus(Localization::T(L"status.backupDirFailed"), StatusError);
        return;
    }
    // ShellExecute открывает папку в Проводнике без запроса UAC.
    HINSTANCE result = ShellExecuteW(nullptr, L"explore", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
        SetStatus(Localization::T(L"status.openBackupsFailed"), StatusError);
}

// -----------------------------------------------------------------------
// Кнопка «Восстановить» — выбор .reg-бэкапа и запись его в реестр.
// Перед записью делается автобэкап текущего состояния.
// -----------------------------------------------------------------------
void MainForm::buttonRestore_Click(System::Object^ sender, System::EventArgs^ e)
{
    DeviceInfo^ sel = dynamic_cast<DeviceInfo^>(comboBoxDevice->SelectedItem);
    if (sel == nullptr) {
        SetStatus(Localization::T(L"status.noSelection"), StatusError);
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
        SetStatus(Localization::T(L"restore.parseError",
            BackupErrorText(parsed.errorCode)), StatusError);
        return;
    }

    // Семантика восстановления — точный снимок: значение есть в бэкапе → пишем,
    // значения нет → УДАЛЯЕМ его из устройства. Пустой бэкап (нет ни OEMData,
    // ни OEMName) — это валидный откат к заводскому «чистому» состоянию
    // (например, отмена ранее созданных стоковых параметров).

    // Санити-проверка длины OEMData: корректны только 8 байт (hws у современных
    // HID) или 112 (полная JOYREGHWCONFIG). Повреждённый при ручной правке hex
    // обычно даёт другую длину (ParseHexBlob обрезает блоб на «битом» символе),
    // поэтому эта проверка заодно ловит несуществующие hex-значения.
    if (!parsed.oemDataRaw.empty()
        && parsed.oemDataRaw.size() != 8
        && parsed.oemDataRaw.size() != sizeof(JOYREGHWCONFIG)) {
        SetStatus(Localization::T(L"restore.invalidLength",
            (int)parsed.oemDataRaw.size()), StatusError);
        return;
    }

    // Собираем hex-строку байт OEMData из бэкапа для показа в диалоге.
    System::Text::StringBuilder^ hexBuilder = gcnew System::Text::StringBuilder((int)parsed.oemDataRaw.size() * 3);
    for (size_t i = 0; i < parsed.oemDataRaw.size(); ++i) {
        if (i > 0) hexBuilder->Append(L' ');
        hexBuilder->AppendFormat(L"{0:X2}", parsed.oemDataRaw[i]);
    }

    // #2: предупреждение о кросс-девайсе — бэкап снят с другого устройства.
    // Сравниваем ключ из заголовка .reg с выбранным (без учёта регистра).
    String^ warning = String::Empty;
    if (!parsed.deviceKey.empty()) {
        String^ fileKey = gcnew String(parsed.deviceKey.c_str());
        if (!String::Equals(fileKey, sel->RegistryKey,
                StringComparison::OrdinalIgnoreCase)) {
            warning = Localization::T(L"restore.crossDeviceWarning",
                fileKey, sel->RegistryKey);
        }
    }

    // Уведомление об удалении: какие значения отсутствуют в бэкапе и потому
    // будут удалены из устройства (откат к «пустому» состоянию).
    String^ deleteNotice = String::Empty;
    if (parsed.oemDataRaw.empty())
        deleteNotice += Localization::T(L"restore.willDeleteData");
    if (parsed.oemName.empty())
        deleteNotice += Localization::T(L"restore.willDeleteName");

    // Подтверждение: восстановление перезапишет/удалит текущие настройки.
    String^ msg = warning + deleteNotice + Localization::T(L"restore.confirmText",
        gcnew String(parsed.oemName.c_str()),
        hexBuilder->ToString());

    if (MessageBox::Show(this, msg, Localization::T(L"restore.confirmTitle"),
            MessageBoxButtons::OKCancel,
            warning->Length > 0 ? MessageBoxIcon::Warning : MessageBoxIcon::Question)
        != System::Windows::Forms::DialogResult::OK)
        return;

    // OEMData: есть в бэкапе → пишем (длина проверена выше); нет → удаляем.
    LSTATUS st = parsed.oemDataRaw.empty()
        ? RegistryEngine::DeleteOemData(oemKey)
        : RegistryEngine::WriteDeviceData(oemKey, parsed.oemDataRaw);
    if (st != ERROR_SUCCESS) {
        SetStatus(Localization::T(L"status.writeOemDataError", st), StatusError);
        return;
    }

    // OEMName: есть в бэкапе → пишем; нет → удаляем.
    st = parsed.oemName.empty()
        ? RegistryEngine::DeleteOemName(oemKey)
        : RegistryEngine::WriteOemName(oemKey, parsed.oemName);
    if (st != ERROR_SUCCESS) {
        SetStatus(Localization::T(L"restore.writeOemNameError", st), StatusError);
        return;
    }

    // Имена осей/кнопок — точный снимок: пишем ровно то, что в бэкапе, включая
    // пустые имена (так восстановление работает и как сброс имени). Best-effort:
    // WriteAxisName/WriteButtonName не создают отсутствующие подключи, поэтому
    // имена осей/кнопок, которых нет у выбранного устройства, тихо пропускаются
    // — основной результат (OEMData/OEMName) уже зафиксирован выше.
    for (const auto& a : parsed.axes)
        RegistryEngine::WriteAxisName(oemKey, a.index, a.name);
    for (const auto& b : parsed.buttons)
        RegistryEngine::WriteButtonName(oemKey, b.index, b.name);

    LoadDeviceData();
    SetStatus(Localization::T(L"restore.success", dlg->SafeFileName), StatusSuccess);
}

// -----------------------------------------------------------------------
// Вкладка «Оси и кнопки» — заполнение сеток из DeviceData
// -----------------------------------------------------------------------
void MainForm::PopulateNamesGrids(const DeviceData& data)
{
    dataGridAxes->Rows->Clear();
    dataGridButtons->Rows->Clear();

    for (const auto& e : data.axes) {
        int row = dataGridAxes->Rows->Add();
        // Колонку Index сохраняем как boxed int — потом тривиально извлечь
        // через Convert::ToInt32, при этом ReadOnly защищает от правки.
        dataGridAxes->Rows[row]->Cells[0]->Value = e.index;
        dataGridAxes->Rows[row]->Cells[1]->Value = gcnew String(e.name.c_str());
    }

    for (const auto& e : data.buttons) {
        int row = dataGridButtons->Rows->Add();
        dataGridButtons->Rows[row]->Cells[0]->Value = e.index;
        dataGridButtons->Rows[row]->Cells[1]->Value = gcnew String(e.name.c_str());
    }
}

// Чтение состояния сетки в std::vector<NamedEntry>.
// Возвращает только записи с непустым именем — пустые поля интерпретируем
// как «не трогать». Возвращаемый тип — managed для удобства передачи между
// двумя обработчиками, но сам обход — нативный.
static std::vector<NamedEntry> ReadGridEntries(System::Windows::Forms::DataGridView^ grid)
{
    std::vector<NamedEntry> out;
    msclr::interop::marshal_context ctx;
    for (int i = 0; i < grid->Rows->Count; ++i) {
        System::Object^ idxBox = grid->Rows[i]->Cells[0]->Value;
        System::Object^ nameBox = grid->Rows[i]->Cells[1]->Value;
        if (idxBox == nullptr) continue;

        NamedEntry e;
        e.index = System::Convert::ToInt32(idxBox);
        if (nameBox != nullptr)
            e.name = SanitizeName(ctx.marshal_as<std::wstring>(nameBox->ToString()));
        out.push_back(std::move(e));
    }
    return out;
}

// -----------------------------------------------------------------------
// Кнопка «Перечитать» — обновить сетки из реестра без изменений UI основной вкладки
// -----------------------------------------------------------------------
void MainForm::buttonReloadNames_Click(System::Object^ sender, System::EventArgs^ e)
{
    // Полная перезагрузка устройства проще и безопаснее: гарантирует, что
    // отображение OEMData и Axes/Buttons согласованы.
    LoadDeviceData();
}

// -----------------------------------------------------------------------
// Кнопка «Применить имена» — автобэкап + запись Axes\<N>\@ и Buttons\<N>\@
// -----------------------------------------------------------------------
void MainForm::buttonApplyNames_Click(System::Object^ sender, System::EventArgs^ e)
{
    DeviceInfo^ sel = dynamic_cast<DeviceInfo^>(comboBoxDevice->SelectedItem);
    if (sel == nullptr) {
        SetStatus(Localization::T(L"status.noSelection"), StatusError);
        return;
    }
    msclr::interop::marshal_context ctxKey;
    std::wstring oemKey = ctxKey.marshal_as<std::wstring>(sel->RegistryKey);

    auto axes    = ReadGridEntries(dataGridAxes);
    auto buttons = ReadGridEntries(dataGridButtons);
    if (axes.empty() && buttons.empty()) {
        SetStatus(Localization::T(L"status.namesNoData"), StatusError);
        return;
    }

    // Автобэкап перед записью — полный экспорт поддерева.
    std::wstring backupPath = BackupManager::WriteBackup(oemKey);
    if (backupPath.empty()) {
        SetStatus(Localization::T(L"status.backupBeforeApplyFailed"), StatusError);
        return;
    }

    // Считаем сколько успешно записано / сколько провалилось.
    int written = 0;
    int failed  = 0;
    LSTATUS lastError = ERROR_SUCCESS;

    for (const auto& a : axes) {
        LSTATUS s = RegistryEngine::WriteAxisName(oemKey, a.index, a.name);
        if (s == ERROR_SUCCESS) ++written;
        else                    { ++failed; lastError = s; }
    }
    for (const auto& b : buttons) {
        LSTATUS s = RegistryEngine::WriteButtonName(oemKey, b.index, b.name);
        if (s == ERROR_SUCCESS) ++written;
        else                    { ++failed; lastError = s; }
    }

    if (failed > 0 && written == 0) {
        SetStatus(Localization::T(L"status.namesWriteError",
            (int)lastError), StatusError);
        return;
    }
    if (failed > 0) {
        // Частичный успех: говорим сколько записано и сколько провалено.
        SetStatus(Localization::T(L"status.namesAppliedPartial",
            written, failed), StatusError);
        return;
    }
    SetStatus(Localization::T(L"status.namesApplied", written), StatusSuccess);
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
