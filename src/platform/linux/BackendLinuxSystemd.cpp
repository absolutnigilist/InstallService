#if defined(__linux__)

#include "service_installer/IServiceBackend.hpp"
#include "service_installer/Process.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace svcinst {

    namespace fs = std::filesystem;

    namespace {

        //---Проверка корректности имени systemd unit
        //   Имена должны состоять только из безопасных символов
        //   разрешенные символы: A-Z, a-z, 0-9, '_', '.', '-'
        static bool isValidUnitName(const std::string& name)
        {
            //---Минимально строгая валидация: systemd unit name обычно безопаснее
            //   ограничить на [A-Za-z0-9_.-]. Без пробелов и слэшей.
            if (name.empty()) return false;

            for (char c : name)
            {
                const bool ok =
                    (c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') ||
                    c == '_' || c == '.' || c == '-';
                if (!ok) return false;
            }
            return true;
        }
        
        //---Возвращает полный путь к файлу systemd unit
        static fs::path unitPath(const std::string& name)
        {
            // системный unit - размещается в /etc/systemd/system/
            return fs::path("/etc/systemd/system") / (name + ".service");
        }

        //---Возвращает полное имя unit с расширением .service
        static std::string unitName(const std::string& name)
        {
            return name + ".service";
        }

        //---Вспомогательная функция для запуска systemctl
        
        // Запускает systemctl с указанными аргументами и проверяет код возврата
        // Параметры:
        //   args - аргументы для systemctl (например {"start", "myservice.service"})
        //   okExitCodes - список допустимых кодов возврата (например {0})
        //   error - строка для записи ошибки (если указана)
        //   what - описание операции для сообщений об ошибках
        static bool runSystemctl(const std::vector<std::string>& args,
            std::initializer_list<int> okExitCodes,
            std::string* error,
            const char* what)
        {
            process::RunResult rr;
            //---Запускаем /bin/systemctl с указанными аргументами
            const bool ok = process::run(fs::path("/bin/systemctl"), args, rr, process::RunOptions{});
            if (!ok || !rr.started)
            {
                if (error)
                {
                    std::ostringstream os;
                    os << what << ": failed to start systemctl. sysError=" << rr.sysError;
                    *error = os.str();
                }
                return false;
            }

            //---Проверяем код возврата на соответствие допустимым значениям
            for (int code : okExitCodes)
            {
                if (rr.exitCode == code) return true;
            }

            //---Если код возврата недопустимый - записываем ошибку
            if (error)
            {
                std::ostringstream os;
                os << what << ": systemctl exitCode=" << rr.exitCode;
                *error = os.str();
            }
            return false;
        }

        
        //---Генерация файла systemd unit
        
        //---Очищает описание от символов новой строки
        static std::string sanitizeDescription(const std::string& s)
        {
            // В unit-файле нежелательны переводы строк
            std::string out;
            out.reserve(s.size());
            for (char c : s)
            {
                if (c == '\r' || c == '\n') continue;
                out.push_back(c);
            }
            if (out.empty()) out = "service";
            return out;
        }

        //---Добавляет кавычки к строке, если она содержит пробелы или кавычки
        static std::string quoteIfNeeded(const std::string& s)
        {
            //---Для ExecStart systemd поддерживает кавычки. Мы гарантируем кавычки для exe с пробелами.
            //   Аргументы добавляем "как есть" (на следующем шаге можно сделать токенизацию).
            if (s.empty()) return "\"\"";

            bool need = false;
            for (char c : s)
            {
                if (c == ' ' || c == '\t' || c == '"') { need = true; break; }
            }
            if (!need) return s;

            std::string out;
            out.reserve(s.size() + 2);
            out.push_back('"');
            for (char c : s)
            {
                if (c == '"') out += "\\\"";  //---Экранируем кавычки внутри строки
                else out.push_back(c);
            }
            out.push_back('"');
            return out;
        }

        //---Создает и записывает файл systemd unit на основе спецификации сервиса
        static bool writeUnitFile(const ServiceSpec& spec, std::string* error)
        {
            const fs::path p = unitPath(spec.name);

            std::error_code ec;
            // ---Создаем директорию, если она не существует (обычно /etc/systemd/system уже существует)
            fs::create_directories(p.parent_path(), ec);
            //---если ec != 0 — не фатально, попробуем писать

            std::ofstream f(p, std::ios::binary | std::ios::trunc);
            if (!f)
            {
                if (error)
                {
                    std::ostringstream os;
                    os << "Failed to open unit file for writing: " << p.string();
                    *error = os.str();
                }
                return false;
            }

            const std::string desc = sanitizeDescription(spec.description.empty() ? spec.name : spec.description);

            //---exeAbs должен быть абсолютным путем
            const std::string exe = spec.exeAbs.string();
            const std::string execStart = quoteIfNeeded(exe) + (spec.args.empty() ? "" : (" " + spec.args));

            //---Политика восстановления — аналог Windows recovery:
            //   Restart=on-failure, RestartSec=2, StartLimitBurst=3, StartLimitIntervalSec=10
            f <<
                "[Unit]\n"
                "Description=" << desc << "\n"
                "After=network.target\n"           // Сервис запускается после настройки сети
                "\n"
                "[Service]\n"
                "Type=simple\n"                    // Простой сервис без дополнительного контроля
                "ExecStart=" << execStart << "\n"
                "Restart=on-failure\n"             // Перезапускать при ошибках
                "RestartSec=2\n"                   // Ждать 2 секунды перед перезапуском
                "StartLimitBurst=3\n"              // Максимум 3 попытки запуска
                "StartLimitIntervalSec=10\n"       // В течение 10 секунд
                "\n"
                "[Install]\n"
                "WantedBy=multi-user.target\n";    // Запускать в multi-user режиме

            f.flush();
            if (!f)
            {
                if (error)
                {
                    std::ostringstream os;
                    os << "Failed to write unit file: " << p.string();
                    *error = os.str();
                }
                return false;
            }

            return true;
        }

        //---Проверяет существование файла systemd unit
        static bool unitFileExists(const std::string& name)
        {
            std::error_code ec;
            return fs::exists(unitPath(name), ec);
        }

    } // namespace

    //---BackendLinuxSystemd - реализация сервисного бэкенда для Linux/systemd
    class BackendLinuxSystemd final : public IServiceBackend {
    public:
        //---Установка или обновление сервиса
        bool installOrUpdate(const ServiceSpec& spec, std::string* error) override
        {
            //---Валидация имени сервиса
            if (!isValidUnitName(spec.name))
            {
                if (error) *error = "installOrUpdate: invalid service name (allowed: A-Za-z0-9_.-)";
                return false;
            }
            //---Валидация пути к исполняемому файлу
            if (spec.exeAbs.empty() || !spec.exeAbs.is_absolute())
            {
                if (error) *error = "installOrUpdate: spec.exeAbs must be an absolute path";
                return false;
            }

            //--- 1) Создание и запись файла unit в /etc/systemd/system/<name>.service
            if (!writeUnitFile(spec, error))
                return false;

            //--- 2) Перезагрузка конфигурации systemd
            if (!runSystemctl({ "daemon-reload" }, { 0 }, error, "systemctl daemon-reload"))
                return false;

            //--- 3) Включение/отключение автозапуска
            const std::string u = unitName(spec.name);
            if (spec.autostart)
            {
                // Включаем автозапуск
                if (!runSystemctl({ "enable", u }, { 0 }, error, "systemctl enable"))
                    return false;
            }
            else
            {
                // Отключаем автозапуск
                // disable может вернуть ошибку если unit не был enabled — treat as ok?
                // systemctl обычно возвращает 0, но оставим строго: если хотите, смягчим на следующем шаге.
                if (!runSystemctl({ "disable", u }, { 0 }, error, "systemctl disable"))
                    return false;
            }

            //--- 4) Немедленный запуск (или перезапуск, если уже запущен)
            if (spec.runNow)
            {
                if (!runSystemctl({ "restart", u }, { 0 }, error, "systemctl restart"))
                    return false;
            }

            return true;
        }

        //---Удаление сервиса
        bool uninstall(const std::string& name, bool stopFirst, std::string* error) override
        {
            // Валидация имени сервиса
            if (!isValidUnitName(name))
            {
                if (error) *error = "uninstall: invalid service name (allowed: A-Za-z0-9_.-)";
                return false;
            }

            // Идемпотентность: если unit-файла нет — считаем, что уже удалено
            const bool exists = unitFileExists(name);
            const std::string u = unitName(name);

            //---Остановка сервиса (если требуется)
            if (stopFirst)
            {
                // stop best-effort: не делаем фатальным, если юнит не найден/не запущен
                std::string tmp;
                runSystemctl({ "stop", u }, { 0 }, &tmp, "systemctl stop");
            }

            //---Отключение автозапуска (best-effort)
            {
                std::string tmp;
                runSystemctl({ "disable", u }, { 0 }, &tmp, "systemctl disable");
            }

            //---Удаление файла unit (если он существует)
            if (exists)
            {
                std::error_code ec;
                fs::remove(unitPath(name), ec);
                if (ec)
                {
                    if (error)
                    {
                        std::ostringstream os;
                        os << "Failed to remove unit file: " << unitPath(name).string() << " : " << ec.message();
                        *error = os.str();
                    }
                    return false;
                }
            }

            //---Перезагрузка конфигурации systemd (важно после удаления файла)
            if (!runSystemctl({ "daemon-reload" }, { 0 }, error, "systemctl daemon-reload"))
                return false;

            return true;
        }

        //---Запуск сервиса
        bool start(const std::string& name, std::string* error) override
        {
            if (!isValidUnitName(name))
            {
                if (error) *error = "start: invalid service name (allowed: A-Za-z0-9_.-)";
                return false;
            }
            return runSystemctl({ "start", unitName(name) }, { 0 }, error, "systemctl start");
        }

        //---Остановка сервиса
        bool stop(const std::string& name, std::string* error) override
        {
            if (!isValidUnitName(name))
            {
                if (error) *error = "stop: invalid service name (allowed: A-Za-z0-9_.-)";
                return false;
            }
            return runSystemctl({ "stop", unitName(name) }, { 0 }, error, "systemctl stop");
        }
    };

} // namespace svcinst

namespace svcinst::platform {

    //---Фабричная функция для создания экземпляра бэкенда Linux/systemd
    std::unique_ptr<IServiceBackend> makeBackend()
    {
        return std::make_unique<svcinst::BackendLinuxSystemd>();
    }

} // namespace svcinst::platform

#endif // __linux__