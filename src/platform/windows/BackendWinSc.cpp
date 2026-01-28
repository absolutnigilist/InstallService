#ifdef _WIN32

#include "service_installer/IServiceBackend.hpp"
#include "service_installer/Process.hpp"

#include <windows.h>
#include <filesystem>
#include <memory>
#include <sstream>

namespace svcinst {

    namespace fs = std::filesystem;

	//--- Путь к sc.exe
    static fs::path scExePath()
    {
        wchar_t sysDir[MAX_PATH]{};
        UINT n = GetSystemDirectoryW(sysDir, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return fs::path(L"sc.exe");
        return fs::path(sysDir) / L"sc.exe";
    }
	//--- Проверка, что код выхода входит в список допустимых
    static bool scOkExit(int code, std::initializer_list<int> ok)
    {
        for (int v : ok) if (code == v) return true;
        return false;
    }

    //---Обёртка над sc.exe
    static bool runSc(const std::vector<std::string>& args,
        std::initializer_list<int> okExitCodes,
        std::string* error,
        const char* what)
    {
        process::RunResult rr;
        const bool started = process::run(scExePath(), args, rr, process::RunOptions{ .hideWindow = true });

        if (!started || !rr.started)
        {
            if (error)
            {
                std::ostringstream os;
                os << what << ": failed to start sc.exe. sysError=" << rr.sysError;
                *error = os.str();
            }
            return false;
        }

        if (!scOkExit(rr.exitCode, okExitCodes))
        {
            if (error)
            {
                std::ostringstream os;
                os << what << ": sc.exe exitCode=" << rr.exitCode;
                *error = os.str();
            }
            return false;
        }

        return true;
    }
	//---Проверка существования службы
    static bool serviceExists(const std::string& name, bool& exists, std::string* error)
    {
        process::RunResult rr;
        const bool started = process::run(scExePath(), { "query", name }, rr, process::RunOptions{ .hideWindow = true });
        if (!started || !rr.started)
        {
            if (error)
            {
                std::ostringstream os;
                os << "sc query: failed to start sc.exe. sysError=" << rr.sysError;
                *error = os.str();
            }
            return false;
        }

        if (rr.exitCode == 0) { exists = true; return true; }
        if (rr.exitCode == (int)ERROR_SERVICE_DOES_NOT_EXIST) { exists = false; return true; }

        if (error)
        {
            std::ostringstream os;
            os << "sc query: unexpected exitCode=" << rr.exitCode;
            *error = os.str();
        }
        return false;
    }
    //---
    static std::string wideToUtf8(std::wstring_view w)
    {
        if (w.empty()) return {};

        int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
            nullptr, 0, nullptr, nullptr);
        if (n <= 0) return {};

        std::string s((size_t)n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
            s.data(), n, nullptr, nullptr);
        return s;
    }
	//---Преобразование fs::path в std::string в кодировке UTF-8
    static std::string pathToUtf8(const fs::path& p)
    {
#ifdef _WIN32
        return wideToUtf8(p.wstring());
#else
        return p.string();
#endif
    }

    //---Значение для binPath: "\"C:\...\app.exe\" <args>"
    static std::string buildBinPathValue(const ServiceSpec& spec)
    {
        std::string exe = pathToUtf8(spec.exeAbs);
        std::string v = "\"" + exe + "\"";
        if (!spec.args.empty())
        {
            v += " ";
            v += spec.args;
        }
        return v;
    }
	//---Реализация бэкенда установки службы для Windows с использованием sc.exe
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
	
    //---Cоздание бэкенда для текущей платформы
    std::unique_ptr<IServiceBackend> makeBackend()
    {
        return std::make_unique<svcinst::BackendWinSc>();
    }

} // namespace svcinst::platform

#endif
