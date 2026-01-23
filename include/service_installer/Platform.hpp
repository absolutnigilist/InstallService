#pragma once
#include <memory>

namespace svcinst {
	
	//---Интерфейс бэкэнда установки службы
	class IServiceBackend;

	//---Запуск с правами администратора
	bool requireAdminRoot();

	//---Создание бэкенда для текущей платформы
	std::unique_ptr<IServiceBackend> makeBackend();

};//---namespace svcinst