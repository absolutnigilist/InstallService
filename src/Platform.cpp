#include "service_installer/Platform.hpp"
#include "service_installer/IServiceBackend.hpp"
#include "platform/PlatformImpl.hpp"

namespace svcinst {
	//------------------------------------------------------------
	//	Проверка прав администратора
	//------------------------------------------------------------
	bool requireAdminRoot() {
		return platform::isElevated();
	}
	//------------------------------------------------------------
	//	Создание бэкенда для текущей платформы
	//------------------------------------------------------------
	std::unique_ptr<IServiceBackend> makeBackend() {
		return platform::makeBackend();
	}
}; //---namespace svcinst