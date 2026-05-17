#pragma once

// =====================================================================
// Localization — менеджер строк интерфейса (managed-класс).
//
// Загружает строки из embedded resource (.resources) в зависимости от
// Thread::CurrentThread->CurrentUICulture (которая берётся .NET'ом из
// языка Windows автоматически).
//
// Нейтральная локаль — русская (Strings.resources). Сейчас поддержан
// английский (Strings.en.resources); для добавления нового языка —
// положить Strings.<lang>.resx в src/ и добавить ветку в Load().
//
// Предпочтение пользователя (Auto / Russian / English) сохраняется в
// %APPDATA%\WinJoyTweaker\settings.ini в виде «language=auto|ru|en».
// =====================================================================

namespace WinJoytweaker {

    public ref class Localization abstract sealed
    {
    public:
        // Порядок важен: значения совпадают с индексами combobox-а языка.
        enum class Language { Auto = 0, Russian = 1, English = 2 };

        // Возвращает строку по ключу. Если ключ не найден — возвращает "[key]".
        static System::String^ T(System::String^ key);

        // Формат с подстановкой: T("status.devicesConnected", count).
        static System::String^ T(System::String^ key, ... array<System::Object^>^ args);

        // Загружает предпочтение из settings.ini; если файла нет — Auto.
        static Language LoadPreference();

        // Сохраняет предпочтение в settings.ini.
        static void SavePreference(Language pref);

        // Устанавливает CurrentUICulture согласно предпочтению (Auto = культура,
        // установленная в Windows как UI-язык) и сбрасывает кеш строк, чтобы
        // следующий T() перечитал нужный resx.
        static void ApplyPreference(Language pref);

    private:
        // Managed-статика инициализируется прямо в декларации — C++/CLI не
        // позволяет определять её в .cpp как обычный C++ static.
        static System::Collections::Generic::Dictionary<System::String^, System::String^>^ _strings = nullptr;
        static bool _loaded = false;

        static void EnsureLoaded();
        static System::String^ GetSettingsPath();
    };
}
