#pragma once

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

        // Снимок оригинальных байт OEMData — основа для предпросмотра изменений.
        array<System::Byte>^                       _oemDataSnapshot;

        void ApplyDarkTheme();
        void RefreshDeviceList();
        void LoadDeviceData();
        void ClearFlagControls();
        void UpdatePreviewData();
        void flagsControl_Changed(System::Object^ sender, System::EventArgs^ e);
        void checkHasPov_CheckedChanged(System::Object^ sender, System::EventArgs^ e);
        void textBoxDwNumButtons_KeyPress(System::Object^ sender, System::Windows::Forms::KeyPressEventArgs^ e);
        void textBoxOemName_KeyPress(System::Object^ sender, System::Windows::Forms::KeyPressEventArgs^ e);
        void buttonApply_Click(System::Object^ sender, System::EventArgs^ e);
        void buttonBackup_Click(System::Object^ sender, System::EventArgs^ e);
        void buttonRefresh_Click(System::Object^ sender, System::EventArgs^ e);
        void comboBoxDevice_SelectedIndexChanged(System::Object^ sender, System::EventArgs^ e);
        void refreshDebounceTimer_Tick(System::Object^ sender, System::EventArgs^ e);

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
            this->refreshDebounceTimer = (gcnew System::Windows::Forms::Timer(this->components));
            this->panelButtons = (gcnew System::Windows::Forms::TableLayoutPanel());
            this->buttonApply = (gcnew System::Windows::Forms::Button());
            this->buttonBackup = (gcnew System::Windows::Forms::Button());
            this->rootLayout->SuspendLayout();
            this->panelButtons->SuspendLayout();
            this->groupBoxFlags->SuspendLayout();
            this->innerLayoutFlags->SuspendLayout();
            this->panelXAxis->SuspendLayout();
            this->panelYAxis->SuspendLayout();
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
            this->rootLayout->Padding = System::Windows::Forms::Padding(19, 20, 19, 20);
            this->rootLayout->RowCount = 11;
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 26)));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 330)));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle()));
            this->rootLayout->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Absolute, 40)));
            this->rootLayout->Size = System::Drawing::Size(864, 720);
            this->rootLayout->TabIndex = 0;
            // 
            // labelDevice
            // 
            this->labelDevice->AutoSize = true;
            this->rootLayout->SetColumnSpan(this->labelDevice, 3);
            this->labelDevice->Location = System::Drawing::Point(19, 20);
            this->labelDevice->Margin = System::Windows::Forms::Padding(0, 0, 0, 7);
            this->labelDevice->Name = L"labelDevice";
            this->labelDevice->Size = System::Drawing::Size(78, 17);
            this->labelDevice->TabIndex = 0;
            this->labelDevice->Text = L"Устройство:";
            // 
            // comboBoxDevice
            // 
            this->rootLayout->SetColumnSpan(this->comboBoxDevice, 2);
            this->comboBoxDevice->Dock = System::Windows::Forms::DockStyle::Fill;
            this->comboBoxDevice->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
            this->comboBoxDevice->ItemHeight = 17;
            this->comboBoxDevice->Location = System::Drawing::Point(19, 44);
            this->comboBoxDevice->Margin = System::Windows::Forms::Padding(0, 0, 8, 0);
            this->comboBoxDevice->Name = L"comboBoxDevice";
            this->comboBoxDevice->Size = System::Drawing::Size(658, 25);
            this->comboBoxDevice->TabIndex = 1;
            this->comboBoxDevice->SelectedIndexChanged += gcnew System::EventHandler(this, &MainForm::comboBoxDevice_SelectedIndexChanged);
            // 
            // buttonRefresh
            // 
            this->buttonRefresh->Dock = System::Windows::Forms::DockStyle::Fill;
            this->buttonRefresh->Location = System::Drawing::Point(685, 44);
            this->buttonRefresh->Margin = System::Windows::Forms::Padding(0);
            this->buttonRefresh->MinimumSize = System::Drawing::Size(160, 0);
            this->buttonRefresh->Name = L"buttonRefresh";
            this->buttonRefresh->Size = System::Drawing::Size(160, 26);
            this->buttonRefresh->TabIndex = 2;
            this->buttonRefresh->Text = L"Обновить";
            this->buttonRefresh->Click += gcnew System::EventHandler(this, &MainForm::buttonRefresh_Click);
            // 
            // labelOemNameCaption
            // 
            this->labelOemNameCaption->AutoSize = true;
            this->rootLayout->SetColumnSpan(this->labelOemNameCaption, 3);
            this->labelOemNameCaption->Location = System::Drawing::Point(19, 85);
            this->labelOemNameCaption->Margin = System::Windows::Forms::Padding(0, 15, 0, 5);
            this->labelOemNameCaption->Name = L"labelOemNameCaption";
            this->labelOemNameCaption->Size = System::Drawing::Size(213, 17);
            this->labelOemNameCaption->TabIndex = 3;
            this->labelOemNameCaption->Text = L"Название устройства (OEMName):";
            // 
            // textBoxOemName
            // 
            this->rootLayout->SetColumnSpan(this->textBoxOemName, 3);
            this->textBoxOemName->Dock = System::Windows::Forms::DockStyle::Fill;
            this->textBoxOemName->Location = System::Drawing::Point(19, 107);
            this->textBoxOemName->Margin = System::Windows::Forms::Padding(0);
            this->textBoxOemName->MaxLength = 128;
            this->textBoxOemName->Name = L"textBoxOemName";
            this->textBoxOemName->Size = System::Drawing::Size(826, 24);
            this->textBoxOemName->TabIndex = 4;
            this->textBoxOemName->KeyPress += gcnew System::Windows::Forms::KeyPressEventHandler(this, &MainForm::textBoxOemName_KeyPress);
            // 
            // labelOemDataCaption
            // 
            this->labelOemDataCaption->AutoSize = true;
            this->rootLayout->SetColumnSpan(this->labelOemDataCaption, 3);
            this->labelOemDataCaption->Location = System::Drawing::Point(19, 146);
            this->labelOemDataCaption->Margin = System::Windows::Forms::Padding(0, 15, 0, 8);
            this->labelOemDataCaption->Name = L"labelOemDataCaption";
            this->labelOemDataCaption->Size = System::Drawing::Size(217, 17);
            this->labelOemDataCaption->TabIndex = 5;
            this->labelOemDataCaption->Text = L"Параметры устройства (OEMData):";
            // 
            // labelDwNumButtons
            // 
            this->labelDwNumButtons->AutoSize = true;
            this->labelDwNumButtons->Location = System::Drawing::Point(19, 176);
            this->labelDwNumButtons->Margin = System::Windows::Forms::Padding(0, 5, 12, 5);
            this->labelDwNumButtons->Name = L"labelDwNumButtons";
            this->labelDwNumButtons->Size = System::Drawing::Size(180, 17);
            this->labelDwNumButtons->TabIndex = 7;
            this->labelDwNumButtons->Text = L"Кнопок (hws.dwNumButtons):";
            // 
            // textBoxDwNumButtons
            // 
            this->rootLayout->SetColumnSpan(this->textBoxDwNumButtons, 2);
            this->textBoxDwNumButtons->Dock = System::Windows::Forms::DockStyle::Fill;
            this->textBoxDwNumButtons->Location = System::Drawing::Point(211, 174);
            this->textBoxDwNumButtons->Margin = System::Windows::Forms::Padding(0, 3, 0, 3);
            this->textBoxDwNumButtons->MaxLength = 2;
            this->textBoxDwNumButtons->Name = L"textBoxDwNumButtons";
            this->textBoxDwNumButtons->Size = System::Drawing::Size(634, 24);
            this->textBoxDwNumButtons->TabIndex = 7;
            this->textBoxDwNumButtons->TextChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            this->textBoxDwNumButtons->KeyPress += gcnew System::Windows::Forms::KeyPressEventHandler(this, &MainForm::textBoxDwNumButtons_KeyPress);
            // 
            // groupBoxFlags
            // 
            this->rootLayout->SetColumnSpan(this->groupBoxFlags, 3);
            this->groupBoxFlags->Controls->Add(this->innerLayoutFlags);
            this->groupBoxFlags->Dock = System::Windows::Forms::DockStyle::Fill;
            this->groupBoxFlags->Location = System::Drawing::Point(19, 213);
            this->groupBoxFlags->Margin = System::Windows::Forms::Padding(0, 12, 0, 0);
            this->groupBoxFlags->Name = L"groupBoxFlags";
            this->groupBoxFlags->Padding = System::Windows::Forms::Padding(12, 8, 12, 12);
            this->groupBoxFlags->Size = System::Drawing::Size(826, 258);
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
            this->innerLayoutFlags->Location = System::Drawing::Point(12, 25);
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
            this->innerLayoutFlags->Size = System::Drawing::Size(802, 221);
            this->innerLayoutFlags->TabIndex = 0;
            // 
            // labelDeviceTypeHeader
            // 
            this->labelDeviceTypeHeader->AutoSize = true;
            this->labelDeviceTypeHeader->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 9.5F));
            this->labelDeviceTypeHeader->Location = System::Drawing::Point(0, 0);
            this->labelDeviceTypeHeader->Margin = System::Windows::Forms::Padding(0, 0, 0, 6);
            this->labelDeviceTypeHeader->Name = L"labelDeviceTypeHeader";
            this->labelDeviceTypeHeader->Size = System::Drawing::Size(103, 17);
            this->labelDeviceTypeHeader->TabIndex = 0;
            this->labelDeviceTypeHeader->Text = L"Тип устройства";
            // 
            // labelAxesHeader
            // 
            this->labelAxesHeader->AutoSize = true;
            this->labelAxesHeader->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 9.5F));
            this->labelAxesHeader->Location = System::Drawing::Point(212, 0);
            this->labelAxesHeader->Margin = System::Windows::Forms::Padding(12, 0, 0, 6);
            this->labelAxesHeader->Name = L"labelAxesHeader";
            this->labelAxesHeader->Size = System::Drawing::Size(103, 17);
            this->labelAxesHeader->TabIndex = 1;
            this->labelAxesHeader->Text = L"Доступные оси";
            // 
            // labelXAxisHeader
            // 
            this->labelXAxisHeader->AutoSize = true;
            this->labelXAxisHeader->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 9.5F));
            this->labelXAxisHeader->Location = System::Drawing::Point(412, 0);
            this->labelXAxisHeader->Margin = System::Windows::Forms::Padding(12, 0, 0, 6);
            this->labelXAxisHeader->Name = L"labelXAxisHeader";
            this->labelXAxisHeader->Size = System::Drawing::Size(43, 17);
            this->labelXAxisHeader->TabIndex = 10;
            this->labelXAxisHeader->Text = L"Ось X";
            // 
            // labelYAxisHeader
            // 
            this->labelYAxisHeader->AutoSize = true;
            this->labelYAxisHeader->Font = (gcnew System::Drawing::Font(L"Segoe UI Semibold", 9.5F));
            this->labelYAxisHeader->Location = System::Drawing::Point(612, 0);
            this->labelYAxisHeader->Margin = System::Windows::Forms::Padding(12, 0, 0, 6);
            this->labelYAxisHeader->Name = L"labelYAxisHeader";
            this->labelYAxisHeader->Size = System::Drawing::Size(43, 17);
            this->labelYAxisHeader->TabIndex = 12;
            this->labelYAxisHeader->Text = L"Ось Y";
            // 
            // radioGeneric
            // 
            this->radioGeneric->AutoSize = true;
            this->radioGeneric->Location = System::Drawing::Point(0, 25);
            this->radioGeneric->Margin = System::Windows::Forms::Padding(0, 2, 0, 2);
            this->radioGeneric->Name = L"radioGeneric";
            this->radioGeneric->Size = System::Drawing::Size(142, 21);
            this->radioGeneric->TabIndex = 2;
            this->radioGeneric->Text = L"Обычный джойстик";
            this->radioGeneric->UseVisualStyleBackColor = true;
            this->radioGeneric->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // checkHasZ
            // 
            this->checkHasZ->AutoSize = true;
            this->checkHasZ->Location = System::Drawing::Point(212, 25);
            this->checkHasZ->Margin = System::Windows::Forms::Padding(12, 2, 0, 2);
            this->checkHasZ->Name = L"checkHasZ";
            this->checkHasZ->Size = System::Drawing::Size(110, 21);
            this->checkHasZ->TabIndex = 3;
            this->checkHasZ->Text = L"Z (третья ось)";
            this->checkHasZ->UseVisualStyleBackColor = true;
            this->checkHasZ->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioYoke
            // 
            this->radioYoke->AutoSize = true;
            this->radioYoke->Location = System::Drawing::Point(0, 50);
            this->radioYoke->Margin = System::Windows::Forms::Padding(0, 2, 0, 2);
            this->radioYoke->Name = L"radioYoke";
            this->radioYoke->Size = System::Drawing::Size(118, 21);
            this->radioYoke->TabIndex = 4;
            this->radioYoke->Text = L"Штурвал (Yoke)";
            this->radioYoke->UseVisualStyleBackColor = true;
            this->radioYoke->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // checkHasR
            // 
            this->checkHasR->AutoSize = true;
            this->checkHasR->Location = System::Drawing::Point(212, 50);
            this->checkHasR->Margin = System::Windows::Forms::Padding(12, 2, 0, 2);
            this->checkHasR->Name = L"checkHasR";
            this->checkHasR->Size = System::Drawing::Size(141, 21);
            this->checkHasR->TabIndex = 5;
            this->checkHasR->Text = L"R (Rudder, 4-я ось)";
            this->checkHasR->UseVisualStyleBackColor = true;
            this->checkHasR->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioGamepad
            // 
            this->radioGamepad->AutoSize = true;
            this->radioGamepad->Location = System::Drawing::Point(0, 75);
            this->radioGamepad->Margin = System::Windows::Forms::Padding(0, 2, 0, 2);
            this->radioGamepad->Name = L"radioGamepad";
            this->radioGamepad->Size = System::Drawing::Size(76, 21);
            this->radioGamepad->TabIndex = 6;
            this->radioGamepad->Text = L"Геймпад";
            this->radioGamepad->UseVisualStyleBackColor = true;
            this->radioGamepad->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // checkHasU
            // 
            this->checkHasU->AutoSize = true;
            this->checkHasU->Location = System::Drawing::Point(212, 75);
            this->checkHasU->Margin = System::Windows::Forms::Padding(12, 2, 0, 2);
            this->checkHasU->Name = L"checkHasU";
            this->checkHasU->Size = System::Drawing::Size(92, 21);
            this->checkHasU->TabIndex = 7;
            this->checkHasU->Text = L"U (5-я ось)";
            this->checkHasU->UseVisualStyleBackColor = true;
            this->checkHasU->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioWheel
            // 
            this->radioWheel->AutoSize = true;
            this->radioWheel->Location = System::Drawing::Point(0, 100);
            this->radioWheel->Margin = System::Windows::Forms::Padding(0, 2, 0, 2);
            this->radioWheel->Name = L"radioWheel";
            this->radioWheel->Size = System::Drawing::Size(146, 21);
            this->radioWheel->TabIndex = 8;
            this->radioWheel->Text = L"Руль / Car controller";
            this->radioWheel->UseVisualStyleBackColor = true;
            this->radioWheel->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // checkHasV
            // 
            this->checkHasV->AutoSize = true;
            this->checkHasV->Location = System::Drawing::Point(212, 100);
            this->checkHasV->Margin = System::Windows::Forms::Padding(12, 2, 0, 2);
            this->checkHasV->Name = L"checkHasV";
            this->checkHasV->Size = System::Drawing::Size(91, 21);
            this->checkHasV->TabIndex = 9;
            this->checkHasV->Text = L"V (6-я ось)";
            this->checkHasV->UseVisualStyleBackColor = true;
            this->checkHasV->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // checkHasPov
            // 
            this->checkHasPov->AutoSize = true;
            this->checkHasPov->Location = System::Drawing::Point(212, 125);
            this->checkHasPov->Margin = System::Windows::Forms::Padding(12, 2, 0, 2);
            this->checkHasPov->Name = L"checkHasPov";
            this->checkHasPov->Size = System::Drawing::Size(103, 21);
            this->checkHasPov->TabIndex = 10;
            this->checkHasPov->Text = L"POV (шляпа)";
            this->checkHasPov->UseVisualStyleBackColor = true;
            this->checkHasPov->CheckedChanged += gcnew System::EventHandler(this, &MainForm::checkHasPov_CheckedChanged);
            //
            // checkPovIsButtonCombos
            //
            this->checkPovIsButtonCombos->AutoSize = true;
            this->checkPovIsButtonCombos->Visible = false;
            this->checkPovIsButtonCombos->Margin = System::Windows::Forms::Padding(36, 1, 0, 1);
            this->checkPovIsButtonCombos->Name = L"checkPovIsButtonCombos";
            this->checkPovIsButtonCombos->TabIndex = 11;
            this->checkPovIsButtonCombos->Text = L"└ комбинация кнопок";
            this->checkPovIsButtonCombos->UseVisualStyleBackColor = true;
            this->checkPovIsButtonCombos->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            //
            // checkPovIsPoll
            //
            this->checkPovIsPoll->AutoSize = true;
            this->checkPovIsPoll->Visible = false;
            this->checkPovIsPoll->Margin = System::Windows::Forms::Padding(36, 1, 0, 1);
            this->checkPovIsPoll->Name = L"checkPovIsPoll";
            this->checkPovIsPoll->TabIndex = 12;
            this->checkPovIsPoll->Text = L"└ опрос драйвером";
            this->checkPovIsPoll->UseVisualStyleBackColor = true;
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
            this->panelXAxis->Location = System::Drawing::Point(412, 23);
            this->panelXAxis->Margin = System::Windows::Forms::Padding(12, 0, 0, 0);
            this->panelXAxis->Name = L"panelXAxis";
            this->innerLayoutFlags->SetRowSpan(this->panelXAxis, 7);
            this->panelXAxis->Size = System::Drawing::Size(188, 198);
            this->panelXAxis->TabIndex = 11;
            this->panelXAxis->WrapContents = false;
            // 
            // radioXDefault
            // 
            this->radioXDefault->AutoSize = true;
            this->radioXDefault->Location = System::Drawing::Point(0, 2);
            this->radioXDefault->Margin = System::Windows::Forms::Padding(0, 2, 0, 2);
            this->radioXDefault->Name = L"radioXDefault";
            this->radioXDefault->Size = System::Drawing::Size(121, 21);
            this->radioXDefault->TabIndex = 0;
            this->radioXDefault->Text = L"J1 X (по умолч.)";
            this->radioXDefault->UseVisualStyleBackColor = true;
            this->radioXDefault->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioXJ1Y
            // 
            this->radioXJ1Y->AutoSize = true;
            this->radioXJ1Y->Location = System::Drawing::Point(0, 27);
            this->radioXJ1Y->Margin = System::Windows::Forms::Padding(0, 2, 0, 2);
            this->radioXJ1Y->Name = L"radioXJ1Y";
            this->radioXJ1Y->Size = System::Drawing::Size(49, 21);
            this->radioXJ1Y->TabIndex = 1;
            this->radioXJ1Y->Text = L"J1 Y";
            this->radioXJ1Y->UseVisualStyleBackColor = true;
            this->radioXJ1Y->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioXJ2X
            // 
            this->radioXJ2X->AutoSize = true;
            this->radioXJ2X->Location = System::Drawing::Point(0, 52);
            this->radioXJ2X->Margin = System::Windows::Forms::Padding(0, 2, 0, 2);
            this->radioXJ2X->Name = L"radioXJ2X";
            this->radioXJ2X->Size = System::Drawing::Size(50, 21);
            this->radioXJ2X->TabIndex = 2;
            this->radioXJ2X->Text = L"J2 X";
            this->radioXJ2X->UseVisualStyleBackColor = true;
            this->radioXJ2X->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioXJ2Y
            // 
            this->radioXJ2Y->AutoSize = true;
            this->radioXJ2Y->Location = System::Drawing::Point(0, 77);
            this->radioXJ2Y->Margin = System::Windows::Forms::Padding(0, 2, 0, 2);
            this->radioXJ2Y->Name = L"radioXJ2Y";
            this->radioXJ2Y->Size = System::Drawing::Size(49, 21);
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
            this->panelYAxis->Location = System::Drawing::Point(612, 23);
            this->panelYAxis->Margin = System::Windows::Forms::Padding(12, 0, 0, 0);
            this->panelYAxis->Name = L"panelYAxis";
            this->innerLayoutFlags->SetRowSpan(this->panelYAxis, 7);
            this->panelYAxis->Size = System::Drawing::Size(190, 198);
            this->panelYAxis->TabIndex = 13;
            this->panelYAxis->WrapContents = false;
            // 
            // radioYDefault
            // 
            this->radioYDefault->AutoSize = true;
            this->radioYDefault->Location = System::Drawing::Point(0, 2);
            this->radioYDefault->Margin = System::Windows::Forms::Padding(0, 2, 0, 2);
            this->radioYDefault->Name = L"radioYDefault";
            this->radioYDefault->Size = System::Drawing::Size(120, 21);
            this->radioYDefault->TabIndex = 0;
            this->radioYDefault->Text = L"J1 Y (по умолч.)";
            this->radioYDefault->UseVisualStyleBackColor = true;
            this->radioYDefault->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioYJ1X
            // 
            this->radioYJ1X->AutoSize = true;
            this->radioYJ1X->Location = System::Drawing::Point(0, 27);
            this->radioYJ1X->Margin = System::Windows::Forms::Padding(0, 2, 0, 2);
            this->radioYJ1X->Name = L"radioYJ1X";
            this->radioYJ1X->Size = System::Drawing::Size(50, 21);
            this->radioYJ1X->TabIndex = 1;
            this->radioYJ1X->Text = L"J1 X";
            this->radioYJ1X->UseVisualStyleBackColor = true;
            this->radioYJ1X->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioYJ2X
            // 
            this->radioYJ2X->AutoSize = true;
            this->radioYJ2X->Location = System::Drawing::Point(0, 52);
            this->radioYJ2X->Margin = System::Windows::Forms::Padding(0, 2, 0, 2);
            this->radioYJ2X->Name = L"radioYJ2X";
            this->radioYJ2X->Size = System::Drawing::Size(50, 21);
            this->radioYJ2X->TabIndex = 2;
            this->radioYJ2X->Text = L"J2 X";
            this->radioYJ2X->UseVisualStyleBackColor = true;
            this->radioYJ2X->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // radioYJ2Y
            // 
            this->radioYJ2Y->AutoSize = true;
            this->radioYJ2Y->Location = System::Drawing::Point(0, 77);
            this->radioYJ2Y->Margin = System::Windows::Forms::Padding(0, 2, 0, 2);
            this->radioYJ2Y->Name = L"radioYJ2Y";
            this->radioYJ2Y->Size = System::Drawing::Size(49, 21);
            this->radioYJ2Y->TabIndex = 3;
            this->radioYJ2Y->Text = L"J2 Y";
            this->radioYJ2Y->UseVisualStyleBackColor = true;
            this->radioYJ2Y->CheckedChanged += gcnew System::EventHandler(this, &MainForm::flagsControl_Changed);
            // 
            // labelRawData
            // 
            this->labelRawData->AutoSize = true;
            this->labelRawData->Location = System::Drawing::Point(19, 486);
            this->labelRawData->Margin = System::Windows::Forms::Padding(0, 15, 12, 5);
            this->labelRawData->Name = L"labelRawData";
            this->labelRawData->Size = System::Drawing::Size(152, 17);
            this->labelRawData->TabIndex = 10;
            this->labelRawData->Text = L"Сырые байты OEMData:";
            // 
            // textBoxRawData
            // 
            this->rootLayout->SetColumnSpan(this->textBoxRawData, 2);
            this->textBoxRawData->Dock = System::Windows::Forms::DockStyle::Fill;
            this->textBoxRawData->Font = (gcnew System::Drawing::Font(L"Consolas", 10));
            this->textBoxRawData->Location = System::Drawing::Point(211, 483);
            this->textBoxRawData->Margin = System::Windows::Forms::Padding(0, 12, 0, 3);
            this->textBoxRawData->Name = L"textBoxRawData";
            this->textBoxRawData->ReadOnly = true;
            this->textBoxRawData->Size = System::Drawing::Size(634, 23);
            this->textBoxRawData->TabIndex = 11;
            // 
            // labelPreviewData
            // 
            this->labelPreviewData->AutoSize = true;
            this->labelPreviewData->Location = System::Drawing::Point(19, 517);
            this->labelPreviewData->Margin = System::Windows::Forms::Padding(0, 8, 12, 5);
            this->labelPreviewData->Name = L"labelPreviewData";
            this->labelPreviewData->Size = System::Drawing::Size(116, 17);
            this->labelPreviewData->TabIndex = 12;
            this->labelPreviewData->Text = L"После изменений:";
            // 
            // textBoxPreviewData
            // 
            this->rootLayout->SetColumnSpan(this->textBoxPreviewData, 2);
            this->textBoxPreviewData->Dock = System::Windows::Forms::DockStyle::Fill;
            this->textBoxPreviewData->Font = (gcnew System::Drawing::Font(L"Consolas", 10));
            this->textBoxPreviewData->Location = System::Drawing::Point(211, 514);
            this->textBoxPreviewData->Margin = System::Windows::Forms::Padding(0, 5, 0, 3);
            this->textBoxPreviewData->Name = L"textBoxPreviewData";
            this->textBoxPreviewData->ReadOnly = true;
            this->textBoxPreviewData->Size = System::Drawing::Size(634, 23);
            this->textBoxPreviewData->TabIndex = 13;
            // 
            // labelStatus
            // 
            this->labelStatus->AutoSize = true;
            this->rootLayout->SetColumnSpan(this->labelStatus, 3);
            this->labelStatus->Location = System::Drawing::Point(19, 552);
            this->labelStatus->Margin = System::Windows::Forms::Padding(0, 12, 0, 0);
            this->labelStatus->Name = L"labelStatus";
            this->labelStatus->Size = System::Drawing::Size(0, 17);
            this->labelStatus->TabIndex = 9;
            // 
            // refreshDebounceTimer
            // 
            this->refreshDebounceTimer->Interval = 250;
            this->refreshDebounceTimer->Tick += gcnew System::EventHandler(this, &MainForm::refreshDebounceTimer_Tick);
            //
            // panelButtons
            //
            // 3 колонки: пустой spacer слева (100%) и две фиксированные справа
            this->panelButtons->ColumnCount = 3;
            this->panelButtons->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Percent, 100)));
            this->panelButtons->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Absolute, 140)));
            this->panelButtons->ColumnStyles->Add((gcnew System::Windows::Forms::ColumnStyle(System::Windows::Forms::SizeType::Absolute, 140)));
            this->panelButtons->Controls->Add(this->buttonBackup, 1, 0);
            this->panelButtons->Controls->Add(this->buttonApply, 2, 0);
            this->panelButtons->Dock = System::Windows::Forms::DockStyle::Fill;
            this->panelButtons->Margin = System::Windows::Forms::Padding(0, 8, 0, 0);
            this->panelButtons->Name = L"panelButtons";
            this->panelButtons->RowCount = 1;
            this->panelButtons->RowStyles->Add((gcnew System::Windows::Forms::RowStyle(System::Windows::Forms::SizeType::Percent, 100)));
            this->rootLayout->SetColumnSpan(this->panelButtons, 3);
            //
            // buttonBackup
            //
            this->buttonBackup->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right);
            this->buttonBackup->Size = System::Drawing::Size(134, 30);
            this->buttonBackup->Margin = System::Windows::Forms::Padding(0, 0, 6, 0);
            this->buttonBackup->Name = L"buttonBackup";
            this->buttonBackup->TabIndex = 20;
            this->buttonBackup->Text = L"Сделать бэкап";
            this->buttonBackup->Click += gcnew System::EventHandler(this, &MainForm::buttonBackup_Click);
            //
            // buttonApply
            //
            this->buttonApply->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right);
            this->buttonApply->Size = System::Drawing::Size(134, 30);
            this->buttonApply->Margin = System::Windows::Forms::Padding(6, 0, 0, 0);
            this->buttonApply->Name = L"buttonApply";
            this->buttonApply->TabIndex = 21;
            this->buttonApply->Text = L"Применить";
            this->buttonApply->Click += gcnew System::EventHandler(this, &MainForm::buttonApply_Click);
            //
            // MainForm
            //
            this->AutoScaleDimensions = System::Drawing::SizeF(7, 17);
            this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
            this->ClientSize = System::Drawing::Size(864, 720);
            this->Controls->Add(this->rootLayout);
            this->Font = (gcnew System::Drawing::Font(L"Segoe UI", 9.5F));
            this->Margin = System::Windows::Forms::Padding(5);
            this->MinimumSize = System::Drawing::Size(700, 740);
            this->Name = L"MainForm";
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
            this->ResumeLayout(false);

        }
#pragma endregion
    private: System::Void MainForm_Load(System::Object^ sender, System::EventArgs^ e) {}
};
}
