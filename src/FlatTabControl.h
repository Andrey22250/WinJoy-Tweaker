#pragma once

namespace WinJoytweaker {

    // TabControl, который после системной отрисовки закрашивает левую,
    // правую и нижнюю кромки цветом фона родителя. Событие Paint у
    // TabControl на этих участках не вызывается, поэтому единственный
    // надёжный путь — перехватить WM_PAINT в WndProc и дорисовать поверх
    // через CreateGraphics() после Base::WndProc.
    //
    // Вынесен в отдельный заголовок: WinForms-конструктор C++/CLI требует,
    // чтобы designable-класс формы был единственным ref-классом в своём .h.
    public ref class FlatTabControl : public System::Windows::Forms::TabControl
    {
    public:
        // Толщина перекрытия в пикселях (одинакова для всех трёх кромок).
        property int BorderOverlay;

        FlatTabControl() { BorderOverlay = 6; }

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
                System::Drawing::SolidBrush^ br = gcnew System::Drawing::SolidBrush(bg);
                try {
                    int tabStripH = this->ItemSize.Height + 4;
                    int w = this->ClientSize.Width;
                    int h = this->ClientSize.Height;
                    int t = BorderOverlay;
                    g->FillRectangle(br, 0,     tabStripH, t, h - tabStripH);
                    g->FillRectangle(br, w - t, tabStripH, t, h - tabStripH);
                    g->FillRectangle(br, 0,     h - t,     w, t);
                }
                finally { delete br; }
            }
            finally { delete g; }
        }
    };

}
