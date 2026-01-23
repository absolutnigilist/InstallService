#pragma once
#include <filesystem>
#include <string>

namespace svcinst {
	
	namespace fs = std::filesystem;

	//---Спецификация службы для установки / обновления
	struct ServiceSpec final{

		//---Параметры службы
		std::string name;			//	Обязательное поле
		std::string description;	//	Если поле пустое, бэкэнд может использовать имя

		fs::path exeAbs;			//	Для установки / обновления требуется абсолютный путь
		std::string args;			//	Аргументы командной строки для exe

		//---Флаги
		bool autostart = true;		//	Включать при загрузке(включать systemd / автозапуск Windows)
		bool runNow = false;		//	Запустить службу сразу после установки
	};
};//---namespace svcinst