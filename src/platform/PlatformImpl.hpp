#pragma once
#include <filesystem>
#include <memory>

namespace svcinst {

	//---Интерфейс бэкэнда установки службы
	class IServiceBackend;

	//---Платформенно-зависимые реализации
	namespace platform {
		namespace fs = std::filesystem;

		//---Получение пути к собственному исполняемому файлу
		fs::path selfExePath();
		//---Проверка, что процесс запущен с правами администратора / root
		bool isElevated();
		//---Cоздание бэкенда для текущей платформы
		std::unique_ptr<IServiceBackend> makeBackend();
		//---Удалить папку установки (fromInno: Windows-only смысл (если true — installDir не трогаем))
		bool removeInstallDir(const fs::path& installDir, std::string* error, bool fromInno = false);
		//---Удалить DataRoot (данные/кэш/сигналы и т.п.)
		bool removeDataRoot(const fs::path& dataRoot, std::string* error);
	}

} // namespace svcinst