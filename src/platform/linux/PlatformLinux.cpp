#if defined(__linux__)
#include "platform/PlatformImpl.hpp"
#include <unistd.h>
#include <vector>

namespace svcinst::platform {

	//---Получение пути к собственному исполняемому файлу
	fs::path selfExePath()
	{
		std::vector<char> buf(4096, '\0');
		ssize_t n = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
		if (n <= 0) return {};
		buf[(size_t)n] = '\0';
		return fs::path(buf.data());
	}
	//---Проверка, что процесс запущен с правами root
	bool isElevated()
	{
		return ::geteuid() == 0;
	}

} // namespace svcinst::platform
#endif
