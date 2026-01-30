#include "service_installer/Installer.hpp"
#include "service_installer/Platform.hpp"
#include "service_installer/Paths.hpp"
#include "service_installer/ServiceSpec.hpp"
#include "service_installer/IServiceBackend.hpp"
#include "platform/PlatformImpl.hpp" 

#include <iostream>
#include <filesystem>
#include <glog/logging.h>

namespace svcinst {

	namespace {
		//------------------------------------------------------------
		//	ПРоверяем нужно ли удалять папку инсталляции
		//------------------------------------------------------------
		static bool wantDeleteInstallDir(svcinst::DeletePolicy p)
		{
			return p == svcinst::DeletePolicy::InstallDir || p == svcinst::DeletePolicy::All;
		}
		//------------------------------------------------------------
		//	Проверяем нужно ли удалять папку с сигналами
		//------------------------------------------------------------
		static bool wantDeleteDataRoot(svcinst::DeletePolicy p)
		{
			return p == svcinst::DeletePolicy::DataRoot || p == svcinst::DeletePolicy::All;
		}
		//------------------------------------------------------------
		//	Удаляем папку инсталляции и папку с сигналами
		//------------------------------------------------------------
		static void cleanupAfterUninstall(const svcinst::CliOptions& opt)
		{
			//---DataRoot
			if (wantDeleteDataRoot(opt.del))
			{
				if (!opt.dataRoot.empty())
				{
					std::string delErr;
					(void)svcinst::platform::removeDataRoot(svcinst::fs::path(opt.dataRoot), &delErr);
					if (!delErr.empty())
						LOG(WARNING) << "removeDataRoot: " << delErr;
				}
				else
				{
					LOG(WARNING) << "DeletePolicy requires DataRoot, but --data-root is empty";
				}
			}

			//---InstallDir
			if (wantDeleteInstallDir(opt.del))
			{
				const svcinst::fs::path installDir = svcinst::selfDir();

				std::string delErr;
				(void)svcinst::platform::removeInstallDir(installDir, &delErr, opt.fromInno);
				if (!delErr.empty())
					LOG(WARNING) << "removeInstallDir: " << delErr;
			}
		}
	} // namespace

	//------------------------------------------------------------
	//	Логирование ошибки и возврат кода ошибки
	//------------------------------------------------------------
	static int fail(const std::string& msg) {
		LOG(ERROR) << msg;
		return 1;
	}
	//------------------------------------------------------------
	//	Проверка, что строка пустая или содержит только пробельные символы
	//------------------------------------------------------------
	static bool isEmptyOrWhitespace(const std::string& s) {
		for (char c : s)
		{
			if (!std::isspace(static_cast<char>(c))) return false;
		}
		return true;
	}
	//------------------------------------------------------------
	//	Оркестратор: запуск установщика службы с заданными опциями
	//------------------------------------------------------------
	int runInstaller(const CliOptions& opt) {

		//---Проверка прав администратора / root
		if (!requireAdminRoot()) return fail("Administrator/root privileges required.");

		//---Создание бэкенда для текущей платформы
		auto backend = makeBackend();

		//---Проверка доступности бэкенда
		if (!backend) return fail("Backend not available on this platform.");

		//---Валидация опций
		if (opt.cmd != Command::Help)
		{
			if (opt.name.empty()) return fail("Missing required option: --name=<service_name>");
		}

		std::string err;

		//---Если команда — установка службы 
		if (opt.cmd == Command::Install)
		{
			//---Если путь к исполняемому файлу службы не задан → ошибка
			if (opt.exe.empty()) return fail("Missing required option for --install: --exe=<path_to_service_executable>");

			//---Формирование спецификации службы
			ServiceSpec spec;
			spec.name = opt.name;
			spec.description = opt.description;

			//---Если описание службы пустое → использование имени службы
			if (spec.description.empty() || isEmptyOrWhitespace(spec.description))
			{
				spec.description = spec.name;
			}
			//---Парсим путь к исполняемому файлу службы
			spec.exeAbs = resolveServiceExePath(opt.exe);

			//---Если путь к службе не задан → ошибка
			if (spec.exeAbs.empty()) return fail("Failed to resolve --exe path.");

			std::error_code ec;
			if (!fs::exists(spec.exeAbs, ec)) return fail("Service executable does not exist: " + spec.exeAbs.string());

			spec.args = opt.args;		//	Аргументы командной строки для exe
			spec.autostart = true;		//	Включать при загрузке(включать systemd / автозапуск Windows)
			spec.runNow = opt.runNow;	//	Запустить службу сразу после установки

			//---Установка или обновление службы c заданной спецификацией
			if (!backend->installOrUpdate(spec, &err))
			{
				return fail(err.empty() ? "installOrUpdate failed." : err);
			}

			if (spec.runNow)
			{
				if (!backend->start(spec.name, &err)) return fail(err.empty() ? "start failed." : err);
			}
			return 0;
		}
		//---Если команда — удаление службы
		if (opt.cmd == Command::Uninstall)
		{
			if (!backend->uninstall(opt.name, opt.stopFirst, &err))
			{
				return fail(err.empty() ? "uninstall failed." : err);
			}

			cleanupAfterUninstall(opt);
			return 0;
		}
		//---Если команда — запуск службы
		if (opt.cmd == Command::Start)
		{
			if (!backend->start(opt.name, &err))
			{
				return fail(err.empty() ? "start failed." : err);
			}
			return 0;
		}
		//---Если команда — остановка службы
		if (opt.cmd == Command::Stop)
		{
			if (!backend->stop(opt.name, &err))
			{
				return fail(err.empty() ? "stop failed." : err);
			}
			return 0;
		}
		return 2;
	}
}; //---namespace svcinst