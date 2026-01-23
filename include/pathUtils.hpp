#pragma once
#include <filesystem>

namespace fs = std::filesystem;

//---Получение пути к папке с сигналами
std::filesystem::path getDataRoot(); 

//---Получение значения аргумента командной строки
std::string getArgValue(int argc, char* argv[], const std::string& key);

//---Создание директории, если она не существует
bool createDirectory(const std::filesystem::path& dirPath);