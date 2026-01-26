#pragma once
#include "service_installer/Process.hpp"

namespace svcinst::process::detail {

// Платформенно-специфичная реализация запуска процесса
// Определяется в соответствующих файлах реализации:
//   - ProcessWin.cpp для Windows
//   - ProcessLinux.cpp для Linux
// Параметры:
//   exe - путь к исполняемому файлу
//   args - аргументы командной строки для передачи процессу
//   out - структура для записи результатов выполнения (передается по ссылке)
//   opt - опции запуска процесса
// Возвращает:
//   true - если процесс успешно запущен и завершился (независимо от exitCode)
//   false - если произошла ошибка при запуске процесса
// Примечание:
//   Эта функция должна быть реализована для каждой поддерживаемой платформы
//   и учитывать особенности каждой операционной системы
    // Platform-specific implementation (defined in ProcessWin.cpp / ProcessLinux.cpp)
    bool runPlatform(const fs::path& exe, const std::vector<std::string>& args,
        RunResult& out, const RunOptions& opt);

} // namespace svcinst::process::detail
