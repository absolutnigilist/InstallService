#pragma once
#include <string>
#include <iostream>

namespace svcinst {

	//---Команды CLI
	enum class Command {
	Help,
	Install,
	Uninstall,
	Start,
	Stop,
	Invalid
	};

	//---Политика удаления
	enum class DeletePolicy {
		None,						// Не удалять ничего
		InstallDir,					// Удалить папку установки
		DataRoot,					// Удалить папку с данными
		All							// Удалить и установку, и данные
	};
	//---Опции командной строки
	struct CliOptions final {
	
		//---Команда
		Command cmd = Command::Help;
		//---Политика удаления по умолчанию
		DeletePolicy del = DeletePolicy::None; 

		//---Параметры службы
		std::string name;			//	Имя службы
		std::string exe;			//	Путь к EXE
		std::string args;			//	Аргументы командной строки для EXE
		std::string description;	//	Описание службы

		//---Флаги
		bool runNow = false;		//	Запустить службу сразу после установки
		bool stopFirst = false;		//	Остановить службу перед удалением

		

		std::string dataRoot;       // путь к данным (если нужен)
		bool fromInno = false;      // чтобы на Windows не удалять {app} из helper'а
	};

	CliOptions parceCli(int argc, char** argv);
	void printHelp(std::ostream& os);

};//---namespace svcinst