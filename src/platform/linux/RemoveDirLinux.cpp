#if defined(__linux__)

#include "service_installer/Platform.hpp"

#include <unistd.h>
#include <filesystem>
#include <string>
#include <system_error>

namespace svcinst::platform {
    namespace fs = std::filesystem;

    static bool isDangerousPath(const fs::path& p)
    {
        if (p.empty()) return true;

        std::error_code ec;
        fs::path abs = fs::absolute(p, ec);
        if (ec) abs = p;

        if (abs == abs.root_path()) return true; // "/"
        if (abs.string().size() <= 1) return true;

        return false;
    }

    static bool removeTreeNow(const fs::path& dir, std::string* error)
    {
        if (dir.empty()) return true;

        if (isDangerousPath(dir))
        {
            if (error) *error = "Refuse to delete dangerous path: " + dir.string();
            return false;
        }

        std::error_code ec;
        if (!fs::exists(dir, ec)) return true;

        ec.clear();
        (void)fs::remove_all(dir, ec);
        if (!ec) return true;

        if (error) *error = "remove_all failed for '" + dir.string() + "': " + ec.message();
        return false;
    }

    static bool deferRemoveWithSh(const fs::path& installDir,
        const fs::path& dataRoot,
        std::string* error)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            if (error) *error = "fork() failed";
            return false;
        }

        if (pid == 0)
        {
            (void)chdir("/");

            execl("/bin/sh", "sh", "-c",
                "sleep 2; "
                "[ -n \"$1\" ] && rm -rf -- \"$1\" >/dev/null 2>&1; "
                "[ -n \"$2\" ] && rm -rf -- \"$2\" >/dev/null 2>&1",
                "sh",
                installDir.c_str(),
                dataRoot.c_str(),
                (char*)nullptr);

            _exit(127);
        }

        return true;
    }

    bool removeInstallDir(const fs::path& installDir, std::string* error, bool /*fromInno*/)
    {
        std::string err;
        if (removeTreeNow(installDir, &err))
            return true;

        std::string err2;
        if (deferRemoveWithSh(installDir, fs::path{}, &err2))
            return true;

        if (error) *error = err + "; defer failed: " + err2;
        return false;
    }

    bool removeDataRoot(const fs::path& dataRoot, std::string* error)
    {
        std::string err;
        if (removeTreeNow(dataRoot, &err))
            return true;

        std::string err2;
        if (deferRemoveWithSh(fs::path{}, dataRoot, &err2))
            return true;

        if (error) *error = err + "; defer failed: " + err2;
        return false;
    }

} // namespace svcinst::platform

#endif
