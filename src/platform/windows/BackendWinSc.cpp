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
                //---Логгируем ошибку запуска с системным кодом ошибки
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
                //---Логгируем код завершения sc.exe
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
                //---Логгируем код завершения sc.exe
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
            //---Логгируем код неизвестной ошибки
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
        //------------------------------------------------------------
		//  Установка или обновление службы
        //------------------------------------------------------------
        bool installOrUpdate(const ServiceSpec& spec, std::string* error) override
        {
            //---Проверка на пустое имя службы
			if (spec.name.empty())
			{
				if (error)
				{
					*error = "installOrUpdate: empty service name"; 
                    LOG(ERROR) << "installOrUpdate: empty service name";
				}
                return false;
			}
            //---Проверка на пустой путь к исполняемому файлу
            if (spec.exeAbs.empty()) 
            { 
                if (error)
                {
                    *error = "installOrUpdate: empty exeAbs";
                    LOG(ERROR) << "installOrUpdate: empty exeAbs";
                }
                return false;
            }
            //---Проверяем, существует ли уже служба с таким именем
            bool exists = false;
            if (!serviceExists(spec.name, exists, error)) return false;

            //---Подготавливаем параметры для sc.exe
            // 
            //---auto - автозапуск, demand - вручную
            const std::string startType = spec.autostart ? "auto" : "demand";   
            //---Формируем значение binPath
            const std::string binValue = buildBinPathValue(spec);

            //---Если службы не существует - создаем новую
            if (!exists)
            {
                //---Команда создания службы: sc create "<name>" binPath= "<value>" start= auto
                if (!runSc(
                    { 
                        "create", spec.name,            // Команда create и имя службы
                        "binPath=", binValue,           // binPath= "<путь к exe> [аргументы]"
                        "start=", startType             // start= auto/demand
                    },
                    { 0, (int)ERROR_SERVICE_EXISTS },   // Допустимые коды завершения: 0 или 1073
                    error,                              // Указатель для сообщения об ошибке
                    "sc create"                         // Описание операции для логгирования
                ))                       
                {
                    return false;   // Если создание не удалось
                }   
            }   // Конец блока создания новой службы
            
            //---Всегда выполняем config - обновляем параметры существующей службы
            //   Даже если только что создали службу, настраиваем её параметры
            //   всегда config (обновить binPath/start)
            if (!runSc(
                { 
                    "config", spec.name,               // Команда config и имя службы 
                    "binPath=", binValue,              // Обновляем путь к исполняемому файлу
                    "start=", startType                // Обновляем тип запуска
                },
                { 0 },                                 // Только код 0 считается успехом
                error,                                 // Указатель для ошибки
                "sc config"                            // Описание операции
            ))
            {
                return false;   // Если настройка не удалась
            }
             
			//---Установка описания службы (опционально)
			if (!spec.description.empty())  // Если описание не пустое
			{
				if (!runSc(
					{
						"description", spec.name, spec.description          // Команда description и имя службы
					},
					{ 0 },                                                  // Только код 0
					error,                                                  // Указатель для ошибки
					"sc description"                                        // Описание операции
				))
				{
					return false;   // Если установка описания не удалась
				}
			}
            //---Настройка восстановления службы при сбоях
            //   reset= 10 - сбрасывать счетчик сбоев через 10 секунд
            //   actions= restart/2000/restart/2000/restart/2000 - три попытки перезапуска с интервалом 2 сек
			if (!runSc(
				{
					"failure", spec.name,                                   // Команда failure и имя службы
					"reset=", "10",                                         // Сброс счетчика через 10 сек
					"actions=", "restart/2000/restart/2000/restart/2000"    // 3 перезапуска
				},
				{ 0 },                                                      // Только код 0
				error,                                                      // Указатель для ошибки
				"sc failure"                                                // Описание операции
			))
			{
				return false;   // Если настройка восстановления не удалась
			}
             
			//---Включение флага восстановления службы
			//   failureflag 1 - включаем обработку сбоев
			if (!runSc(
				{
					"failureflag", spec.name, "1"                           // Команда failureflag и имя службы
				},
				{ 0 },                                                      // Только код 0
				error,                                                      // Указатель для ошибки
				"sc failureflag"                                            // Описание операции
			))
			{
				return false;   // Если включение флага не удалось
			}
            return true;    // Все операции успешно выполнены
        }
        //------------------------------------------------------------
		//  Удаление службы
        //------------------------------------------------------------
        bool uninstall (const std::string& name, bool stopFirst, std::string* error) override
        {
            //---Проверка на пустое имя службы
            if (name.empty()) 
            { 
                if (error) 
                {
                    *error = "uninstall: empty service name"; 
                    LOG(ERROR) << "uninstall: empty service name";
                    return false;
                }
            }
            //---Проверяем существование службы
            bool exists = false;
            if (!serviceExists(name, exists, error)) return false; // Ошибка проверки
            
            //---Если служба не существует - возвращаем true (идемпотентность)
            if (!exists) return true; // идемпотентно - можно вызывать многократно

            //---Если нужно остановить службу перед удалением
            if (stopFirst)
            {
                // Команда остановки службы
                // ERROR_SERVICE_NOT_ACTIVE (1062) - "служба не запущена", считается допустимым
                if (!runSc(
                    { "stop", name },                           // Команда stop и имя службы
                    { 0, (int)ERROR_SERVICE_NOT_ACTIVE },       // Допустимые коды: 0 или 1062
                    error,                                      // Указатель для ошибки
                    "sc stop"                                   // Описание операции
                ))
                {
                    return false;   // Если остановка не удалась
                }
            }
            //---Удаление службы
            //   ERROR_SERVICE_MARKED_FOR_DELETE (1072) - "служба помечена на удаление", допустимо
            if (!runSc(
                { "delete", name },                             // Команда delete и имя службы
                { 0, (int)ERROR_SERVICE_MARKED_FOR_DELETE },    // Допустимые коды: 0 или 1072
                error,                                          // Указатель для ошибки
                "sc delete"                                     // Описание операции
            ))
            {
                return false;   // Если удаление не удалось
            }
            return true;    // Удаление успешно
        }
        //------------------------------------------------------------
        //  Запуск службы
        //------------------------------------------------------------
		bool start(const std::string& name, std::string* error) override
		{
			//---Команда запуска службы
			//   ERROR_SERVICE_ALREADY_RUNNING (1056) - "служба уже запущена", допустимо
			return runSc(
				{ "start", name },                              // Команда start и имя службы
				{ 0, (int)ERROR_SERVICE_ALREADY_RUNNING },      // Допустимые коды: 0 или 1056
				error,                                          // Указатель для ошибки                                
				"sc start"                                      // Описание операции
			);
		}
        //------------------------------------------------------------
        //  Остановка службы
        //------------------------------------------------------------
        bool stop(const std::string& name, std::string* error) override
		{
			//---Команда остановки службы
			//  ERROR_SERVICE_NOT_ACTIVE (1062) - "служба не запущена", допустимо
			return runSc(
				{ "stop", name },                               // Команда stop и имя службы
				{ 0, (int)ERROR_SERVICE_NOT_ACTIVE },           // Допустимые коды: 0 или 1062
				error,                                          // Указатель для ошибки 
				"sc stop"                                       // Описание операции
			);
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
