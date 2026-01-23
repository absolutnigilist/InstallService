#ifdef _WIN32
#include "..\src\platform\PlatformImpl.hpp"
#include <windows.h>

namespace svcinst::platform {

	//---Получение пути к собственному исполняемому файлу
    fs::path selfExePath()
    {
        wchar_t buf[MAX_PATH]{};
        DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return {};
        return fs::path(buf);
    }

	//---Проверка, что процесс запущен с правами администратора
    bool isElevated()
    {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

        if (AllocateAndInitializeSid(
            &NtAuthority, 2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &adminGroup))
        {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        return isAdmin == TRUE;
    }

} // namespace svcinst::platform
#endif
