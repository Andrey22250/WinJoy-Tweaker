#pragma once
#include <Windows.h>
#include <string>
#include <vector>

#include "RegistryEngine.h"   // NamedEntry

// =====================================================================
// BackupManager — Safety Manager проекта.
//
// Отвечает за создание .reg-файлов со ВСЕМ поддеревом устройства
// (OEMName, OEMData, Axes\*, Buttons\*, OEMForceFeedback\*) перед записью
// в реестр и за разбор .reg-файлов при восстановлении.
// =====================================================================

// Результат разбора .reg-файла бэкапа.
struct BackupParseResult {
    std::wstring             oemName;
    std::vector<BYTE>        oemDataRaw;
    std::vector<NamedEntry>  axes;          // имена осей из подключей Axes\<N>
    std::vector<NamedEntry>  buttons;       // имена кнопок из подключей Buttons\<N>
    bool                     valid = false;
    std::wstring             errorMessage;
};

namespace BackupManager {

    // Максимальное число .reg-файлов в папке бэкапов; старые удаляются.
    constexpr size_t MAX_BACKUPS = 15;

    // Создаёт %APPDATA%\WinJoyTweaker\backups и возвращает путь,
    // либо пустую строку при ошибке.
    std::wstring EnsureBackupDir();

    // Сохраняет в .reg-файл (UTF-16 LE + BOM, формат «Windows Registry Editor
    // Version 5.00») всё поддерево HKCU\...\OEM\<oemKey>: значения корневого
    // ключа (OEMName/OEMData/...) и все подключи рекурсивно (Axes, Buttons,
    // OEMForceFeedback). Читает свежее состояние из реестра.
    //
    // Возвращает путь к файлу или "" при ошибке. После записи ротирует папку
    // бэкапов (max MAX_BACKUPS файлов).
    std::wstring WriteBackup(const std::wstring& oemKey,
                             const std::wstring& outDir = L"");

    // Парсит .reg-файл бэкапа: UTF-16 LE с BOM (формат «Version 5.00»)
    // или ANSI (REGEDIT4). Извлекает OEMName, OEMData и имена @ в подключах
    // Axes\<N> / Buttons\<N>. Учитывает экранирование \\ и \" в строках
    // и перенос hex-блока через '\' в конце строки.
    BackupParseResult ParseBackupFile(const std::wstring& filePath);
}
