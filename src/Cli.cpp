#include "service_installer/Cli.hpp"
#include "string_view"
#include <iomanip>

namespace svcinst {

	//---Проверка, что строка соответствует флагу
	static bool isFlag(std::string_view s, std::string_view f) { return s == f; }

	//---Проверка, что строка начинается с префикса
	static bool startsWith(std::string_view s, std::string_view p) {
		return s.size() >= p.size() && s.substr(0, p.size()) == p;
	}
	
	//---Удаление кавычек в начале и конце строки
	static std::string trimQuotes(std::string v) {

		//---Если строка слишком короткая → возврат без изменений
		if (v.size() < 2) return v;

		//---Проверка на двойные или одинарные кавычки
		const bool dbl = (v.front() == '"' && v.back() == '"');
		const bool sgl = (v.front() == '\'' && v.back() == '\'');
		
		//---Если есть кавычки → удаление
		if (dbl || sgl) v = v.substr(1, v.size() - 2);

		return v;
	}
	//---Получение значения ключа из аргументов командной строки
	static std::string getKv(int argc, char** argv, const std::string& key) {
	
		//---Формирование префикса ключа
		const std::string prefix = key + "=";
		
		//---Поиск ключа в аргументах
		for (size_t i = 0; i < argc; i++)
		{
			//---Проверка, что аргумент начинается с префикса
			std::string_view a = argv[i];
			if (startsWith(a,prefix))
			{
				//---Возврат значения без префикса и с удалёнными кавычками
				return trimQuotes(std::string(a.substr(prefix.size())));
			}
		}
		return{};
	}
	//---Проверка наличия флага в аргументах командной строки
	static bool hasFlag(int argc, char** argv, std::string_view flag) {
		for (size_t i = 0; i < argc; i++)
		{
			if (isFlag(argv[i], flag)) return true;
		}
		return false;
	}
	CliOptions parceCli(int argc, char** argv) {
	
		//---Результирующие опции
		CliOptions o;

		//---Определение команды
		const bool install = hasFlag(argc, argv, "--install");
		const bool unistall = hasFlag(argc, argv, "--uninstall");
		const bool start = hasFlag(argc, argv, "--start");	
		const bool stop = hasFlag(argc, argv, "--stop");

		//---Запустить службу?
		o.runNow = hasFlag(argc, argv, "--run");

		//---Параметры службы
		o.name = getKv(argc, argv, "--name");
		o.exe = getKv(argc, argv, "--exe");
		o.args = getKv(argc, argv, "--args");	

		const std::string desc = getKv(argc, argv, "--desc");
		if (!desc.empty()) o.description = desc;

		//---Определение команды
		const int cmdCount =
			(install ? 1 : 0) +
			(unistall ? 1 : 0) +
			(start ? 1 : 0) +
			(stop ? 1 : 0);

		//---Если не указана ни одна команда → Help
		if (cmdCount == 0) 
		{
			o.cmd = Command::Help;
			return o;
		}
		//---Если некорректно указана команда → Invalid
		if (cmdCount > 1)
		{
			o.cmd = Command::Invalid;
			return o;
		}
		if(install) o.cmd = Command::Install;
		if(unistall) o.cmd = Command::Uninstall;
		if(start) o.cmd = Command::Start;
		if(stop) o.cmd = Command::Stop;
	
		//---Флаг остановки службы перед удалением
		o.stopFirst = (o.cmd == Command::Uninstall) && stop;
		
		//---Возврат опций
		return o;
	}
	//---Вывод опции с описанием
	static void printOpt(std::ostream& os, const std::string& opt, const std::string& desc, int w = 24)
	{
		os << "  " << std::left << std::setw(w) << opt << desc << "\n";
	}
	//---Вывод справки по использованию
	void printHelp(std::ostream& os)
	{
		os <<
			"service-installer\n\n"
			"Commands (choose exactly one):\n"
			"  --install     Install or update service\n"
			"  --uninstall   Uninstall service\n"
			"  --start       Start service\n"
			"  --stop        Stop service\n\n"
			"Options:\n";

		printOpt(os, "--name=<name>", "Service name (required)");
		printOpt(os, "--exe=<path>", "Service executable (required for --install)");
		printOpt(os, "", "If relative: resolved relative to service-installer location.");
		printOpt(os, "--args=\"...\"", "Optional args passed to the service");
		printOpt(os, "--desc=\"...\"", "Optional description");
		printOpt(os, "--run", "For --install: start right after install");
		printOpt(os, "--stop", "For --uninstall: stop before uninstall");

		os <<
			"\nExamples:\n"
			"  service-installer --install --name=Valenta --exe=Valenta.exe --run\n"
			"  service-installer --uninstall --name=Valenta --stop\n"
			"  service-installer --start --name=Valenta\n"
			"  service-installer --stop  --name=Valenta\n";
	}
};//---namespace svcinst
