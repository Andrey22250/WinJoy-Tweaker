#pragma once

#include "locales/Localization.h"  // Используется в конструкторе и ApplyLocalization

// Forward-declare native-структуры из RegistryEngine.h, чтобы не тащить весь
// заголовок (включая Windows.h/dinput.h) в этот хедер.
struct DeviceData;

namespace WinJoytweaker {

    using namespace System;
    using namespace System::ComponentModel;
    using namespace System::Collections;
    using namespace System::Windows::Forms;
    using namespace System::Data;
    using namespace System::Drawing;

    /// <summary>
    /// Главное окно WinJoy Tweaker
    /// </summary>
    public ref class MainForm : public System::Windows::Forms::Form
    {
    public:
        MainForm(void)
        {
            InitializeComponent();

            _declinedCreate = gcnew System::Collections::Generic::List<System::String^>();

            // Подменяем стандартный TabControl на FlatTabControl (скрытие кромок).
            // Делается в рантайме, чтобы кастомный тип не попадал в файл формы
            // и не ломал дизайнер C++/CLI.
            SwapTabControlToFlat();

            // Поля сырых/предпросмотренных байт — только просмотр и копирование:
            // прячем каретку, убираем из Tab-обхода, вешаем контекстное меню.
            SetupReadOnlyDataFields();

            // Заполняем переключатель языка и выставляем сохранённое значение
            // ДО подписки на handler, чтобы инициализация не триггерила его.
            comboBoxLanguage->Items->Add(L"Auto / Авто");
            comboBoxLanguage->Items->Add(L"Русский");
            comboBoxLanguage->Items->Add(L"English");
            comboBoxLanguage->SelectedIndex = (int)Localization::LoadPreference();
            comboBoxLanguage->SelectedIndexChanged += gcnew System::EventHandler(
                this, &MainForm::comboBoxLanguage_SelectedIndexChanged);

            ApplyLocalization();
            ApplyDarkTheme();
            RefreshDeviceList();
        }

    protected:
        ~MainForm()
        {
            if (components)
                delete components;
        }

    private: System::ComponentModel::IContainer^ components;
    protected:

    private:

        // ── Верхняя часть формы ──────────────────────────────────────────
        System::Windows::Forms::TableLayoutPanel^  rootLayout;
        System::Windows::Forms::Label^             labelDevice;
        System::Windows::Forms::ComboBox^          comboBoxDevice;
        System::Windows::Forms::Button^            buttonRefresh;
        System::Windows::Forms::Label^             labelOemNameCaption;
        System::Windows::Forms::TextBox^           textBoxOemName;
        System::Windows::Forms::Label^             labelOemDataCaption;

        // ── Информационные поля (read-only) ──────────────────────────────
        System::Windows::Forms::Label^             labelDwNumButtons;
        System::Windows::Forms::TextBox^           textBoxDwNumButtons;
        // Отладочный hex-dump сырых байт OEMData (под GroupBox'ом)
        System::Windows::Forms::Label^             labelRawData;
        System::Windows::Forms::TextBox^           textBoxRawData;

        // ── GroupBox «Параметры (hws.dwFlags)»: тип устройства + оси ─────
        System::Windows::Forms::GroupBox^          groupBoxFlags;
        System::Windows::Forms::TableLayoutPanel^  innerLayoutFlags;
        System::Windows::Forms::Label^             labelDeviceTypeHeader;
        System::Windows::Forms::RadioButton^       radioGeneric;
        System::Windows::Forms::RadioButton^       radioYoke;
        System::Windows::Forms::RadioButton^       radioGamepad;
        System::Windows::Forms::RadioButton^       radioWheel;
        System::Windows::Forms::Label^             labelAxesHeader;
        System::Windows::Forms::CheckBox^          checkHasZ;
        System::Windows::Forms::CheckBox^          checkHasPov;
        System::Windows::Forms::CheckBox^          checkPovIsButtonCombos;
        System::Windows::Forms::CheckBox^          checkPovIsPoll;
        System::Windows::Forms::CheckBox^          checkHasR;
        System::Windows::Forms::CheckBox^          checkHasU;
        System::Windows::Forms::CheckBox^          checkHasV;

        // ── Маппинг оси X (в FlowLayoutPanel, изолированная группа) ─────
        System::Windows::Forms::Label^             labelXAxisHeader;
        System::Windows::Forms::FlowLayoutPanel^   panelXAxis;
        System::Windows::Forms::RadioButton^       radioXDefault;  // J1 X (по умолч.)
        System::Windows::Forms::RadioButton^       radioXJ1Y;      // XISJ1Y
        System::Windows::Forms::RadioButton^       radioXJ2X;      // XISJ2X
        System::Windows::Forms::RadioButton^       radioXJ2Y;      // XISJ2Y

        // ── Маппинг оси Y (в FlowLayoutPanel, изолированная группа) ─────
        System::Windows::Forms::Label^             labelYAxisHeader;
        System::Windows::Forms::FlowLayoutPanel^   panelYAxis;
        System::Windows::Forms::RadioButton^       radioYDefault;  // J1 Y (по умолч.)
        System::Windows::Forms::RadioButton^       radioYJ1X;      // YISJ1X
        System::Windows::Forms::RadioButton^       radioYJ2X;      // YISJ2X
        System::Windows::Forms::RadioButton^       radioYJ2Y;      // YISJ2Y

        // Отладочное поле: предпросмотр байт после применения текущих настроек UI
        System::Windows::Forms::Label^             labelPreviewData;
        System::Windows::Forms::TextBox^           textBoxPreviewData;

        System::Windows::Forms::Label^             labelStatus;
        System::Windows::Forms::Timer^             refreshDebounceTimer;

        // ── Кнопки действий (Применить / Бэкап) ─────────────────────────
        System::Windows::Forms::TableLayoutPanel^  panelButtons;
        System::Windows::Forms::Button^            buttonApply;
        System::Windows::Forms::Button^            buttonBackup;
        System::Windows::Forms::Button^            buttonOpenBackups;
        System::Windows::Forms::Button^            buttonRestore;

        // Переключатель языка интерфейса (Auto / Русский / English).
        System::Windows::Forms::ComboBox^          comboBoxLanguage;

        // ── Вкладки ───────────────────────────────────────────────────────
        // Корневой контейнер формы — TabControl с двумя страницами:
        //   tabPageOem   — параметры OEMData (всё прежнее содержимое rootLayout)
        //   tabPageAxes  — редактор имён осей и кнопок (Axes\<N>\@, Buttons\<N>\@)
        // Стандартный TabControl для дизайнера; в рантайме конструктор
        // подменяет его на FlatTabControl (SwapTabControlToFlat) ради скрытия
        // кромок. Кастомный тип НЕ упоминается в этом файле — иначе дизайнер
        // C++/CLI не открывает форму.
        System::Windows::Forms::TabControl^         tabMain;
        System::Windows::Forms::TabPage^            tabPageOem;
        System::Windows::Forms::TabPage^            tabPageAxes;

        // ── Вкладка «Оси и кнопки» ────────────────────────────────────────
        System::Windows::Forms::TableLayoutPanel^   layoutAxesTab;
        System::Windows::Forms::Label^              labelAxesGridHeader;
        System::Windows::Forms::Label^              labelButtonsGridHeader;
        System::Windows::Forms::DataGridView^                dataGridAxes;
        System::Windows::Forms::DataGridView^                dataGridButtons;
        System::Windows::Forms::DataGridViewTextBoxColumn^   colAxesIndex;
        System::Windows::Forms::DataGridViewTextBoxColumn^   colAxesName;
        System::Windows::Forms::DataGridViewTextBoxColumn^   colButtonsIndex;
        System::Windows::Forms::DataGridViewTextBoxColumn^   colButtonsName;
        System::Windows::Forms::TableLayoutPanel^   panelAxesButtons;  // строка с двумя кнопками
        System::Windows::Forms::Button^             buttonReloadNames;
        System::Windows::Forms::Button^             buttonApplyNames;

        // Снимок оригинальных байт OEMData — основа для предпросмотра изменений.
        array<System::Byte>^                       _oemDataSnapshot;

        // Ключи устройств, для которых пользователь уже отказался создавать
        // стоковые параметры — чтобы не предлагать повторно в рамках сессии.
        System::Collections::Generic::List<System::String^>^ _declinedCreate;

        // true во время RefreshDeviceList: программная установка SelectedIndex
        // синхронно дёргает SelectedIndexChanged, и без этого флага диалог
        // создания параметров выскакивал бы при каждом авто-обновлении списка.
        bool _refreshing;

        // Tooltip для подсказок о правилах ввода (OEMName и др.).
        System::Windows::Forms::ToolTip^            toolTipOemName;

        void ApplyDarkTheme();
        void ApplyLocalization();
        // Заменяет tabMain экземпляром FlatTabControl, перенося в него вкладки.
        void SwapTabControlToFlat();
        // Настраивает поля сырых/предпросмотренных байт как «только чтение и
        // копирование»: скрытая каретка, без Tab-фокуса, контекстное меню.
        void SetupReadOnlyDataFields();
        void dataField_GotFocus(System::Object^ sender, System::EventArgs^ e);
        void dataField_MouseDown(System::Object^ sender, System::Windows::Forms::MouseEventArgs^ e);
        void dataField_Leave(System::Object^ sender, System::EventArgs^ e);
        void dataFieldCopy_Click(System::Object^ sender, System::EventArgs^ e);
        void dataFieldSelectAll_Click(System::Object^ sender, System::EventArgs^ e);
        void RefreshDeviceList();
        void LoadDeviceData();
        // Если у выбранного устройства нет OEMData, предлагает создать стоковые
        // параметры. Возвращает true, если что-то было записано.
        bool OfferCreateStockData();
        void ClearFlagControls();

        // kind: 0 = нейтральный (серый), 1 = успех (зелёный), 2 = ошибка (красный).
        // Используются константы Status* из MainForm.cpp.
        void SetStatus(System::String^ text, int kind);
        // Блокировка/разблокировка элементов редактирования (нет устройств → false).
        void SetControlsEnabled(bool enabled);
        void UpdatePreviewData();
        void flagsControl_Changed(System::Object^ sender, System::EventArgs^ e);
        void checkHasPov_CheckedChanged(System::Object^ sender, System::EventArgs^ e);
        void textBoxDwNumButtons_KeyPress(System::Object^ sender, System::Windows::Forms::KeyPressEventArgs^ e);
        void textBoxOemName_KeyPress(System::Object^ sender, System::Windows::Forms::KeyPressEventArgs^ e);
        void buttonApply_Click(System::Object^ sender, System::EventArgs^ e);
        void buttonBackup_Click(System::Object^ sender, System::EventArgs^ e);
        void buttonOpenBackups_Click(System::Object^ sender, System::EventArgs^ e);
        void buttonRestore_Click(System::Object^ sender, System::EventArgs^ e);
        void buttonRefresh_Click(System::Object^ sender, System::EventArgs^ e);
        void comboBoxDevice_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e);
        void comboBoxLanguage_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e);
        void refreshDebounceTimer_Tick(System::Object^ sender, System::EventArgs^ e);

        // Вкладка «Оси и кнопки».
        void PopulateNamesGrids(const DeviceData& data);
        void buttonApplyNames_Click(System::Object^ sender, System::EventArgs^ e);
        void buttonReloadNames_Click(System::Object^ sender, System::EventArgs^ e);

    protected:
        // Перехват WM_DEVICECHANGE: подключение/отключение USB-HID устройства.
        // DBT_DEVNODES_CHANGED приходит top-level окнам без RegisterDeviceNotification.
        virtual void WndProc(System::Windows::Forms::Message% m) override
        {
            const int WM_DEVICECHANGE      = 0x0219;
            const int DBT_DEVNODES_CHANGED = 0x0007;

            if (m.Msg == WM_DEVICECHANGE &&
                m.WParam.ToInt32() == DBT_DEVNODES_CHANGED)
            {
                refreshDebounceTimer->Stop();
                refreshDebounceTimer->Start();
            }

            System::Windows::Forms::Form::WndProc(m);
        }

    private:

#pragma region Windows Form Designer generated code
        void InitializeComponent(void)
        {
            this->components = (gcnew System::ComponentModel::Container());
            this->rootLayout = (gcnew System::Windows::Forms::TableLayoutPanel());
            this->labelDevice = (gcnew System::Windows::Forms::Label());
            this->comboBoxDevice = (gcnew System::Windows::Forms::ComboBox());
            this->buttonRefresh = (gcnew System::Windows::Forms::Button());
            this->labelOemNameCaption = (gcnew System::Windows::Forms::Label());
            this->textBoxOemName = (gcnew System::Windows::Forms::TextBox());
            this->labelOemDataCaption = (gcnew System::Windows::Forms::Label());
            this->labelDwNumButtons = (gcnew System::Windows::Forms::Label());
            this->textBoxDwNumButtons = (gcnew System::Windows::Forms::TextBox());
            this->groupBoxFlags = (gcnew System::Windows::Forms::GroupBox());
            this->innerLayoutFlags = (gcnew System::Windows::Forms::TableLayoutPanel());
            this->labelDeviceTypeHeader = (gcnew System::Windows::Forms::Label());
            this->labelAxesHeader = (gcnew System::Windows::Forms::Label());
            this->labelXAxisHeader = (gcnew System::Windows::Forms::Label());
            this->labelYAxisHeader = (gcnew System::Windows::Forms::Label());
            this->radioGeneric = (gcnew System::Windows::Forms::RadioButton());
            this->checkHasZ = (gcnew System::Windows::Forms::CheckBox());
            this->radioYoke = (gcnew System::Windows::Forms::RadioButton());
            this->checkHasR = (gcnew System::Windows::Forms::CheckBox());
            this->radioGamepad = (gcnew System::Windows::Forms::RadioButton());
            this->checkHasU = (gcnew System::Windows::Forms::CheckBox());
            this->radioWheel = (gcnew System::Windows::Forms::RadioButton());
            this->checkHasV = (gcnew System::Windows::Forms::CheckBox());
            this->checkHasPov = (gcnew System::Windows::Forms::CheckBox());
            this->checkPovIsButtonCombos = (gcnew System::Windows::Forms::CheckBox());
            this->checkPovIsPoll = (gcnew System::Windows::Forms::CheckBox());
            this->panelXAxis = (gcnew System::Windows::Forms::FlowLayoutPanel());
            this->radioXDefault = (gcnew System::Windows::Forms::RadioButton());
            this->radioXJ1Y = (gcnew System::Windows::Forms::RadioButton());
            this->radioXJ2X = (gcnew System::Windows::Forms::RadioButton());
            this->radioXJ2Y = (gcnew System::Windows::Forms::RadioButton());
            this->panelYAxis = (gcnew System::Windows::Forms::FlowLayoutPanel());
            this->radioYDefault = (gcnew System::Windows::Forms::RadioButton());
            this->radioYJ1X = (gcnew System::Windows::Forms::RadioButton());
            this->radioYJ2X = (gcnew System::Windows::Forms::RadioButton());
            this->radioYJ2Y = (gcnew System::Windows::Forms::RadioButton());
            this->labelRawData = (gcnew System::Windows::Forms::Label());
            this->textBoxRawData = (gcnew System::Windows::Forms::TextBox());
            this->labelPreviewData = (gcnew System::Windows::Forms::Label());
            this->textBoxPreviewData = (gcnew System::Windows::Forms::TextBox());
            this->labelStatus = (gcnew System::Windows::Forms::Label());
            this->panelButtons = (gcnew System::Windows::Forms::TableLayoutPanel());
            this->buttonOpenBackups = (gcnew System::Windows::Forms::Button());
            this->buttonBackup = (gcnew System::Windows::Forms::Button());
            this->buttonRestore = (gcnew System::Windows::Forms::Button());
            this->buttonApply = (gcnew System::Windows::Forms::Button());
            this->refreshDebounceTimer = (gcnew System::Windows::Forms::Timer(this->components));
            this->toolTipOemName = (gcnew System::Windows::Forms::ToolTip(this->components));
            this->comboBoxLanguage = (gcnew System::Windows::Forms::ComboBox());
            this->tabMain = (gcnew System::Windows::Forms::TabControl());
            this->tabPageOem = (gcnew System::Windows::Forms::TabPage());
            this->tabPageAxes = (gcnew System::Windows::Forms::TabPage());
            this->layoutAxesTab = (gcnew System::Windows::Forms::TableLayoutPanel());
            this->labelAxesGridHeader = (gcnew System::Windows::Forms::Label());
            this->labelButtonsGridHeader = (gcnew System::Windows::Forms::Label());
            this->dataGridAxes = (gcnew System::Windows::Forms::DataGridView());
            this->colAxesIndex = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
            this->colAxesName = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
            this->dataGridButtons = (gcnew System::Windows::Forms::DataGridView());
            this->colButtonsIndex = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
            this->colButtonsName = (gcnew System::Windows::Forms::DataGridViewTextBoxColumn());
            this->panelAxesButtons = (gcnew System::Windows::Forms::TableLayoutPanel());
            this->buttonReloadNames = (gcnew System::Windows::Forms::Button());
            this->buttonApplyNames = (gcnew System::Windows::Forms::Button());
            this->rootLayout->SuspendLayout();
            this->groupBoxFlags->SuspendLayout();
            this->innerLayoutFlags->SuspendLayout();
            this->panelXAxis->SuspendLayout();
            this->panelYAxis->SuspendLayout();
            this->panelButtons->SuspendLayout();
            this->tabMain->SuspendLayout();
            this->tabPageOem->SuspendLayout();
            this->tabPageAxes->SuspendLayout();
            this->layoutAxesTab->SuspendLayout();
            (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->dataGridAxes))->BeginInit();
            (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->dataGridButtons))->BeginInit();
            this->panelAxesButtons->SuspendLayout();
            this->SuspendLayout();
            // 
            // rootLayout
            // 
            this->rootLayout->ColumnCount = 3;
            this->rootLayout->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle()));
            this->rootLayout->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 100)));
            this->rootLayout->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle()));
            this->rootLayout->Controls->Add(this->labelDevice, 0, 0);
            this->rootLayout->Controls->Add(this->comboBoxDevice, 0, 1);
            this->rootLayout->Controls->Add(this->buttonRefresh, 2, 1);
            this->rootLayout->Controls->Add(this->labelOemNameCaption, 0, 2);
            this->rootLayout->Controls->Add(this->textBoxOemName, 0, 3);
            this->rootLayout->Controls->Add(this->labelOemDataCaption, 0, 4);
            this->rootLayout->Controls->Add(this->labelDwNumButtons, 0, 5);
            this->rootLayout->Controls->Add(this->textBoxDwNumButtons, 1, 5);
            this->rootLayout->Controls->Add(this->groupBoxFlags, 0, 6);
            this->rootLayout->Controls->Add(this->labelRawData, 0, 7);
            this->rootLayout->Controls->Add(this->textBoxRawData, 1, 7);
            this->rootLayout->Controls->Add(this->labelPreviewData, 0, 8);
            this->rootLayout->Controls->Add(this->textBoxPreviewData, 1, 8);
            this->rootLayout->Controls->Add(this->labelStatus, 0, 9);
            this->rootLayout->Controls->Add(this->panelButtons, 0, 10);
            this->rootLayout->Dock = System::Windows::Forms::DockStyle::Fill;
            this->rootLayout->Location = System::Drawing::Point(0, 0);
            this->rootLayout->Margin = System::Windows::Forms::Padding(0);
            this->rootLayout->Name = L"rootLayout";
            this->rootLayout->Padding = System::Windows::Forms::Padding(28, 8, 28, 30);
            this->rootLayout->RowCount = 11;
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 33)));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 495)));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 114)));
            this->rootLayout->Size = System::Drawing::Size(1288, 1046);
            this->rootLayout->TabIndex = 0;
            // 
            // labelDevice
            // 
            this->labelDevice->AutoSize = true;
            this->rootLayout->SetColumnSpan(this->labelDevice, 3);
            this->labelDevice->Location = System::Drawing::Point(28, 8);
            this->labelDevice->Margin = System::Windows::Forms::Padding(0, 0, 0, 10);
            this->labelDevice->Name = L"labelDevice";
            this->labelDevice->Size = System::Drawing::Size(115, 25);
            this->labelDevice->TabIndex = 0;
            this->labelDevice->Text = L"Устройство:";
            // 
            // comboBoxDevice
            // 
            this->rootLayout->SetColumnSpan(this->comboBoxDevice, 2);
            this->comboBoxDevice->Dock = System::Windows::Forms::DockStyle::Fill;
            this->comboBoxDevice->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
            this->comboBoxDevice->ItemHeight = 25;
            this->comboBoxDevice->Location = System::Drawing::Point(28, 43);
            this->comboBoxDevice->Margin = System::Windows::Forms::Padding(0, 0, 12, 0);
            this->comboBoxDevice->Name = L"comboBoxDevice";
            this->comboBoxDevice->Size = System::Drawing::Size(980, 33);
            this->comboBoxDevice->TabIndex = 1;
            this->comboBoxDevice->SelectedIndexChanged += gcnew System::EventHandler(this, &MainForm::comboBoxDevice_SelectedIndexChanged);
            // 
            // buttonRefresh
            // 
            this->buttonRefresh->Dock = System::Windows::Forms::DockStyle::Fill;
            this->buttonRefresh->Location = System::Drawing::Point(1020, 43);
            this->buttonRefresh->Margin = System::Windows::Forms::Padding(0);
            this->buttonRefresh->MinimumSize = System::Drawing::Size(240, 0);
            this->buttonRefresh->Name = L"buttonRefresh";
            this->buttonRefresh->Size = System::Drawing::Size(240, 33);
            this->buttonRefresh->TabIndex = 2;
            this->buttonRefresh->Text = L"Обновить";
            this->buttonRefresh->Click += gcnew System::EventHandler(this, &MainForm::buttonRefresh_Click);
            // 
            // labelOemNameCaption
            // 
            this->labelOemNameCaption->AutoSize = true;
            this->rootLayout->SetColumnSpan(this->labelOemNameCaption, 3);
            this->labelOemNameCaption->Location = System::Drawing::Point(28, 104);
            this->labelOemNameCaption->Margin = System::Windows::Forms::Padding(0, 22, 0, 8);
            this->labelOemNameCaption->Name = L"labelOemNameCaption";
            this->labelOemNameCaption->Size = System::Drawing::Size(308, 25);
            this->labelOemNameCaption->TabIndex = 3;
            this->labelOemNameCaption->Text = L"Название устройства (OEMName):";
            // 
            // textBoxOemName
            // 
            this->rootLayout->SetColumnSpan(this->textBoxOemName, 3);
            this->textBoxOemName->Dock = System::Windows::Forms::DockStyle::Fill;
            this->textBoxOemName->Location = System::Drawing::Point(28, 137);
            this->textBoxOemName->Margin = System::Windows::Forms::Padding(0);
            this->textBoxOemName->MaxLength = 128;
            this->textBoxOemName->Name = L"textBoxOemName";
            this->textBoxOemName->Size = System::Drawing::Size(1232, 33);
            this->textBoxOemName->TabIndex = 4;
            this->textBoxOemName->KeyPress += gcnew System::Windows::Forms::KeyPressEventHandler(this, &MainForm::textBoxOemName_KeyPress);
            // 
            // labelOemDataCaption
            // 
            this->labelOemDataCaption->AutoSize = true;
            this->rootLayout->SetColumnSpan(this->labelOemDataCaption, 3);
            this->labelOemDataCaption->Location = System::Drawing::Point(28, 192);
            this->labelOemDataCaption->Margin = System::Windows::Forms::Padding(0, 22, 0, 12);
            this->labelOemDataCaption->Name = L"labelOemDataCaption";
            this->labelOemDataCaption->Size = System::Drawing::Size(314, 25);
            this->labelOemDataCaption->TabIndex = 5;
            this->labelOemDataCaption->Text = L"Параметры устройства (OEMData):";
            // 
            // labelDwNumButtons
            // 
            this->labelDwNumButtons->AutoSize = true;
            this->labelDwNumButtons->Location = System::Drawing::Point(28, 237);
            this->labelDwNumButtons->Margin = System::Windows::Forms::Padding(0, 8, 18, 8);
            this->labelDwNumButtons->Name = L"labelDwNumButtons";
            this->labelDwNumButtons->Size = System::Drawing::Size(264, 25);
            this->labelDwNumButtons->TabIndex = 7;
            this->labelDwNumButtons->Text = L"Кнопок (hws.dwNumButtons):";
            // 
            // textBoxDwNumButtons
            // 
            this->rootLayout->SetColumnSpan(this->textBoxDwNumButtons, 2);
            this->textBoxDwNumButtons->Dock = System::Windows::Forms::DockStyle::Fill;
            this->textBoxDwNumButtons->Location = System::Drawing::Point(310, 233);
            this->textBoxDwNumButtons->Margin = System::Windows::Forms::Padding(0, 4, 0, 4);
            this->textBoxDwNumButtons->MaxLength = 2;
            this->textBoxDwNumButtons->Name = L"textBoxDwNumButtons";
            this->textBoxDwNumButtons->Size = System::Drawing::Size(950, 33);
            this->textBoxDwNumButtons->TabIndex = 7;
            this->textBoxDwNumButtons->TextChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            this->textBoxDwNumButtons->KeyPress += gcnew System::Windows::Forms::KeyPressEventHandler(this, &MainForm::textBoxDwNumButtons_KeyPress);
            // 
            // groupBoxFlags
            // 
            this->rootLayout->SetColumnSpan(this->groupBoxFlags, 3);
            this->groupBoxFlags->Controls->Add(this->innerLayoutFlags);
            this->groupBoxFlags->Dock = System::Windows::Forms::DockStyle::Fill;
            this->groupBoxFlags->Location = System::Drawing::Point(28, 288);
            this->groupBoxFlags->Margin = System::Windows::Forms::Padding(0, 18, 0, 0);
            this->groupBoxFlags->Name = L"groupBoxFlags";
            this->groupBoxFlags->Padding = System::Windows::Forms::Padding(18, 12, 18, 18);
            this->groupBoxFlags->Size = System::Drawing::Size(1232, 477);
            this->groupBoxFlags->TabIndex = 8;
            this->groupBoxFlags->TabStop = false;
            this->groupBoxFlags->Text = L"Параметры (hws.dwFlags)";
            // 
            // innerLayoutFlags
            // 
            this->innerLayoutFlags->ColumnCount = 4;
            this->innerLayoutFlags->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent,
                25)));
            this->innerLayoutFlags->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent,
                25)));
            this->innerLayoutFlags->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent,
                25)));
            this->innerLayoutFlags->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent,
                25)));
            this->innerLayoutFlags->Controls->Add(this->labelDeviceTypeHeader, 0, 0);
            this->innerLayoutFlags->Controls->Add(this->labelAxesHeader, 1, 0);
            this->innerLayoutFlags->Controls->Add(this->labelXAxisHeader, 2, 0);
            this->innerLayoutFlags->Controls->Add(this->labelYAxisHeader, 3, 0);
            this->innerLayoutFlags->Controls->Add(this->radioGeneric, 0, 1);
            this->innerLayoutFlags->Controls->Add(this->checkHasZ, 1, 1);
            this->innerLayoutFlags->Controls->Add(this->radioYoke, 0, 2);
            this->innerLayoutFlags->Controls->Add(this->checkHasR, 1, 2);
            this->innerLayoutFlags->Controls->Add(this->radioGamepad, 0, 3);
            this->innerLayoutFlags->Controls->Add(this->checkHasU, 1, 3);
            this->innerLayoutFlags->Controls->Add(this->radioWheel, 0, 4);
            this->innerLayoutFlags->Controls->Add(this->checkHasV, 1, 4);
            this->innerLayoutFlags->Controls->Add(this->checkHasPov, 1, 5);
            this->innerLayoutFlags->Controls->Add(this->checkPovIsButtonCombos, 1, 6);
            this->innerLayoutFlags->Controls->Add(this->checkPovIsPoll, 1, 7);
            this->innerLayoutFlags->Controls->Add(this->panelXAxis, 2, 1);
            this->innerLayoutFlags->Controls->Add(this->panelYAxis, 3, 1);
            this->innerLayoutFlags->Dock = System::Windows::Forms::DockStyle::Fill;
            this->innerLayoutFlags->Location = System::Drawing::Point(18, 38);
            this->innerLayoutFlags->Margin = System::Windows::Forms::Padding(0);
            this->innerLayoutFlags->Name = L"innerLayoutFlags";
            this->innerLayoutFlags->RowCount = 8;
            this->innerLayoutFlags->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->innerLayoutFlags->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->innerLayoutFlags->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->innerLayoutFlags->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->innerLayoutFlags->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->innerLayoutFlags->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->innerLayoutFlags->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->innerLayoutFlags->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->innerLayoutFlags->Size = System::Drawing::Size(1196, 421);
            this->innerLayoutFlags->TabIndex = 0;
            // 
            // labelDeviceTypeHeader
            // 
            this->labelDeviceTypeHeader->AutoSize = true;
            this->labelDeviceTypeHeader->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 9.5F));
            this->labelDeviceTypeHeader->Location = System::Drawing::Point(0, 0);
            this->labelDeviceTypeHeader->Margin = System::Windows::Forms::Padding(0, 0, 0, 9);
            this->labelDeviceTypeHeader->Name = L"labelDeviceTypeHeader";
            this->labelDeviceTypeHeader->Size = System::Drawing::Size(146, 25);
            this->labelDeviceTypeHeader->TabIndex = 0;
            this->labelDeviceTypeHeader->Text = L"Тип устройства";
            // 
            // labelAxesHeader
            // 
            this->labelAxesHeader->AutoSize = true;
            this->labelAxesHeader->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 9.5F));
            this->labelAxesHeader->Location = System::Drawing::Point(317, 0);
            this->labelAxesHeader->Margin = System::Windows::Forms::Padding(18, 0, 0, 9);
            this->labelAxesHeader->Name = L"labelAxesHeader";
            this->labelAxesHeader->Size = System::Drawing::Size(146, 25);
            this->labelAxesHeader->TabIndex = 1;
            this->labelAxesHeader->Text = L"Доступные оси";
            // 
            // labelXAxisHeader
            // 
            this->labelXAxisHeader->AutoSize = true;
            this->labelXAxisHeader->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 9.5F));
            this->labelXAxisHeader->Location = System::Drawing::Point(616, 0);
            this->labelXAxisHeader->Margin = System::Windows::Forms::Padding(18, 0, 0, 9);
            this->labelXAxisHeader->Name = L"labelXAxisHeader";
            this->labelXAxisHeader->Size = System::Drawing::Size(62, 25);
            this->labelXAxisHeader->TabIndex = 10;
            this->labelXAxisHeader->Text = L"Ось X";
            // 
            // labelYAxisHeader
            // 
            this->labelYAxisHeader->AutoSize = true;
            this->labelYAxisHeader->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 9.5F));
            this->labelYAxisHeader->Location = System::Drawing::Point(915, 0);
            this->labelYAxisHeader->Margin = System::Windows::Forms::Padding(18, 0, 0, 9);
            this->labelYAxisHeader->Name = L"labelYAxisHeader";
            this->labelYAxisHeader->Size = System::Drawing::Size(61, 25);
            this->labelYAxisHeader->TabIndex = 12;
            this->labelYAxisHeader->Text = L"Ось Y";
            // 
            // radioGeneric
            // 
            this->radioGeneric->AutoSize = true;
            this->radioGeneric->Location = System::Drawing::Point(0, 37);
            this->radioGeneric->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->radioGeneric->Name = L"radioGeneric";
            this->radioGeneric->Size = System::Drawing::Size(209, 29);
            this->radioGeneric->TabIndex = 2;
            this->radioGeneric->Text = L"Обычный джойстик";
            this->radioGeneric->UseVisualStyleBackColor = true;
            this->radioGeneric->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // checkHasZ
            // 
            this->checkHasZ->AutoSize = true;
            this->checkHasZ->Location = System::Drawing::Point(317, 37);
            this->checkHasZ->Margin = System::Windows::Forms::Padding(18, 3, 0, 3);
            this->checkHasZ->Name = L"checkHasZ";
            this->checkHasZ->Size = System::Drawing::Size(158, 29);
            this->checkHasZ->TabIndex = 3;
            this->checkHasZ->Text = L"Z (третья ось)";
            this->checkHasZ->UseVisualStyleBackColor = true;
            this->checkHasZ->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioYoke
            // 
            this->radioYoke->AutoSize = true;
            this->radioYoke->Location = System::Drawing::Point(0, 72);
            this->radioYoke->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->radioYoke->Name = L"radioYoke";
            this->radioYoke->Size = System::Drawing::Size(169, 29);
            this->radioYoke->TabIndex = 4;
            this->radioYoke->Text = L"Штурвал (Yoke)";
            this->radioYoke->UseVisualStyleBackColor = true;
            this->radioYoke->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // checkHasR
            // 
            this->checkHasR->AutoSize = true;
            this->checkHasR->Location = System::Drawing::Point(317, 72);
            this->checkHasR->Margin = System::Windows::Forms::Padding(18, 3, 0, 3);
            this->checkHasR->Name = L"checkHasR";
            this->checkHasR->Size = System::Drawing::Size(199, 29);
            this->checkHasR->TabIndex = 5;
            this->checkHasR->Text = L"R (Rudder, 4-я ось)";
            this->checkHasR->UseVisualStyleBackColor = true;
            this->checkHasR->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioGamepad
            // 
            this->radioGamepad->AutoSize = true;
            this->radioGamepad->Location = System::Drawing::Point(0, 107);
            this->radioGamepad->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->radioGamepad->Name = L"radioGamepad";
            this->radioGamepad->Size = System::Drawing::Size(111, 29);
            this->radioGamepad->TabIndex = 6;
            this->radioGamepad->Text = L"Геймпад";
            this->radioGamepad->UseVisualStyleBackColor = true;
            this->radioGamepad->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // checkHasU
            // 
            this->checkHasU->AutoSize = true;
            this->checkHasU->Location = System::Drawing::Point(317, 107);
            this->checkHasU->Margin = System::Windows::Forms::Padding(18, 3, 0, 3);
            this->checkHasU->Name = L"checkHasU";
            this->checkHasU->Size = System::Drawing::Size(131, 29);
            this->checkHasU->TabIndex = 7;
            this->checkHasU->Text = L"U (5-я ось)";
            this->checkHasU->UseVisualStyleBackColor = true;
            this->checkHasU->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioWheel
            // 
            this->radioWheel->AutoSize = true;
            this->radioWheel->Location = System::Drawing::Point(0, 142);
            this->radioWheel->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->radioWheel->Name = L"radioWheel";
            this->radioWheel->Size = System::Drawing::Size(210, 29);
            this->radioWheel->TabIndex = 8;
            this->radioWheel->Text = L"Руль / Car controller";
            this->radioWheel->UseVisualStyleBackColor = true;
            this->radioWheel->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // checkHasV
            // 
            this->checkHasV->AutoSize = true;
            this->checkHasV->Location = System::Drawing::Point(317, 142);
            this->checkHasV->Margin = System::Windows::Forms::Padding(18, 3, 0, 3);
            this->checkHasV->Name = L"checkHasV";
            this->checkHasV->Size = System::Drawing::Size(130, 29);
            this->checkHasV->TabIndex = 9;
            this->checkHasV->Text = L"V (6-я ось)";
            this->checkHasV->UseVisualStyleBackColor = true;
            this->checkHasV->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // checkHasPov
            // 
            this->checkHasPov->AutoSize = true;
            this->checkHasPov->Location = System::Drawing::Point(317, 177);
            this->checkHasPov->Margin = System::Windows::Forms::Padding(18, 3, 0, 3);
            this->checkHasPov->Name = L"checkHasPov";
            this->checkHasPov->Size = System::Drawing::Size(148, 29);
            this->checkHasPov->TabIndex = 10;
            this->checkHasPov->Text = L"POV (шляпа)";
            this->checkHasPov->UseVisualStyleBackColor = true;
            this->checkHasPov->CheckedChanged += gcnew System::EventHandler(this, &MainForm::checkHasPov_CheckedChanged);
            // 
            // checkPovIsButtonCombos
            // 
            this->checkPovIsButtonCombos->AutoSize = true;
            this->checkPovIsButtonCombos->Location = System::Drawing::Point(353, 211);
            this->checkPovIsButtonCombos->Margin = System::Windows::Forms::Padding(54, 2, 0, 2);
            this->checkPovIsButtonCombos->Name = L"checkPovIsButtonCombos";
            this->checkPovIsButtonCombos->Size = System::Drawing::Size(228, 29);
            this->checkPovIsButtonCombos->TabIndex = 11;
            this->checkPovIsButtonCombos->Text = L"└ комбинация кнопок";
            this->checkPovIsButtonCombos->UseVisualStyleBackColor = true;
            this->checkPovIsButtonCombos->Visible = false;
            this->checkPovIsButtonCombos->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // checkPovIsPoll
            // 
            this->checkPovIsPoll->AutoSize = true;
            this->checkPovIsPoll->Location = System::Drawing::Point(353, 244);
            this->checkPovIsPoll->Margin = System::Windows::Forms::Padding(54, 2, 0, 2);
            this->checkPovIsPoll->Name = L"checkPovIsPoll";
            this->checkPovIsPoll->Size = System::Drawing::Size(208, 29);
            this->checkPovIsPoll->TabIndex = 12;
            this->checkPovIsPoll->Text = L"└ опрос драйвером";
            this->checkPovIsPoll->UseVisualStyleBackColor = true;
            this->checkPovIsPoll->Visible = false;
            this->checkPovIsPoll->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // panelXAxis
            // 
            this->panelXAxis->Controls->Add(this->radioXDefault);
            this->panelXAxis->Controls->Add(this->radioXJ1Y);
            this->panelXAxis->Controls->Add(this->radioXJ2X);
            this->panelXAxis->Controls->Add(this->radioXJ2Y);
            this->panelXAxis->Dock = System::Windows::Forms::DockStyle::Fill;
            this->panelXAxis->FlowDirection = System::Windows::Forms::FlowDirection::TopDown;
            this->panelXAxis->Location = System::Drawing::Point(616, 34);
            this->panelXAxis->Margin = System::Windows::Forms::Padding(18, 0, 0, 0);
            this->panelXAxis->Name = L"panelXAxis";
            this->innerLayoutFlags->SetRowSpan(this->panelXAxis, 7);
            this->panelXAxis->Size = System::Drawing::Size(281, 387);
            this->panelXAxis->TabIndex = 11;
            this->panelXAxis->WrapContents = false;
            // 
            // radioXDefault
            // 
            this->radioXDefault->AutoSize = true;
            this->radioXDefault->Location = System::Drawing::Point(0, 3);
            this->radioXDefault->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->radioXDefault->Name = L"radioXDefault";
            this->radioXDefault->Size = System::Drawing::Size(172, 29);
            this->radioXDefault->TabIndex = 0;
            this->radioXDefault->Text = L"J1 X (по умолч.)";
            this->radioXDefault->UseVisualStyleBackColor = true;
            this->radioXDefault->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioXJ1Y
            // 
            this->radioXJ1Y->AutoSize = true;
            this->radioXJ1Y->Location = System::Drawing::Point(0, 38);
            this->radioXJ1Y->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->radioXJ1Y->Name = L"radioXJ1Y";
            this->radioXJ1Y->Size = System::Drawing::Size(70, 29);
            this->radioXJ1Y->TabIndex = 1;
            this->radioXJ1Y->Text = L"J1 Y";
            this->radioXJ1Y->UseVisualStyleBackColor = true;
            this->radioXJ1Y->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioXJ2X
            // 
            this->radioXJ2X->AutoSize = true;
            this->radioXJ2X->Location = System::Drawing::Point(0, 73);
            this->radioXJ2X->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->radioXJ2X->Name = L"radioXJ2X";
            this->radioXJ2X->Size = System::Drawing::Size(70, 29);
            this->radioXJ2X->TabIndex = 2;
            this->radioXJ2X->Text = L"J2 X";
            this->radioXJ2X->UseVisualStyleBackColor = true;
            this->radioXJ2X->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioXJ2Y
            // 
            this->radioXJ2Y->AutoSize = true;
            this->radioXJ2Y->Location = System::Drawing::Point(0, 108);
            this->radioXJ2Y->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->radioXJ2Y->Name = L"radioXJ2Y";
            this->radioXJ2Y->Size = System::Drawing::Size(70, 29);
            this->radioXJ2Y->TabIndex = 3;
            this->radioXJ2Y->Text = L"J2 Y";
            this->radioXJ2Y->UseVisualStyleBackColor = true;
            this->radioXJ2Y->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // panelYAxis
            // 
            this->panelYAxis->Controls->Add(this->radioYDefault);
            this->panelYAxis->Controls->Add(this->radioYJ1X);
            this->panelYAxis->Controls->Add(this->radioYJ2X);
            this->panelYAxis->Controls->Add(this->radioYJ2Y);
            this->panelYAxis->Dock = System::Windows::Forms::DockStyle::Fill;
            this->panelYAxis->FlowDirection = System::Windows::Forms::FlowDirection::TopDown;
            this->panelYAxis->Location = System::Drawing::Point(915, 34);
            this->panelYAxis->Margin = System::Windows::Forms::Padding(18, 0, 0, 0);
            this->panelYAxis->Name = L"panelYAxis";
            this->innerLayoutFlags->SetRowSpan(this->panelYAxis, 7);
            this->panelYAxis->Size = System::Drawing::Size(281, 387);
            this->panelYAxis->TabIndex = 13;
            this->panelYAxis->WrapContents = false;
            // 
            // radioYDefault
            // 
            this->radioYDefault->AutoSize = true;
            this->radioYDefault->Location = System::Drawing::Point(0, 3);
            this->radioYDefault->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->radioYDefault->Name = L"radioYDefault";
            this->radioYDefault->Size = System::Drawing::Size(172, 29);
            this->radioYDefault->TabIndex = 0;
            this->radioYDefault->Text = L"J1 Y (по умолч.)";
            this->radioYDefault->UseVisualStyleBackColor = true;
            this->radioYDefault->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioYJ1X
            // 
            this->radioYJ1X->AutoSize = true;
            this->radioYJ1X->Location = System::Drawing::Point(0, 38);
            this->radioYJ1X->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->radioYJ1X->Name = L"radioYJ1X";
            this->radioYJ1X->Size = System::Drawing::Size(70, 29);
            this->radioYJ1X->TabIndex = 1;
            this->radioYJ1X->Text = L"J1 X";
            this->radioYJ1X->UseVisualStyleBackColor = true;
            this->radioYJ1X->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioYJ2X
            // 
            this->radioYJ2X->AutoSize = true;
            this->radioYJ2X->Location = System::Drawing::Point(0, 73);
            this->radioYJ2X->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->radioYJ2X->Name = L"radioYJ2X";
            this->radioYJ2X->Size = System::Drawing::Size(70, 29);
            this->radioYJ2X->TabIndex = 2;
            this->radioYJ2X->Text = L"J2 X";
            this->radioYJ2X->UseVisualStyleBackColor = true;
            this->radioYJ2X->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioYJ2Y
            // 
            this->radioYJ2Y->AutoSize = true;
            this->radioYJ2Y->Location = System::Drawing::Point(0, 108);
            this->radioYJ2Y->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->radioYJ2Y->Name = L"radioYJ2Y";
            this->radioYJ2Y->Size = System::Drawing::Size(70, 29);
            this->radioYJ2Y->TabIndex = 3;
            this->radioYJ2Y->Text = L"J2 Y";
            this->radioYJ2Y->UseVisualStyleBackColor = true;
            this->radioYJ2Y->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // labelRawData
            // 
            this->labelRawData->AutoSize = true;
            this->labelRawData->Location = System::Drawing::Point(28, 787);
            this->labelRawData->Margin = System::Windows::Forms::Padding(0, 22, 18, 8);
            this->labelRawData->Name = L"labelRawData";
            this->labelRawData->Size = System::Drawing::Size(218, 25);
            this->labelRawData->TabIndex = 10;
            this->labelRawData->Text = L"Сырые байты OEMData:";
            // 
            // textBoxRawData
            // 
            this->textBoxRawData->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Left)
                | System::Windows::Forms::AnchorStyles::Right));
            this->rootLayout->SetColumnSpan(this->textBoxRawData, 2);
            this->textBoxRawData->Font = (gcnew System::Drawing::Font(L"Consolas", 10));
            this->textBoxRawData->Location = System::Drawing::Point(310, 783);
            this->textBoxRawData->Margin = System::Windows::Forms::Padding(0, 18, 0, 4);
            this->textBoxRawData->Name = L"textBoxRawData";
            this->textBoxRawData->ReadOnly = true;
            this->textBoxRawData->Size = System::Drawing::Size(950, 31);
            this->textBoxRawData->TabIndex = 11;
            // 
            // labelPreviewData
            // 
            this->labelPreviewData->AutoSize = true;
            this->labelPreviewData->Location = System::Drawing::Point(28, 832);
            this->labelPreviewData->Margin = System::Windows::Forms::Padding(0, 12, 18, 8);
            this->labelPreviewData->Name = L"labelPreviewData";
            this->labelPreviewData->Size = System::Drawing::Size(171, 25);
            this->labelPreviewData->TabIndex = 12;
            this->labelPreviewData->Text = L"После изменений:";
            // 
            // textBoxPreviewData
            // 
            this->textBoxPreviewData->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Left)
                | System::Windows::Forms::AnchorStyles::Right));
            this->rootLayout->SetColumnSpan(this->textBoxPreviewData, 2);
            this->textBoxPreviewData->Font = (gcnew System::Drawing::Font(L"Consolas", 10));
            this->textBoxPreviewData->Location = System::Drawing::Point(310, 828);
            this->textBoxPreviewData->Margin = System::Windows::Forms::Padding(0, 8, 0, 4);
            this->textBoxPreviewData->Name = L"textBoxPreviewData";
            this->textBoxPreviewData->ReadOnly = true;
            this->textBoxPreviewData->Size = System::Drawing::Size(950, 31);
            this->textBoxPreviewData->TabIndex = 13;
            // 
            // labelStatus
            // 
            this->labelStatus->AutoSize = true;
            this->rootLayout->SetColumnSpan(this->labelStatus, 3);
            this->labelStatus->Location = System::Drawing::Point(28, 883);
            this->labelStatus->Margin = System::Windows::Forms::Padding(0, 18, 0, 0);
            this->labelStatus->Name = L"labelStatus";
            this->labelStatus->Size = System::Drawing::Size(0, 25);
            this->labelStatus->TabIndex = 9;
            // 
            // panelButtons
            // 
            this->panelButtons->ColumnCount = 3;
            this->rootLayout->SetColumnSpan(this->panelButtons, 3);
            this->panelButtons->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 100)));
            this->panelButtons->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Absolute,
                210)));
            this->panelButtons->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Absolute,
                210)));
            this->panelButtons->Controls->Add(this->buttonOpenBackups, 1, 0);
            this->panelButtons->Controls->Add(this->buttonBackup, 2, 0);
            this->panelButtons->Controls->Add(this->buttonRestore, 1, 1);
            this->panelButtons->Controls->Add(this->buttonApply, 2, 1);
            this->panelButtons->Dock = System::Windows::Forms::DockStyle::Fill;
            this->panelButtons->Location = System::Drawing::Point(28, 920);
            this->panelButtons->Margin = System::Windows::Forms::Padding(0, 12, 0, 0);
            this->panelButtons->Name = L"panelButtons";
            this->panelButtons->RowCount = 2;
            this->panelButtons->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 51)));
            this->panelButtons->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 51)));
            this->panelButtons->Size = System::Drawing::Size(1232, 102);
            this->panelButtons->TabIndex = 14;
            // 
            // buttonOpenBackups
            // 
            this->buttonOpenBackups->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right));
            this->buttonOpenBackups->Location = System::Drawing::Point(812, 0);
            this->buttonOpenBackups->Margin = System::Windows::Forms::Padding(0, 0, 9, 0);
            this->buttonOpenBackups->Name = L"buttonOpenBackups";
            this->buttonOpenBackups->Size = System::Drawing::Size(201, 45);
            this->buttonOpenBackups->TabIndex = 22;
            this->buttonOpenBackups->Text = L"Папка бэкапов";
            this->buttonOpenBackups->Click += gcnew System::EventHandler(this, &MainForm::buttonOpenBackups_Click);
            // 
            // buttonBackup
            // 
            this->buttonBackup->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right));
            this->buttonBackup->Location = System::Drawing::Point(1031, 0);
            this->buttonBackup->Margin = System::Windows::Forms::Padding(9, 0, 0, 0);
            this->buttonBackup->Name = L"buttonBackup";
            this->buttonBackup->Size = System::Drawing::Size(201, 45);
            this->buttonBackup->TabIndex = 20;
            this->buttonBackup->Text = L"Сделать бэкап";
            this->buttonBackup->Click += gcnew System::EventHandler(this, &MainForm::buttonBackup_Click);
            // 
            // buttonRestore
            // 
            this->buttonRestore->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right));
            this->buttonRestore->Location = System::Drawing::Point(812, 51);
            this->buttonRestore->Margin = System::Windows::Forms::Padding(0, 0, 9, 0);
            this->buttonRestore->Name = L"buttonRestore";
            this->buttonRestore->Size = System::Drawing::Size(201, 45);
            this->buttonRestore->TabIndex = 23;
            this->buttonRestore->Text = L"Восстановить";
            this->buttonRestore->Click += gcnew System::EventHandler(this, &MainForm::buttonRestore_Click);
            // 
            // buttonApply
            // 
            this->buttonApply->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right));
            this->buttonApply->Location = System::Drawing::Point(1031, 51);
            this->buttonApply->Margin = System::Windows::Forms::Padding(9, 0, 0, 0);
            this->buttonApply->Name = L"buttonApply";
            this->buttonApply->Size = System::Drawing::Size(201, 45);
            this->buttonApply->TabIndex = 21;
            this->buttonApply->Text = L"Применить";
            this->buttonApply->Click += gcnew System::EventHandler(this, &MainForm::buttonApply_Click);
            // 
            // refreshDebounceTimer
            // 
            this->refreshDebounceTimer->Interval = 250;
            this->refreshDebounceTimer->Tick += gcnew System::EventHandler(this, &MainForm::refreshDebounceTimer_Tick);
            // 
            // comboBoxLanguage
            // 
            this->comboBoxLanguage->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right));
            this->comboBoxLanguage->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
            this->comboBoxLanguage->FlatStyle = System::Windows::Forms::FlatStyle::Flat;
            this->comboBoxLanguage->Location = System::Drawing::Point(1045, 9);
            this->comboBoxLanguage->Margin = System::Windows::Forms::Padding(4);
            this->comboBoxLanguage->Name = L"comboBoxLanguage";
            this->comboBoxLanguage->Size = System::Drawing::Size(223, 33);
            this->comboBoxLanguage->TabIndex = 30;
            // 
            // tabMain
            // 
            this->tabMain->Controls->Add(this->tabPageOem);
            this->tabMain->Controls->Add(this->tabPageAxes);
            this->tabMain->Dock = System::Windows::Forms::DockStyle::Fill;
            this->tabMain->Location = System::Drawing::Point(0, 50);
            this->tabMain->Name = L"tabMain";
            this->tabMain->SelectedIndex = 0;
            this->tabMain->Size = System::Drawing::Size(1296, 1084);
            this->tabMain->TabIndex = 40;
            // 
            // tabPageOem
            // 
            this->tabPageOem->Controls->Add(this->rootLayout);
            this->tabPageOem->Location = System::Drawing::Point(4, 34);
            this->tabPageOem->Name = L"tabPageOem";
            this->tabPageOem->Size = System::Drawing::Size(1288, 1046);
            this->tabPageOem->TabIndex = 0;
            this->tabPageOem->Text = L"OEMData";
            // 
            // tabPageAxes
            // 
            this->tabPageAxes->Controls->Add(this->layoutAxesTab);
            this->tabPageAxes->Location = System::Drawing::Point(4, 34);
            this->tabPageAxes->Name = L"tabPageAxes";
            this->tabPageAxes->Padding = System::Windows::Forms::Padding(28);
            this->tabPageAxes->Size = System::Drawing::Size(1288, 1046);
            this->tabPageAxes->TabIndex = 1;
            this->tabPageAxes->Text = L"Оси и кнопки";
            // 
            // layoutAxesTab
            // 
            this->layoutAxesTab->ColumnCount = 2;
            this->layoutAxesTab->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent,
                50)));
            this->layoutAxesTab->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent,
                50)));
            this->layoutAxesTab->Controls->Add(this->labelAxesGridHeader, 0, 0);
            this->layoutAxesTab->Controls->Add(this->labelButtonsGridHeader, 1, 0);
            this->layoutAxesTab->Controls->Add(this->dataGridAxes, 0, 1);
            this->layoutAxesTab->Controls->Add(this->dataGridButtons, 1, 1);
            this->layoutAxesTab->Controls->Add(this->panelAxesButtons, 0, 2);
            this->layoutAxesTab->Dock = System::Windows::Forms::DockStyle::Fill;
            this->layoutAxesTab->Location = System::Drawing::Point(28, 28);
            this->layoutAxesTab->Name = L"layoutAxesTab";
            this->layoutAxesTab->RowCount = 3;
            this->layoutAxesTab->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->layoutAxesTab->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 100)));
            this->layoutAxesTab->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 70)));
            this->layoutAxesTab->Size = System::Drawing::Size(1232, 990);
            this->layoutAxesTab->TabIndex = 0;
            // 
            // labelAxesGridHeader
            // 
            this->labelAxesGridHeader->AutoSize = true;
            this->labelAxesGridHeader->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 10));
            this->labelAxesGridHeader->Location = System::Drawing::Point(0, 0);
            this->labelAxesGridHeader->Margin = System::Windows::Forms::Padding(0, 0, 12, 10);
            this->labelAxesGridHeader->Name = L"labelAxesGridHeader";
            this->labelAxesGridHeader->Size = System::Drawing::Size(48, 28);
            this->labelAxesGridHeader->TabIndex = 0;
            this->labelAxesGridHeader->Text = L"Оси";
            // 
            // labelButtonsGridHeader
            // 
            this->labelButtonsGridHeader->AutoSize = true;
            this->labelButtonsGridHeader->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 10));
            this->labelButtonsGridHeader->Location = System::Drawing::Point(628, 0);
            this->labelButtonsGridHeader->Margin = System::Windows::Forms::Padding(12, 0, 0, 10);
            this->labelButtonsGridHeader->Name = L"labelButtonsGridHeader";
            this->labelButtonsGridHeader->Size = System::Drawing::Size(83, 28);
            this->labelButtonsGridHeader->TabIndex = 1;
            this->labelButtonsGridHeader->Text = L"Кнопки";
            // 
            // dataGridAxes
            // 
            this->dataGridAxes->AllowUserToAddRows = false;
            this->dataGridAxes->AllowUserToDeleteRows = false;
            this->dataGridAxes->AllowUserToResizeRows = false;
            this->dataGridAxes->AutoSizeRowsMode = System::Windows::Forms::DataGridViewAutoSizeRowsMode::AllCells;
            this->dataGridAxes->ColumnHeadersHeightSizeMode = System::Windows::Forms::DataGridViewColumnHeadersHeightSizeMode::AutoSize;
            this->dataGridAxes->Columns->AddRange(gcnew cli::array< System::Windows::Forms::DataGridViewColumn^  >(2) {
                this->colAxesIndex,
                    this->colAxesName
            });
            this->dataGridAxes->Dock = System::Windows::Forms::DockStyle::Fill;
            this->dataGridAxes->EnableHeadersVisualStyles = false;
            this->dataGridAxes->Location = System::Drawing::Point(0, 38);
            this->dataGridAxes->Margin = System::Windows::Forms::Padding(0, 0, 12, 18);
            this->dataGridAxes->Name = L"dataGridAxes";
            this->dataGridAxes->RowHeadersVisible = false;
            this->dataGridAxes->RowHeadersWidth = 62;
            this->dataGridAxes->SelectionMode = System::Windows::Forms::DataGridViewSelectionMode::CellSelect;
            this->dataGridAxes->Size = System::Drawing::Size(604, 864);
            this->dataGridAxes->TabIndex = 2;
            // 
            // colAxesIndex
            // 
            this->colAxesIndex->AutoSizeMode = System::Windows::Forms::DataGridViewAutoSizeColumnMode::None;
            this->colAxesIndex->HeaderText = L"#";
            this->colAxesIndex->MinimumWidth = 8;
            this->colAxesIndex->Name = L"colAxesIndex";
            this->colAxesIndex->ReadOnly = true;
            this->colAxesIndex->Width = 60;
            // 
            // colAxesName
            // 
            this->colAxesName->AutoSizeMode = System::Windows::Forms::DataGridViewAutoSizeColumnMode::Fill;
            this->colAxesName->HeaderText = L"Name";
            this->colAxesName->MinimumWidth = 8;
            this->colAxesName->Name = L"colAxesName";
            // 
            // dataGridButtons
            // 
            this->dataGridButtons->AllowUserToAddRows = false;
            this->dataGridButtons->AllowUserToDeleteRows = false;
            this->dataGridButtons->AllowUserToResizeRows = false;
            this->dataGridButtons->AutoSizeRowsMode = System::Windows::Forms::DataGridViewAutoSizeRowsMode::AllCells;
            this->dataGridButtons->ColumnHeadersHeightSizeMode = System::Windows::Forms::DataGridViewColumnHeadersHeightSizeMode::AutoSize;
            this->dataGridButtons->Columns->AddRange(gcnew cli::array< System::Windows::Forms::DataGridViewColumn^  >(2) {
                this->colButtonsIndex,
                    this->colButtonsName
            });
            this->dataGridButtons->Dock = System::Windows::Forms::DockStyle::Fill;
            this->dataGridButtons->EnableHeadersVisualStyles = false;
            this->dataGridButtons->Location = System::Drawing::Point(628, 38);
            this->dataGridButtons->Margin = System::Windows::Forms::Padding(12, 0, 0, 18);
            this->dataGridButtons->Name = L"dataGridButtons";
            this->dataGridButtons->RowHeadersVisible = false;
            this->dataGridButtons->RowHeadersWidth = 62;
            this->dataGridButtons->SelectionMode = System::Windows::Forms::DataGridViewSelectionMode::CellSelect;
            this->dataGridButtons->Size = System::Drawing::Size(604, 864);
            this->dataGridButtons->TabIndex = 3;
            // 
            // colButtonsIndex
            // 
            this->colButtonsIndex->AutoSizeMode = System::Windows::Forms::DataGridViewAutoSizeColumnMode::None;
            this->colButtonsIndex->HeaderText = L"#";
            this->colButtonsIndex->MinimumWidth = 8;
            this->colButtonsIndex->Name = L"colButtonsIndex";
            this->colButtonsIndex->ReadOnly = true;
            this->colButtonsIndex->Width = 60;
            // 
            // colButtonsName
            // 
            this->colButtonsName->AutoSizeMode = System::Windows::Forms::DataGridViewAutoSizeColumnMode::Fill;
            this->colButtonsName->HeaderText = L"Name";
            this->colButtonsName->MinimumWidth = 8;
            this->colButtonsName->Name = L"colButtonsName";
            // 
            // panelAxesButtons
            // 
            this->panelAxesButtons->ColumnCount = 2;
            this->layoutAxesTab->SetColumnSpan(this->panelAxesButtons, 2);
            this->panelAxesButtons->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent,
                50)));
            this->panelAxesButtons->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent,
                50)));
            this->panelAxesButtons->Controls->Add(this->buttonReloadNames, 0, 0);
            this->panelAxesButtons->Controls->Add(this->buttonApplyNames, 1, 0);
            this->panelAxesButtons->Dock = System::Windows::Forms::DockStyle::Fill;
            this->panelAxesButtons->Location = System::Drawing::Point(0, 920);
            this->panelAxesButtons->Margin = System::Windows::Forms::Padding(0);
            this->panelAxesButtons->Name = L"panelAxesButtons";
            this->panelAxesButtons->RowCount = 1;
            this->panelAxesButtons->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 100)));
            this->panelAxesButtons->Size = System::Drawing::Size(1232, 70);
            this->panelAxesButtons->TabIndex = 4;
            // 
            // buttonReloadNames
            // 
            this->buttonReloadNames->Anchor = System::Windows::Forms::AnchorStyles::None;
            this->buttonReloadNames->Location = System::Drawing::Point(148, 12);
            this->buttonReloadNames->Margin = System::Windows::Forms::Padding(12);
            this->buttonReloadNames->Name = L"buttonReloadNames";
            this->buttonReloadNames->Size = System::Drawing::Size(320, 46);
            this->buttonReloadNames->TabIndex = 41;
            this->buttonReloadNames->Text = L"Перечитать";
            this->buttonReloadNames->Click += gcnew System::EventHandler(this, &MainForm::buttonReloadNames_Click);
            // 
            // buttonApplyNames
            // 
            this->buttonApplyNames->Anchor = System::Windows::Forms::AnchorStyles::None;
            this->buttonApplyNames->Location = System::Drawing::Point(764, 12);
            this->buttonApplyNames->Margin = System::Windows::Forms::Padding(12);
            this->buttonApplyNames->Name = L"buttonApplyNames";
            this->buttonApplyNames->Size = System::Drawing::Size(320, 46);
            this->buttonApplyNames->TabIndex = 42;
            this->buttonApplyNames->Text = L"Применить имена";
            this->buttonApplyNames->Click += gcnew System::EventHandler(this, &MainForm::buttonApplyNames_Click);
            // 
            // MainForm
            // 
            this->AutoScaleDimensions = System::Drawing::SizeF(144, 144);
            this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Dpi;
            this->ClientSize = System::Drawing::Size(1296, 1134);
            this->Controls->Add(this->tabMain);
            this->Controls->Add(this->comboBoxLanguage);
            this->Font = (gcnew System::Drawing::Font(L"Segoe UI", 9.5F));
            this->Margin = System::Windows::Forms::Padding(8);
            this->MinimumSize = System::Drawing::Size(1039, 1136);
            this->Name = L"MainForm";
            this->Padding = System::Windows::Forms::Padding(0, 50, 0, 0);
            this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
            this->Text = L"WinJoy Tweaker";
            this->Load += gcnew System::EventHandler(this, &MainForm::MainForm_Load);
            this->rootLayout->ResumeLayout(false);
            this->rootLayout->PerformLayout();
            this->groupBoxFlags->ResumeLayout(false);
            this->innerLayoutFlags->ResumeLayout(false);
            this->innerLayoutFlags->PerformLayout();
            this->panelXAxis->ResumeLayout(false);
            this->panelXAxis->PerformLayout();
            this->panelYAxis->ResumeLayout(false);
            this->panelYAxis->PerformLayout();
            this->panelButtons->ResumeLayout(false);
            this->tabMain->ResumeLayout(false);
            this->tabPageOem->ResumeLayout(false);
            this->tabPageAxes->ResumeLayout(false);
            this->layoutAxesTab->ResumeLayout(false);
            this->layoutAxesTab->PerformLayout();
            (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->dataGridAxes))->EndInit();
            (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->dataGridButtons))->EndInit();
            this->panelAxesButtons->ResumeLayout(false);
            this->ResumeLayout(false);

        }
#pragma endregion
    private: System::Void MainForm_Load(System::Object^ sender, System::EventArgs^ e) {}
};
}
