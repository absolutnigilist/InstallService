#ifdef _WIN32

#include "service_installer/IServiceBackend.hpp"
#include "service_installer/Process.hpp"

#include <windows.h>
#include <filesystem>
#include <memory>
#include <sstream>
#include <glog/logging.h>


namespace svcinst {

    namespace fs = std::filesystem;
    //------------------------------------------------------------
	//  Путь к sc.exe
    //------------------------------------------------------------
    static fs::path scExePath()
    {
        wchar_t sysDir[MAX_PATH]{};
        UINT n = GetSystemDirectoryW(sysDir, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return fs::path(L"sc.exe");
        return fs::path(sysDir) / L"sc.exe";
    }
    //------------------------------------------------------------
	//  Проверка, что код выхода входит в список допустимых
    //------------------------------------------------------------
    static bool scOkExit(int code, std::initializer_list<int> ok)
    {
        for (int v : ok) if (code == v) return true;
        return false;
    }
    //------------------------------------------------------------
    //  Обёртка над sc.exe
    //------------------------------------------------------------
    static bool runSc(
        const std::vector<std::string>& args,       //  Аргументы командной строки для sc.exe
        std::initializer_list<int> okExitCodes,     //  Список допустимых кодов завершения
        std::string* error,                         //  Указатель для возврата ошибки
        const char* what)                           //  Описание операции для логирования
    {
        process::RunResult rr;
        //---Запускаем sc.exe с переданными аргументами и скрытым окном
        const bool started = process::run(
            scExePath(),                                // Получаем путь к sc.exe
            args,                                       // Аргументы командной строки
            rr,                                         // Результат выполнения
            process::RunOptions{ .hideWindow = true }   // Окно скрыто
        );
        //---Проверяем, удалось ли запустить процесс
        if (!started || !rr.started)
        {
            if (error)  // Если передан указатель для ошибки
            {
                std::ostringstream os;
                os << what << ": failed to start sc.exe. sysError=" << rr.sysError;
                *error = os.str();
                //---Логируем ошибку запуска с системным кодом ошибки
                LOG(ERROR) << "Failed to start sc.exe. sysError=" << rr.sysError;
            }
            return false;
        }
        //---Проверяем код завершения sc.exe
        if (!scOkExit(rr.exitCode, okExitCodes))
        {
            if (error)
            {
                std::ostringstream os;
                os << what << ": sc.exe exitCode=" << rr.exitCode;
                *error = os.str();
                //---Логируем код завершения sc.exe
                LOG(ERROR) << "sc.exe exitCode = " << rr.exitCode;
            }
            return false;
        }

        return true;
    }
    //------------------------------------------------------------
	//  Проверка существования службы
    //------------------------------------------------------------
    static bool serviceExists(
        const std::string& name,    // Имя службы для проверки
        bool& exists,               // Выходной параметр: существует ли служба
        std::string* error          // Указатель для возврата сообщения об ошибке
    )
    {
        process::RunResult rr;
       
        //---Запускаем команду sc.exe query <имя_службы> для проверки службы
        const bool started = process::run(
            scExePath(),                                // Получаем полный путь к sc.exe
            { "query", name },                          // Аргументы: команда query и имя службы
            rr,                                         // Куда записать результат выполнения
            process::RunOptions{ .hideWindow = true }   // Окно скрыто
        );
        //---Проверяем, удалось ли запустить процесс sc.exe
        if (!started || !rr.started)
        {
            if (error) // Если передан указатель для ошибки
            {
                std::ostringstream os;
                os << "sc query: failed to start sc.exe. sysError=" << rr.sysError;
                *error = os.str();
                //---Логируем код завершения sc.exe
                LOG(ERROR) << "sc query: failed to start sc.exe. sysError=" << rr.sysError;
            }
            return false;
        }
		//---Проверяем код завершения sc.exe
        // 
        //---Код 0 означает успешное выполнение - служба существует
		if (rr.exitCode == 0)
		{
			exists = true;  // Устанавливаем выходной параметр exists = true
			return true;
		}
        //---Проверяем специфический код ошибки "служба не существует"
        //   ERROR_SERVICE_DOES_NOT_EXIST - системная константа Windows (обычно 1060)
		if (rr.exitCode == (int)ERROR_SERVICE_DOES_NOT_EXIST)
		{
			exists = false; // Устанавливаем выходной параметр exists = false
			return true;
		}
        //---Если код завершения не 0 и не ERROR_SERVICE_DOES_NOT_EXIST,
        //   неизвестная ошибка
        if (error)
        {
            std::ostringstream os;
            os << "sc query: unexpected exitCode=" << rr.exitCode;
            *error = os.str();
            LOG(ERROR) << "sc query: unexpected exitCode=" << rr.exitCode;
        }
        return false;
    }
    //------------------------------------------------------------
    //  Конвертация строки из UTF-16 (широкой строки Windows) в UTF-8
    //------------------------------------------------------------
    static std::string wideToUtf8(std::wstring_view w)
	{
		//---Если входная строка пустая, сразу возвращаем пустую строку UTF-8
		if (w.empty()) return {};

		//---WideCharToMultiByte с пустым выходным буфером, определение необходимого размера буфера
		int n = WideCharToMultiByte(
			CP_UTF8,
			0,
			w.data(),
			(int)w.size(),
			nullptr,
			0,
			nullptr,
			nullptr
		);

		//---Если ошибка или размер 0, возвращаем пустую строку
		if (n <= 0) return {};

		//---Создаем строку UTF-8 нужного размера, заполненную нулевыми символами
		std::string s((size_t)n, '\0');

		//---WideCharToMultiByte: фактическая конвертация
		WideCharToMultiByte(CP_UTF8,
			0,
			w.data(),
			(int)w.size(),
			s.data(),
			n,
			nullptr,
			nullptr
		);
		return s;
    }
    //------------------------------------------------------------
	//  Платформозависимое преобразование 
    //  fs::path в std::string в кодировке UTF-8
    //------------------------------------------------------------
    static std::string pathToUtf8(const fs::path& p)
    {
#ifdef _WIN32    
        return wideToUtf8(p.wstring()); // ВЕТКА ДЛЯ WINDOWS:
#else           
        return p.string();              // ВЕТКА ДЛЯ Linux
#endif
    }
    //------------------------------------------------------------
    //  Построение строки значения BinPath для службы Windows
    //------------------------------------------------------------
    static std::string buildBinPathValue(const ServiceSpec& spec)
    {
        //---Получаем путь к исполняемому файлу в кодировке UTF - 8
        std::string exe = pathToUtf8(spec.exeAbs);
        
        //---Создаем начальную строку с путем в кавычках
        //   Кавычки необходимы, если путь содержит пробелы
        //   Формат: "путь_к_исполняемому_файлу"
        std::string v = "\"" + exe + "\"";
        
        //---Проверка наличия и добавление аргументов
        if (!spec.args.empty())
        {
            v += " ";
            v += spec.args;
        }
        return v;
    }
    //------------------------------------------------------------
	//  Реализация бэкенда установки службы для Windows с использованием sc.exe
    //------------------------------------------------------------
    class BackendWinSc final : public IServiceBackend {
    public:
		//---Установка или обновление службы
        bool installOrUpdate(const ServiceSpec& spec, std::string* error) override
        {
            if (spec.name.empty()) { if (error) *error = "installOrUpdate: empty service name"; return false; }
            if (spec.exeAbs.empty()) { if (error) *error = "installOrUpdate: empty exeAbs"; return false; }

            bool exists = false;
            if (!serviceExists(spec.name, exists, error)) return false;

            const std::string startType = spec.autostart ? "auto" : "demand";
            const std::string binValue = buildBinPathValue(spec);

            if (!exists)
            {
                // sc create "<name>" binPath= "<value>" start= auto
                if (!runSc({ "create", spec.name,
                             "binPath=", binValue,
                             "start=", startType },
                    { 0, (int)ERROR_SERVICE_EXISTS }, error, "sc create"))
                    return false;
            }

            // всегда config (обновить binPath/start)
            if (!runSc({ "config", spec.name,
                         "binPath=", binValue,
                         "start=", startType },
                { 0 }, error, "sc config"))
                return false;

            // description (optional)
            if (!spec.description.empty())
            {
                if (!runSc({ "description", spec.name, spec.description },
                    { 0 }, error, "sc description"))
                    return false;
            }

            // recovery
            if (!runSc({ "failure", spec.name,
                         "reset=", "10",
                         "actions=", "restart/2000/restart/2000/restart/2000" },
                { 0 }, error, "sc failure"))
                return false;

            if (!runSc({ "failureflag", spec.name, "1" },
                { 0 }, error, "sc failureflag"))
                return false;

            return true;
        }
		//---Удаление службы
        bool uninstall(const std::string& name, bool stopFirst, std::string* error) override
        {
            if (name.empty()) { if (error) *error = "uninstall: empty service name"; return false; }

            bool exists = false;
            if (!serviceExists(name, exists, error)) return false;
            if (!exists) return true; // идемпотентно

            if (stopFirst)
            {
                // 1062 = уже остановлена — это ок
                if (!runSc({ "stop", name },
                    { 0, (int)ERROR_SERVICE_NOT_ACTIVE }, error, "sc stop"))
                    return false;
            }

            // 1072 = помечена на удаление — тоже ок
            if (!runSc({ "delete", name },
                { 0, (int)ERROR_SERVICE_MARKED_FOR_DELETE }, error, "sc delete"))
                return false;

            return true;
        }
		//---Запуск службы
        bool start(const std::string& name, std::string* error) override
        {
            // 1056 = already running => ok
            return runSc({ "start", name }, { 0, (int)ERROR_SERVICE_ALREADY_RUNNING }, error, "sc start");
        }
		//---Остановка службы
        bool stop(const std::string& name, std::string* error) override
        {
            return runSc({ "stop", name }, { 0, (int)ERROR_SERVICE_NOT_ACTIVE }, error, "sc stop");
        }
    };

} // namespace svcinst

namespace svcinst::platform {
    //------------------------------------------------------------
    //  Cоздание бэкенда для текущей платформы
    //------------------------------------------------------------
    std::unique_ptr<IServiceBackend> makeBackend()
    {
        return std::make_unique<svcinst::BackendWinSc>();
    }
} // namespace svcinst::platform

#endif
