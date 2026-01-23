#include <iostream>
#include "service_installer/Installer.hpp"
#include "service_installer/Cli.hpp"

int main(int argc, char** argv) {
	
	//---Разбор аргументов командной строки
	const svcinst::CliOptions opt = svcinst::parceCli(argc, argv);

	//---Если запрошена справка или команда некорректна → вывод справки и выход
	if (opt.cmd == svcinst::Command::Help || opt.cmd == svcinst::Command::Invalid) 
	{
		svcinst::printHelp(std::cout);
		return (opt.cmd == svcinst::Command::Invalid) ? 2 : 0;
	}
	//---Запуск установщика службы с заданными опциями
	return svcinst::runInstaller(opt);
}
