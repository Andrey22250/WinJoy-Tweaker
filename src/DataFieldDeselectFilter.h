#pragma once

namespace WinJoytweaker {

    // Фильтр сообщений приложения: снимает выделение в полях «только чтение»
    // при клике ЛКМ в любом месте окна вне самого поля. Обычный обработчик
    // Leave не покрывает клики по нефокусируемым областям (метки, панели,
    // пустое место формы) — фокус там не меняется, поэтому нужен глобальный
    // перехват WM_LBUTTONDOWN.
    //
    // Вынесен в отдельный заголовок: в файле формы (MainForm.h) допустим только
    // один ref-класс — иначе дизайнер C++/CLI не открывает форму.
    public ref class DataFieldDeselectFilter : public System::Windows::Forms::IMessageFilter
    {
    public:
        DataFieldDeselectFilter(array<System::Windows::Forms::TextBox^>^ fields)
            : _fields(fields) {}

        virtual bool PreFilterMessage(System::Windows::Forms::Message% m)
        {
            const int WM_LBUTTONDOWN = 0x0201;
            if (m.Msg == WM_LBUTTONDOWN && _fields != nullptr) {
                System::Windows::Forms::Control^ clicked =
                    System::Windows::Forms::Control::FromHandle(m.HWnd);

                // Клик по самому контекстному меню (ToolStrip) не трогаем —
                // иначе «Копировать» потеряет выделение до срабатывания.
                bool onMenu = (dynamic_cast<System::Windows::Forms::ToolStrip^>(clicked) != nullptr);

                if (!onMenu) {
                    for each (System::Windows::Forms::TextBox^ tb in _fields) {
                        if (tb != nullptr && tb->SelectionLength > 0 && clicked != tb)
                            tb->SelectionLength = 0;
                    }
                }
            }
            return false;  // сообщение не блокируем
        }

    private:
        array<System::Windows::Forms::TextBox^>^ _fields;
    };

}
