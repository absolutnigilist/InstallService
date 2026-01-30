#ifdef _WIN32

#include "platform/ProcessImpl.hpp"
#include <windows.h>
#include <vector>
#include <glog/logging.h>

namespace svcinst::process::detail {

	//--- Преобразование строки UTF-8 в широкую строку (UTF-16) для Windows API
    static std::wstring utf8ToWide(const std::string& s)
    {
        //---Проверка на пустую строку
        if (s.empty()) return {};
        //---Выходной буфер пустой. получаем требуемое число wide-символов
        int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), (int)s.size(), nullptr, 0);
        if (n == 0)
        {
            LOG(ERROR) << "Failed to get required buffer size for UTF - 8 to wide conversion";
        }
        //---Инициализируем широкую строку широкими символами \0
        std::wstring w((size_t)n, L'\0');
        //---Пишем широкие символы
        int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), (int)s.size(), w.data(), n);
        if (written == 0)
        {
            LOG(WARNING) << "MultiByteToWideChar returned 0, but buffer was allocated";
        }
        return w;
    }

    //---Корректное quoting аргументов под CreateProcess (правило backslashes+quotes)
    static std::wstring quoteWindowsArg(std::wstring_view arg)
    {
        //---Кавычки нужны если пустой аргумент, есть пробелы, табуляции, переводы строк или кавычки
        const bool needQuotes =
            arg.empty() || (arg.find_first_of(L" \t\n\v\"") != std::wstring_view::npos);

        //---Если кавычки не нужны, возвращаем строку как есть
        if (!needQuotes) return std::wstring(arg);

        //---Резервируем память (аргумент + 2 кавчки)
        std::wstring out;
        out.reserve(arg.size() + 2);
        out.push_back(L'"');    // открывающая кавычка

        //---Счетчик последовательных обратных слешей
        std::size_t bsCount = 0;
        for (wchar_t ch : arg)
        {
            //---Если символ обратный слеш
            if (ch == L'\\')
            {
                ++bsCount;                  //  увеличиваем счетчик обратных слешей
                out.push_back(L'\\');       // добавляем слеш в выходную строку
                continue;
            }
            //---Если символ кавычка
            if (ch == L'"')
            {
                //--удвоить backslash'и перед кавычкой и экранировать кавычку
                out.append(bsCount, L'\\'); //  удваиваем все слеши перед кавычкой
                bsCount = 0;                //  сбрасываем счетчик
                out.push_back(L'\\');       //  добавляем экранирущий слеш
                out.push_back(L'"');        //  добавляем саму кавычку
                continue;
            }
            //---Любой другой символ
            bsCount = 0;                    //  сбрасываем счетчик слешей
            out.push_back(ch);              //  добавляем символ как есть
        }

        //---Закрывабщая кавычка 
        out.append(bsCount, L'\\');         //  удваиваем слеши перед закрывающей кавычкой
        out.push_back(L'"');                //  закрывающая кавычка
        return out;
    }
	//---Построение командной строки для CreateProcess
    static std::wstring buildCommandLine(const fs::path& exe, const std::vector<std::string>& args)
    {
        std::wstring cmd = quoteWindowsArg(exe.wstring());
        for (const auto& a : args)
        {
            cmd.push_back(L' ');
            cmd += quoteWindowsArg(utf8ToWide(a));
        }
        return cmd;
    }
	//---Платформенно-специфичная реализация запуска процесса для Windows
    bool runPlatform(const fs::path& exe, const std::vector<std::string>& args, RunResult& out, const RunOptions& opt)
    {
        out = {};

        //---Собираем командную строку из пути к исполняемому файлу и аргументов
        const std::wstring cmdLine = buildCommandLine(exe, args);

        STARTUPINFOW si{};
        si.cb = sizeof(si);

        PROCESS_INFORMATION pi{};

        //---Подготовка буфера командной строки
        std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
        buf.push_back(L'\0');

        //---Настройка параметров запуска
        const DWORD flags = opt.hideWindow ? CREATE_NO_WINDOW : 0;                          // опция без окна консоли
        const std::wstring cwdW = opt.workingDir.empty() ? L"" : opt.workingDir.wstring();  // рабочая директория ->wide-строка
        const wchar_t* cwdPtr = opt.workingDir.empty() ? nullptr : cwdW.c_str();

        //---Создание процесса
        BOOL ok = CreateProcessW(
            exe.wstring().c_str(), // Имя исполняемого файла
            buf.data(),            // Командная строка (mutable)
            nullptr, nullptr,      // Атрибуты бещпасности
            FALSE,                 // Без наследования дескрипторов
            flags,                 // Флаг создания окна консоли
            nullptr,               // Переменные окружения(наследовать от родителя)
            cwdPtr,                // Рабочая директория(nullptr = текущая директория)
            &si,                   // Информауия о запускаемом процессе
            &pi                    // Заполнится при успехе
        );

        //---Обработка ошибки создания процесса
        if (!ok)
        {
            out.started = false;                // Процесс не запущен
            out.sysError = GetLastError();      // Получаем код системной ошибки
            out.exitCode = (int)out.sysError;   // Сохраняем код системной ошибки как exitCode
            LOG(ERROR) << "Failed to create process " << exe << " with error: " << out.sysError;
            return false;
        }

        out.started = true;         // старт успешный
        CloseHandle(pi.hThread);    // закрываем дексриптор основного потока

        //---Устанавливае таймаут ожидания
        const DWORD timeout = (opt.timeoutMs == 0) ? INFINITE : opt.timeoutMs;
        //---Ожидаем завершения процесса с таймаутом
        DWORD waitRes = WaitForSingleObject(pi.hProcess, timeout);
        //---Обработка таймаута
        if (waitRes == WAIT_TIMEOUT)
        {
            out.timedOut = true;                    // Процесс превысил лимит времени
            TerminateProcess(pi.hProcess, 1);       // Принудительно завершаем процесс с кодом
            WaitForSingleObject(pi.hProcess, 5000); // 5 секунд на завершение
        }
        //---Обработка ошибки ожидания
        else if (waitRes == WAIT_FAILED)
        {
            out.sysError = GetLastError();      // Сохраняем код ошибки
            CloseHandle(pi.hProcess);           // Закрываем дескриптор процесса
            out.exitCode = (int)out.sysError;   // Сохраняем код ошибки как exitCode
            LOG(ERROR) << "WaitForSingleObject failed for process " << exe << " with error " << out.sysError << " (process handle: " << pi.hProcess << ")"; 
            return false;
        }
        //---Получение кода завершения процесса
        DWORD code = 0;
        //---Запрашиваем код завершения
        if (!GetExitCodeProcess(pi.hProcess, &code))
        {
            out.sysError = GetLastError();      // Если не удалось получить код
            CloseHandle(pi.hProcess);           // Закрывае дексриптор
            out.exitCode = (int)out.sysError;   // Сохраняем код ошибки
            LOG(ERROR) << "Failed to get exit code for process " << exe << " with error " << out.sysError << ". Process may have terminated abnormally.";
            return false;
        }

        //---Закрываем декриптор процесса
        CloseHandle(pi.hProcess);
        //---Сохраняем код завершения
        out.exitCode = (int)code;
        return true;
    }

} // namespace svcinst::process::detail
#endif
