#include <glog/logging.h>
#include <filesystem>
#include "logging.hpp"
#include "pathUtils.hpp"

//---Инициализация логгера
void initLogging(const char* programmName) {
	
	//---Получение корневого пути для логов
	const fs::path root = getDataRoot();
	//---Путь к папке с логами
	const fs::path logDir = root / L"log";

	//---Создание директории для логов
	if (!root.empty()) createDirectory(logDir);

	google::SetLogFilenameExtension(".txt"); // Расширение
	google::SetLogDestination(google::GLOG_INFO, (logDir / "info").string().c_str()); // Путь и название логов для ошибок, предупреждений и информации
	google::SetLogDestination(google::GLOG_WARNING, (logDir / "warning").string().c_str());
	google::SetLogDestination(google::GLOG_ERROR, (logDir / "error").string().c_str());
	google::SetLogDestination(google::GLOG_FATAL, (logDir / "fatal").string().c_str());
	google::InitGoogleLogging(programmName); // Инициализация

	//---Настройка вывода в консоль
	FLAGS_alsologtostderr = true;
	FLAGS_colorlogtostderr = true;
}