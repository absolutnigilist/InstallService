#pragma once
#include <filesystem>
#include <string>

namespace svcinst {

	namespace fs = std::filesystem;

	//---Директория, в которой находится исполняемый файл InstallService (зависит от платформы)
	fs::path selfDir();
	
	//--Определение пути к исполняемому файлу службы:
	//	Если exeArg пустой → возвращается пустой путь(установщик выдаст ошибку, если путь обязателен)
	//	Если exeArg относительный путь → selfDir() / exeArg
	//	Если exeArg абсолютный путь → остаётся без изменений"
	fs::path resolveServiceExePath(const std::string& exeArgs);

};//---namespace svcinst	