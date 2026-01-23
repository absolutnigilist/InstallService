#include "createService.hpp"
#include <iostream>

//---Создание службы
bool createService(const std::string& serviceName, const std::string& exePath) {
	std::string command = "sc create \"" + serviceName + "\" binPath= \"" + exePath + "\" start=auto";

	std::cout << "run command: " << command << std::endl;
	int result = system(command.c_str());
	return (result == 0);
}

//---Настройка политики восстановления
bool setServiceRecovery(const std::string& serviceName) {
	std::string command = "sc failure \"" + serviceName + "\" reset=10 actions= restart/2000/restart/2000/restart/2000";
	std::cout << "Setting up recovery: " << command << std::endl;
	int result = system(command.c_str());
	return (result == 0);
}

//---Включение перезапуска при ошибках
bool enableServiceFailure(const std::string& serviceName) {
	std::string command = "sc failureflag \"" + serviceName + "\" 1";
	std::cout << "Enable service restart: " << command << std::endl;

	int result = system(command.c_str());
	return (result == 0);
}