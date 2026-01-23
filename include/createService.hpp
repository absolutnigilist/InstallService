#pragma once
#include <string>

//---Создание службы
bool createService(const std::string& serviceName, const std::string& exePath);

//---Настройка политики восстановления
bool setServiceRecovery(const std::string& serviceName);

//---Включение перезапуска при ошибках
bool enableServiceFailure(const std::string& serviceName);