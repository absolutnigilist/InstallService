#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace svcinst::process {

    namespace fs = std::filesystem;

    struct RunOptions final {
        fs::path workingDir;          // Рабочий каталог для запускаемого процесса (опционально)
        std::uint32_t timeoutMs = 0;  // Таймаут выполнения в миллисекундах, 0 - бесконечно
        bool hideWindow = true;       // Скрыть окно консоли (Windows: CREATE_NO_WINDOW, Linux: игнорируется)
    };

    struct RunResult final {
        bool started = false;         // Успешно ли запущен процесс (true - да, false - ошибка запуска)
        bool timedOut = false;        // Был ли превышен таймаут выполнения (true - да)
        int exitCode = 0;             // Код завершения процесса (или синтетический код при ошибках)
        std::uint32_t sysError = 0;   // Код системной ошибки (GetLastError() на Windows или errno на Linux)
    };
    //---Запускает внешний процесс с заданными параметрами
    // 
    // Параметры:
    //   exe - путь к исполняемому файлу
    //   args - аргументы командной строки для передачи процессу
    //   out - структура для записи результатов выполнения (передается по ссылке)
    //   opt - опции запуска процесса (по умолчанию пустые)
    // Возвращает:
    //   true - если процесс успешно запущен и завершился (независимо от exitCode)
    //   false - если произошла ошибка при запуске процесса
    // Примечание:
    //   Функция блокирующая - ожидает завершения процесса или истечения таймаута
    bool run(const fs::path& exe, const std::vector<std::string>& args,
        RunResult& out, const RunOptions& opt = {});

} // namespace svcinst::process
