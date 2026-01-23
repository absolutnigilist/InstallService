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
	}

} // namespace svcinst