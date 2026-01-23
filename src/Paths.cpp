#include "service_installer/Paths.hpp"
#include"service_installer/Platform.hpp"
#include "platform/PlatformImpl.hpp"

namespace svcinst {

	//---Директория, в которой находится исполняемый файл InstallService (зависит от платформы)
	fs::path selfDir() {
		
		//---Получение пути к собственному исполняемому файлу
		const fs::path exe = platform::selfExePath();

		//---Возврат родительской директории или текущей директории, если путь не определён
		if (!exe.empty())
		{
			return exe.parent_path();
		}
		return fs::current_path();
	}
	//---Определение пути к исполняемому файлу службы:
	fs::path resolveServiceExePath(const std::string& exeArg) {
		
		//	 Обязательный параметр для установки; при его отсутствии установщик завершится с ошибкой
		if (exeArg.empty())
		{
			return {};
		}
		
		//---Формирование пути
		fs::path p(exeArg);
		
		//---Если путь относительный → формирование абсолютного пути относительно selfDir
		if (p.is_relative())
		{ 
			p = (selfDir() / p).lexically_normal();
		}
		return p;
	}
} // namespace svcinst
