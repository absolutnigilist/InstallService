#ifdef _WIN32
#include "..\src\platform\PlatformImpl.hpp"
#include "service_installer/IServiceBackend.hpp"
#include <sstream>

namespace svcinst {

    class BackendWinSc final : public IServiceBackend {
    public:
		//---Установка или обновление службы
        bool installOrUpdate(const ServiceSpec& spec, std::string* error) override {
            if (error) {
                std::ostringstream os;
                os << "[Windows backend stub] installOrUpdate not implemented yet. "
                    << "name=" << spec.name << " exe=" << spec.exeAbs.string();
                *error = os.str();
            }
            return false;
        }

		//---Удаление службы
        bool uninstall(const std::string& name, bool stopFirst, std::string* error) override {
            if (error) {
                std::ostringstream os;
                os << "[Windows backend stub] uninstall not implemented yet. "
                    << "name=" << name << " stopFirst=" << (stopFirst ? "true" : "false");
                *error = os.str();
            }
            return false;
        }
		//---Запуск службы
        bool start(const std::string& name, std::string* error) override {
            if (error) *error = "[Windows backend stub] start not implemented yet. name=" + name;
            return false;
        }
		//---Остановка службы
        bool stop(const std::string& name, std::string* error) override {
            if (error) *error = "[Windows backend stub] stop not implemented yet. name=" + name;
            return false;
        }
    };

} // namespace svcinst

namespace svcinst::platform {

	//---Cоздание бэкенда для текущей платформы
    std::unique_ptr<IServiceBackend> makeBackend()
    {
		//---Создание экземпляра бэкенда для Windows Service Control
        return std::make_unique<svcinst::BackendWinSc>();
    }

} // namespace svcinst::platform
#endif
