#include "pathUtils.hpp"
#include <windows.h>
#include <shlobj.h>   
#include <glog/logging.h>

//---Получение пути к папке с сигналами
fs::path getDataRoot() {
	PWSTR wpath = nullptr;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &wpath);
	if (FAILED(hr) || !wpath) return{};
	fs::path base(wpath);
	CoTaskMemFree(wpath);
	return base / L"Valenta";
}
//---Получение значения аргумента командной строки
std::string getArgValue(int argc, char* argv[], const std::string& key) {
	for (size_t i = 1; i < argc; i++)
	{
		const std::string a = argv[i];
		const std::string prefix = key + "=";
		if (a.find(prefix) == 0) {
			return a.substr(prefix.length());
		}
	}
	return{};
}

//---Создание директории, если она не существует
bool createDirectory(const std::filesystem::path& dirPath)
{
	std::error_code ec;
	//---Проверяем наличие директории
	if (!fs::exists(dirPath))
	{
		//---Создаем директорию
		fs::create_directories(dirPath, ec);
		if (ec)
		{
			//---Ошибка при создании директории
			LOG(ERROR) << "Error creating directory " << dirPath << ": " << ec.message() << std::endl;
			return false;
		}
		else
		{
			LOG(INFO) << "Directory created: " << dirPath << std::endl;
			return true;
		}
	}
	return true; // Директория уже существует
}