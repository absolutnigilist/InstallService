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

	//---Опции командной строки
	struct CliOptions final {
	
		//---Команда
		Command cmd = Command::Help;

		//---Параметры службы
		std::string name;			//	Имя службы
		std::string exe;			//	Путь к EXE
		std::string args;			//	Аргументы командной строки для EXE
		std::string description;	//	Описание службы

		//---Флаги
		bool runNow = false;		//	Запустить службу сразу после установки
		bool stopFirst = false;		//	Остановить службу перед удалением
	
	};

	CliOptions parceCli(int argc, char** argv);
	void printHelp(std::ostream& os);

};//---namespace svcinst