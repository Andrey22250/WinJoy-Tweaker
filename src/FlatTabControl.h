#pragma once

namespace WinJoytweaker {

    // TabControl, который после системной отрисовки закрашивает левую,
    // правую и нижнюю кромки цветом фона родителя. Событие Paint у
    // TabControl на этих участках не вызывается, поэтому единственный
    // надёжный путь — перехватить WM_PAINT в WndProc и дорисовать поверх
    // через CreateGraphics() после Base::WndProc.
    //
    // Этот тип НЕ упоминается в MainForm.h и InitializeComponent: дизайнер
    // C++/CLI не открывает форму, если в её .h есть второй ref-класс, и не
    // резолвит кастомный тип из другого заголовка. Поэтому подмена tabMain
    // на FlatTabControl выполняется в рантайме (MainForm::SwapTabControlToFlat).
    public ref class FlatTabControl : public System::Windows::Forms::TabControl
    {
    public:
        // Толщина перекрытия в пикселях (одинакова для всех трёх кромок).
        property int BorderOverlay;

        FlatTabControl() { BorderOverlay = 6; }
        ~FlatTabControl() { delete _overlayBrush; }

    protected:
        virtual void WndProc(System::Windows::Forms::Message% m) override
        {
            const int WM_PAINT = 0x000F;
            System::Windows::Forms::TabControl::WndProc(m);
            if (m.Msg != WM_PAINT) return;

            System::Drawing::Graphics^ g = this->CreateGraphics();
            try {
                System::Drawing::Color bg = (this->Parent != nullptr)
                    ? this->Parent->BackColor
                    : this->BackColor;
                // Кисть кэшируется между перерисовками (WM_PAINT приходит при
                // каждом переключении вкладки/перекрытии окна); пересоздаём её
                // только если цвет фона родителя сменился.
                if (_overlayBrush == nullptr || _overlayBrush->Color != bg) {
                    delete _overlayBrush;
                    _overlayBrush = gcnew System::Drawing::SolidBrush(bg);
                }
                int tabStripH = this->ItemSize.Height + 4;
                int w = this->ClientSize.Width;
                int h = this->ClientSize.Height;
                int t = BorderOverlay;
                g->FillRectangle(_overlayBrush, 0,     tabStripH, t, h - tabStripH);
                g->FillRectangle(_overlayBrush, w - t, tabStripH, t, h - tabStripH);
                g->FillRectangle(_overlayBrush, 0,     h - t,     w, t);
            }
            finally { delete g; }
        }

    private:
        // Кэш кисти кромок; владеет ею контрол, освобождается в деструкторе.
        System::Drawing::SolidBrush^ _overlayBrush;
    };

}
