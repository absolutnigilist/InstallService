#include "service_installer/Process.hpp"
#include "platform/ProcessImpl.hpp"

namespace svcinst::process {

// Реализация публичной функции запуска процесса
// Выступает в качестве оболочки для платформенно-специфичной реализации
// Параметры:
//   exe - путь к исполняемому файлу
//   args - аргументы командной строки для передачи процессу
//   out - структура для записи результатов выполнения (передается по ссылке)
//   opt - опции запуска процесса
// Возвращает:
//   Результат вызова платформенно-специфичной реализации (true/false)
// Примечание:
//   Эта функция делегирует выполнение функции detail::runPlatform(),
//   которая содержит реальную реализацию для конкретной операционной системы
//   (Windows/Linux)
    bool run(const fs::path& exe, const std::vector<std::string>& args,
        RunResult& out, const RunOptions& opt)
    {
        return detail::runPlatform(exe, args, out, opt);
    }

} // namespace svcinst::process
