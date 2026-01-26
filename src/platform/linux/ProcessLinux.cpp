#if defined(__linux__)

#include "platform/ProcessImpl.hpp"
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

namespace svcinst::process::detail {

	//---Платформенно-специфичная реализация запуска процесса для Linux
    bool runPlatform(const fs::path& exe, const std::vector<std::string>& args,
        RunResult& out, const RunOptions& opt)
    {
        out = {};

        pid_t pid = fork();
        if (pid < 0)
        {
            out.started = false;
            out.sysError = (std::uint32_t)errno;
            out.exitCode = (int)out.sysError;
            return false;
        }

        if (pid == 0)
        {
            if (!opt.workingDir.empty())
                (void)chdir(opt.workingDir.c_str());

            std::vector<std::string> argvStorage;
            argvStorage.reserve(args.size() + 1);
            argvStorage.push_back(exe.string());
            argvStorage.insert(argvStorage.end(), args.begin(), args.end());

            std::vector<char*> argv;
            argv.reserve(argvStorage.size() + 1);
            for (auto& s : argvStorage) argv.push_back(s.data());
            argv.push_back(nullptr);

            execv(exe.c_str(), argv.data());
            _exit(127); // exec failed
        }

        out.started = true;

        int status = 0;
        // timeoutMs здесь пока не реализован (можно добавить позже циклом waitpid(WNOHANG))
        if (waitpid(pid, &status, 0) < 0)
        {
            out.sysError = (std::uint32_t)errno;
            out.exitCode = (int)out.sysError;
            return false;
        }

        if (WIFEXITED(status))
            out.exitCode = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            out.exitCode = 128 + WTERMSIG(status);
        else
            out.exitCode = 1;

        return true;
    }

} // namespace svcinst::process::detail
#endif
