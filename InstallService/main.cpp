#include <windows.h>
#include <iostream> 
#include <shellapi.h>
#include <glog/logging.h>
#include "adminChecker.hpp"
#include "createService.hpp"
#include "pathUtils.hpp"
#include "logging.hpp"

 

int main(int argc, char* argv[]) {

	//---Инициализация логгера
	initLogging(argv[0]);

	//---Проверка прав администратора
	if (!IsRunAsAdmin())
	{
		std::cout << "Run the program as administrator!\n";
		system("pause");
		return 1;
	}
	//---Параметры службы
	const char* serviceName = "Valenta";

	//---Получаем путь к EXE из аргументов командной строки
	std::string exePath = getArgValue(argc, argv, "--exe");
	if (exePath.empty()) {
		LOG(ERROR) << "Usage: ServiceInstaller.exe --exe=\"C:\\Path\\Valenta.exe\"\n";
		system("pause");
		return 1;
	}

	//---Создаем директорию для данных
	bool dirCreated = createDirectory(getDataRoot());
	if (!dirCreated)
	{
		LOG(ERROR) << "Error creating data directory!\n";
		system("pause");
		return 1;
	}

	//---Создаем службу
	std::cout << "Create service " << serviceName << "..." << std::endl;
	if (!createService(serviceName, exePath))
	{
		LOG(ERROR) << "Error creating service!\n";
		system("pause");
		return 1;
	}
	
	//---Настраиваем восстановление
	std::cout << "Setting up a recovery policy..." << std::endl;
	if (!setServiceRecovery(serviceName))
	{
		LOG(ERROR) << "Error while setting up recovery\n";
	}
	
	//---Включаем перезапуск при ошибках
	std::cout << "Enable restart on errors..." << std::endl;
	if (!enableServiceFailure(serviceName))
	{
		LOG(ERROR) << "Error when enabling restart!\n";
	}

	std::cout << "\n================================\n";
	std::cout << "Service created successfully!\n";
	std::cout << "Service name: " << serviceName << "\n";
	std::cout << "EXE path: " << exePath << "\n";
	std::cout << "Autostart: Yes\n";
	std::cout << "Recovery policy: 3 restart attempts (with 2 sec interval)\n";
	std::cout << "================================\n";

	std::cout << "\nManagement commands:\n";
	std::cout << "  Start:    sc start " << serviceName << "\n";
	std::cout << "  Stop:     sc stop " << serviceName << "\n";
	std::cout << "  Status:   sc query " << serviceName << "\n";
	std::cout << "  Remove:   sc delete " << serviceName << "\n";

	system("pause");
	return 0;
}