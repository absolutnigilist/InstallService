#pragma once
#include <string>
#include "ServiceSpec.hpp"

namespace svcinst {

	//---Интерфейс бэкэнда установки службы
	class IServiceBackend {
	public:
		virtual ~IServiceBackend() = default;

		virtual bool installOrUpdate(const ServiceSpec& spec, std::string* error) = 0;
		virtual bool uninstall(const std::string& name, bool stopFirst, std::string* error) = 0;

		virtual bool start(const std::string& name, std::string* error) = 0;
		virtual bool stop(const std::string& name, std::string* error) = 0;
	};
};//---namespace svcinst