#ifdef _WIN32

#include "service_installer/Platform.hpp"
#include <windows.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>
#include <glog/logging.h>



namespace svcinst::platform {

    namespace fs = std::filesystem;
    //------------------------------------------------------------
    //  Safety: не дать снести "опасные" пути типа C:\ или корня
    //------------------------------------------------------------
    static bool isDangerousPath(const fs::path& p)
    {
        //---Проверка на пустой путь
        if (p.empty()) return true;

        std::error_code ec;
        //---Получение абсолютного пути
        fs::path abs = fs::absolute(p, ec);

        //---Если не удалось получить абсолютный путь (например, нет прав доступа)
        if (ec) abs = p;    // Используем исходныйпуть как есть

        //---Проверка на корневой диск(C:\, D:\)
        //
        //---Получаем корневую часть пути
        const fs::path root = abs.root_path();

        //---Если корень не пустой и весь путь равен корню 
        if (!root.empty() && abs == root) return true; 

        //---Проверка на короткий путь(не более 3 символов)
        //
        //---Получаем путь как широкую строку (для Windows)
        const auto w = abs.wstring();

        //---Размер 3 символа или меньше: "C:\" (3 символа), "C:" (2 символа) и т.д.
        if (w.size() <= 3) return true; 

        return false;
    }

    //------------------------------------------------------------
    //  Снять ReadOnly/Hidden/System (часто мешают remove_all на Win)
    //------------------------------------------------------------
    static void makeNormalAttributes(const fs::path& p)
    {
        //---Преобразуем путь в широкую строку для WinApi
        const std::wstring w = p.wstring();

        //---Получаем текущие атрибуты файла/папки
        DWORD attr = GetFileAttributesW(w.c_str());

        //---Если не удалось получить атрибуты (файл не существует или нет доступа)
        if (attr == INVALID_FILE_ATTRIBUTES) return;    // INVALID_FILE_ATTRIBUTES = 0xFFFFFFFF

        //---Создаем новые атрибуты, убирая "особые" флаги
        DWORD newAttr = attr;
        newAttr &= ~FILE_ATTRIBUTE_READONLY;
        newAttr &= ~FILE_ATTRIBUTE_HIDDEN;
        newAttr &= ~FILE_ATTRIBUTE_SYSTEM;
        
        //---Если атрибуты изменились, применяем новые
        if (newAttr != attr)
        {
            (void)SetFileAttributesW(w.c_str(), newAttr);
        }
    }
    //------------------------------------------------------------
    //  Рекурсивная функция очистки атрибутов файлов и папок
    //------------------------------------------------------------
    static void clearAttributesRecursive(const fs::path& root)
    {
        std::error_code ec;

        //---Проверяем, существует ли корневой путь
        if (!fs::exists(root, ec)) return;

        //---Обработка корневой директории
        makeNormalAttributes(root);

        //---Настройка итератора для рекурсивного обхода
        // 
        
        fs::directory_options opt = fs::directory_options::skip_permission_denied;  //  Пропускать объекты без доступа
        fs::recursive_directory_iterator it(root, opt, ec);                         //  Создаем рекурсивный итератор для обхода всех вложенных элементов
        fs::recursive_directory_iterator end;                                       //  Конечный итератор (по умолчанию пустой)
        
        //---Рекурсивный обход всех файлов и папок
        //   Цикл продолжается пока нет ошибок и не достигнут конец
        while (!ec && it != end)
        {
            //---Обрабатываем текущий путь
            makeNormalAttributes(it->path());
            it.increment(ec);
        }
    }

    //------------------------------------------------------------
    //  Попытка удалить сразу
    //------------------------------------------------------------
    static bool removeTreeNow(const fs::path& dir, std::string* error)
    {
        //---Проверка на пустой путь
        if (dir.empty()) return true;

        //---Проверка на опасные пути (системные, корневые и т.д.)
        if (isDangerousPath(dir))
        {
            if (error)
            {
                *error = "Refuse to delete dangerous path: " + dir.string();
                LOG(ERROR) << "Refuse to delete dangerous path: " + dir.string();
            }
            return false;   // Отказываемся удалять опасный путь
        }

        std::error_code ec;
        //---Проверка существования директории
        if (!fs::exists(dir, ec)) return true; // Если не существует - считаем успехом, делает функцию идемпотентной

        //---Очистка атрибутов файлов перед удалением
        clearAttributesRecursive(dir);

        ec.clear();

        //---Рекурсивное удаление директории
        (void)fs::remove_all(dir, ec);
        //---Проверяем, не было ли ошибок
        if (!ec) return true;

        //---Обработка ошибки удаления
        if (error)
        {
            //  Формируем детальное сообщение об ошибке:
            //  - путь, который не удалось удалить
            //  - сообщение об ошибке от системы
            *error = "remove_all failed for '" + dir.string() + "': " + ec.message();
            LOG(ERROR) << "remove_all failed for '" + dir.string() + "': " + ec.message();
        }
        return false;
    }

    //------------------------------------------------------------
    //  Отложенное удаление: cmd.exe в фоне
    //------------------------------------------------------------
    static fs::path cmdExePath()
    {
        //---Буфер для системной директории
        wchar_t sysDir[MAX_PATH]{};

        //---Получаем системную директорию
        UINT n = GetSystemDirectoryW(sysDir, MAX_PATH);

        //---Если ошибка или путь слишком длинный, вернем "cmd.exe" (ищет в PATH)
        if (n == 0 || n >= MAX_PATH) return fs::path(L"cmd.exe");
        
        //---Формируем полный путь: C:\Windows\System32\cmd.exe
        return fs::path(sysDir) / L"cmd.exe";
    }
    //-----------------------------------------------------------
    //  Функция оборачивания строки в кавычки 
    //  для командной строки
    //-----------------------------------------------------------
    static std::wstring q(const std::wstring& s)
    {
        return L"\"" + s + L"\"";
    }
    //-----------------------------------------------------------
    //  Отложенное удаление через cmd.exe 
    //  (после завершения текущего процесса)
    //-----------------------------------------------------------
    static bool deferRemoveWithCmd(
        const fs::path& installDir,
        const fs::path& dataRoot,
        std::string* error
    )
    {
        //---Формируем внутреннюю команду для cmd.exe
        // timeout - ждем 2 секунды
        // /t 2 - время в секундах
        // /nobreak - игнорировать нажатия клавиш
        // >nul - перенаправляем вывод в никуда (не показывать в консоли)
        std::wstring inner = L"timeout /t 2 /nobreak >nul";

        //---Добавляем удаление директории установки, если она не опасная и не пустая
        // & - выполнить следующую команду после предыдущей
        // rmdir /s /q - удалить директорию рекурсивно (/s) без запросов (/q)
        // q() - оборачивает путь в кавычки (на случай пробелов)
        if (!installDir.empty() && !isDangerousPath(installDir))
        {
            inner += L" & rmdir /s /q " + q(installDir.wstring());
        }
            
        //---Добавляем удаление директории данных, если она не опасная и не пустая
        if (!dataRoot.empty() && !isDangerousPath(dataRoot))
        {
            inner += L" & rmdir /s /q " + q(dataRoot.wstring());
        }
            
        //---Формируем полную командную строку для запуска cmd.exe, "C:\Windows\System32\cmd.exe" /C ""timeout ... & rmdir ... ""
        std::wstring cmdLine = q(cmdExePath().wstring()) + L" /C \"\"" + inner + L"\"\"";

        //---Подготовка буфера для CreateProcessW
        std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
        buf.push_back(L'\0');

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        //---Создаем процесс
        BOOL ok = CreateProcessW(
            nullptr,
            buf.data(),
            nullptr, nullptr,
            FALSE,
            CREATE_NO_WINDOW | DETACHED_PROCESS,
            nullptr,
            nullptr,
            &si,
            &pi
        );
        //---Обработка ошибки создания процесса
        if (!ok)
        {
            if (error)
            {
                *error = "CreateProcessW(cmd.exe) failed, error=" + std::to_string((int)GetLastError());
                LOG(ERROR) << "CreateProcessW(cmd.exe) failed, error=" + std::to_string((int)GetLastError());
            }
            return false;
        }
        //---Освобождение дескрипторов (важно для избежания утечек)
        CloseHandle(pi.hThread);    // Дескриптор основного потока процесса
        CloseHandle(pi.hProcess);   // Дескриптор самого процесса

        return true;
    }

    //------------------------------------------------------------
    //  Публичные функции из Platform.hpp
    //------------------------------------------------------------

    //------------------------------------------------------------
    //  Удаление директории установки приложения
    //------------------------------------------------------------
    bool removeInstallDir(const fs::path& installDir, std::string* error,bool fromInno)
    {
        //---Если удаление выполняется из InnoSetup
        if (fromInno) return true; // Inno сам удалит {app}, а helper не должен удалять installDir

        std::string err;

        //---Пытаемся удалить директорию немедленно
        if (removeTreeNow(installDir, &err)) return true;

        //---Если немедленное удаление не удалось, пробуем отложенное удаление
        std::string err2;
        if (deferRemoveWithCmd(installDir, fs::path{}, &err2)) return true;
        
        //---Если оба метода не сработали, формируем объединенное сообщение об ошибке
        if (error) 
        {
            *error = err + "; defer failed: " + err2;
        }
        return false;
    }
    //------------------------------------------------------------
    //  Удаление директории с данными приложения
    //------------------------------------------------------------
    bool removeDataRoot(const fs::path& dataRoot, std::string* error)
    {
        std::string err;
        //---Пытаемся удалить директорию немедленно
        if (removeTreeNow(dataRoot, &err)) return true;

        std::string err2;
        //---Если немедленное удаление не удалось, пробуем отложенное удаление
        if (deferRemoveWithCmd(fs::path{}, dataRoot, &err2)) return true;

        //---Если оба метода не сработали, формируем объединенное сообщение об ошибке
        if (error)
        {
            *error = err + "; defer failed: " + err2;
        }
        return false;
    }
} // namespace svcinst::platform

#endif // _WIN32
