#include "Localization.h"

using namespace System;
using namespace System::Collections;
using namespace System::Collections::Generic;
using namespace System::Globalization;
using namespace System::IO;
using namespace System::Reflection;
using namespace System::Resources;
using namespace System::Threading;
using namespace System::Windows::Forms;  // ResXResourceReader (fallback для .resx)

namespace WinJoytweaker {

// Ищет имя embedded resource, оканчивающееся на нужный суффикс.
// Возвращает nullptr, если не найдено. Работает независимо от того,
// положил ли MSBuild ресурс с расширением .resources (нормальный путь)
// или с .resx (если LogicalName заблокировал перекодирование).
static String^ FindResourceByEnding(Assembly^ asm_, String^ suffixWithoutExt)
{
    array<String^>^ names = asm_->GetManifestResourceNames();
    // Сначала пробуем «правильный» вариант — .resources (бинарный формат).
    for each (String^ n in names) {
        if (n->EndsWith(suffixWithoutExt + L".resources",
                        StringComparison::OrdinalIgnoreCase))
            return n;
    }
    // Fallback — .resx (XML).
    for each (String^ n in names) {
        if (n->EndsWith(suffixWithoutExt + L".resx",
                        StringComparison::OrdinalIgnoreCase))
            return n;
    }
    return nullptr;
}

// Перекладывает пары "ключ → строка" из enumerator-а в словарь.
static void DrainEnumerator(IDictionaryEnumerator^ en,
                            Dictionary<String^, String^>^ dst)
{
    while (en->MoveNext()) {
        String^ key = safe_cast<String^>(en->Key);
        String^ val = dynamic_cast<String^>(en->Value);
        if (val != nullptr) dst[key] = val;
    }
}

// Читает один ресурс по имени; пробует ResourceReader (binary), а если
// формат XML — ResXResourceReader. Возвращает true при успехе.
static bool ReadResourceInto(Assembly^ asm_, String^ resourceName,
                             Dictionary<String^, String^>^ dst)
{
    Stream^ stream = asm_->GetManifestResourceStream(resourceName);
    if (stream == nullptr) return false;

    bool isXml = resourceName->EndsWith(L".resx", StringComparison::OrdinalIgnoreCase);
    try {
        if (isXml) {
            ResXResourceReader^ reader = gcnew ResXResourceReader(stream);
            try { DrainEnumerator(reader->GetEnumerator(), dst); }
            finally { delete reader; }
        } else {
            ResourceReader^ reader = gcnew ResourceReader(stream);
            try { DrainEnumerator(reader->GetEnumerator(), dst); }
            finally { delete reader; }
        }
        return true;
    } catch (Exception^) {
        // На случай, если ResourceReader получил XML — пробуем как ResX.
        if (!isXml) {
            try {
                stream->Position = 0;
                ResXResourceReader^ reader = gcnew ResXResourceReader(stream);
                try { DrainEnumerator(reader->GetEnumerator(), dst); }
                finally { delete reader; }
                return true;
            } catch (Exception^) {}
        }
        return false;
    } finally {
        delete stream;
    }
}

void Localization::EnsureLoaded()
{
    if (_loaded) return;
    _loaded = true;
    _strings = gcnew Dictionary<String^, String^>();

    Assembly^ asm_ = Assembly::GetExecutingAssembly();
    String^ langCode = Thread::CurrentThread->CurrentUICulture->TwoLetterISOLanguageName;

    // Сначала загружаем нейтральный (русский) набор — он работает как fallback,
    // если в выбранном языке какие-то ключи отсутствуют.
    String^ neutralName = FindResourceByEnding(asm_, L".Strings");
    if (neutralName != nullptr)
        ReadResourceInto(asm_, neutralName, _strings);

    // Затем накладываем поверх локаль-специфичные строки.
    // Имя файла — Strings_en.resx (с подчёркиванием, а не точкой), потому что
    // MSBuild распознаёт точку перед двухбуквенным кодом как культуру и
    // пытается собрать satellite assembly, которая в vcxproj C++/CLI не работает.
    if (langCode == L"en") {
        String^ enName = FindResourceByEnding(asm_, L".Strings_en");
        if (enName != nullptr)
            ReadResourceInto(asm_, enName, _strings);
    }
}

String^ Localization::T(String^ key)
{
    EnsureLoaded();
    String^ value;
    if (_strings->TryGetValue(key, value))
        return value;
    return L"[" + key + L"]";  // явный маркер пропущенного ключа
}

String^ Localization::T(String^ key, ... array<Object^>^ args)
{
    return String::Format(T(key), args);
}

// =====================================================================
// Хранение предпочтения языка
// =====================================================================

String^ Localization::GetSettingsPath()
{
    String^ appData = Environment::GetFolderPath(Environment::SpecialFolder::ApplicationData);
    return Path::Combine(appData, L"WinJoyTweaker", L"settings.ini");
}

// Читает весь settings.ini в словарь "ключ(в нижнем регистре) → значение".
// Пустой словарь, если файла нет или чтение упало.
static Dictionary<String^, String^>^ ReadSettings(String^ path)
{
    auto kv = gcnew Dictionary<String^, String^>(StringComparer::OrdinalIgnoreCase);
    try {
        if (!File::Exists(path)) return kv;
        for each (String^ line in File::ReadAllLines(path)) {
            String^ s = line->Trim();
            int eq = s->IndexOf(L'=');
            if (eq <= 0) continue;
            kv[s->Substring(0, eq)->Trim()] = s->Substring(eq + 1)->Trim();
        }
    } catch (...) {}
    return kv;
}

// Перезаписывает settings.ini всеми парами из словаря. Merge-aware вызывающий
// сначала читает существующие ключи через ReadSettings, меняет нужный и пишет
// весь набор — так сохранение одного параметра не затирает другие.
static void WriteSettings(String^ path, Dictionary<String^, String^>^ kv)
{
    try {
        String^ dir = Path::GetDirectoryName(path);
        if (!Directory::Exists(dir)) Directory::CreateDirectory(dir);

        System::Text::StringBuilder^ sb = gcnew System::Text::StringBuilder();
        for each (KeyValuePair<String^, String^> p in kv)
            sb->Append(p.Key)->Append(L'=')->Append(p.Value)->Append(L"\r\n");
        File::WriteAllText(path, sb->ToString());
    } catch (...) {
        // Не критично: если не получилось сохранить — просто не запомним выбор.
    }
}

Localization::Language Localization::LoadPreference()
{
    auto kv = ReadSettings(GetSettingsPath());
    String^ value;
    if (kv->TryGetValue(L"language", value)) {
        value = value->ToLowerInvariant();
        if (value == L"ru") return Language::Russian;
        if (value == L"en") return Language::English;
    }
    return Language::Auto;
}

void Localization::SavePreference(Language pref)
{
    String^ path = GetSettingsPath();
    auto kv = ReadSettings(path);   // сохраняем прочие ключи (onlyConnected и т.п.)
    switch (pref) {
        case Language::Russian: kv[L"language"] = L"ru";   break;
        case Language::English: kv[L"language"] = L"en";   break;
        default:                kv[L"language"] = L"auto"; break;
    }
    WriteSettings(path, kv);
}

bool Localization::LoadShowOnlyConnected()
{
    auto kv = ReadSettings(GetSettingsPath());
    String^ value;
    if (kv->TryGetValue(L"onlyConnected", value)) {
        value = value->ToLowerInvariant();
        // Любое явное «выкл» снимает фильтр; всё прочее (и отсутствие) — true.
        if (value == L"false" || value == L"0") return false;
    }
    return true;
}

void Localization::SaveShowOnlyConnected(bool onlyConnected)
{
    String^ path = GetSettingsPath();
    auto kv = ReadSettings(path);   // сохраняем прочие ключи (language)
    kv[L"onlyConnected"] = onlyConnected ? L"true" : L"false";
    WriteSettings(path, kv);
}

void Localization::ApplyPreference(Language pref)
{
    CultureInfo^ culture;
    switch (pref) {
        case Language::Russian: culture = gcnew CultureInfo(L"ru"); break;
        case Language::English: culture = gcnew CultureInfo(L"en"); break;
        default:                culture = CultureInfo::InstalledUICulture; break;
    }

    Thread::CurrentThread->CurrentUICulture = culture;
    // Также ставим в качестве дефолта для будущих потоков (на всякий случай).
    CultureInfo::DefaultThreadCurrentUICulture = culture;

    // Сбрасываем кеш — следующий вызов T() перечитает нужный resx.
    _loaded  = false;
    _strings = nullptr;
}

} // namespace WinJoytweaker
