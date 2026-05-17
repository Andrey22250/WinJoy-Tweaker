#pragma once
#include <Windows.h>
#include <string>
#include <vector>

// =====================================================================
// BackupManager — Safety Manager проекта.
//
// Отвечает за создание .reg-файлов с текущим состоянием параметров
// устройства (OEMName/OEMData) перед записью в реестр и за разбор
// .reg-файлов при восстановлении. Не знает про RegistryEngine —
// работает только с файлами на диске и переданными байтами.
// =====================================================================

// Результат разбора .reg-файла бэкапа.
struct BackupParseResult {
    std::wstring        oemName;
    std::vector<BYTE>   oemDataRaw;
    bool                valid = false;
    std::wstring        errorMessage;
};

namespace BackupManager {

    // Максимальное число .reg-файлов в папке бэкапов; старые удаляются.
    constexpr size_t MAX_BACKUPS = 15;

    // Создаёт %APPDATA%\WinJoyTweaker\backups и возвращает путь,
    // либо пустую строку при ошибке.
    std::wstring EnsureBackupDir();

    // Сохраняет OEMName и OEMData в .reg-файл (UTF-16 LE + BOM, формат
    // «Windows Registry Editor Version 5.00»). Возвращает путь к файлу
    // или "" при ошибке. После записи ротирует папку (max MAX_BACKUPS файлов).
    //
    // Параметры:
    //   oemKey   — имя OEM-подраздела (например "VID_046D&PID_C29B")
    //   oemName  — текущее значение OEMName (REG_SZ)
    //   rawBytes — сырые байты OEMData (REG_BINARY)
    //   outDir   — директория для сохранения; если пусто — EnsureBackupDir()
    std::wstring WriteBackup(const std::wstring& oemKey,
                             const std::wstring& oemName,
                             const std::vector<BYTE>& rawBytes,
                             const std::wstring& outDir = L"");

    // Парсит .reg-файл бэкапа: UTF-16 LE с BOM (формат «Version 5.00»)
    // или ANSI (REGEDIT4). Учитывает экранирование \\ и \" в OEMName
    // и перенос hex-блока через '\' в конце строки.
    BackupParseResult ParseBackupFile(const std::wstring& filePath);
}
