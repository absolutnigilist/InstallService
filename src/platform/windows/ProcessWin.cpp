#ifdef _WIN32

#include "platform/ProcessImpl.hpp"
#include <windows.h>
#include <vector>

namespace svcinst::process::detail {

	//--- Преобразование строки UTF-8 в широкую строку (UTF-16) для Windows API
    static std::wstring utf8ToWide(const std::string& s)
    {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring w((size_t)n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
        return w;
    }

    //---Корректное quoting аргументов под CreateProcess (правило backslashes+quotes)
    static std::wstring quoteWindowsArg(std::wstring_view arg)
    {
        const bool needQuotes =
            arg.empty() || (arg.find_first_of(L" \t\n\v\"") != std::wstring_view::npos);

        if (!needQuotes) return std::wstring(arg);

        std::wstring out;
        out.reserve(arg.size() + 2);
        out.push_back(L'"');

        std::size_t bsCount = 0;
        for (wchar_t ch : arg)
        {
            if (ch == L'\\')
            {
                ++bsCount;
                out.push_back(L'\\');
                continue;
            }

            if (ch == L'"')
            {
                // удвоить backslash'и перед кавычкой и экранировать кавычку
                out.append(bsCount, L'\\');
                bsCount = 0;
                out.push_back(L'\\');
                out.push_back(L'"');
                continue;
            }

            bsCount = 0;
            out.push_back(ch);
        }

        // удвоить backslash'и перед закрывающей кавычкой
        out.append(bsCount, L'\\');
        out.push_back(L'"');
        return out;
    }
	//---Построение командной строки для CreateProcess
    static std::wstring buildCommandLine(const fs::path& exe, const std::vector<std::string>& args)
    {
        std::wstring cmd = quoteWindowsArg(exe.wstring());
        for (const auto& a : args)
        {
            cmd.push_back(L' ');
            cmd += quoteWindowsArg(utf8ToWide(a));
        }
        return cmd;
    }
	//---Платформенно-специфичная реализация запуска процесса для Windows
    bool runPlatform(const fs::path& exe, const std::vector<std::string>& args,
        RunResult& out, const RunOptions& opt)
    {
        out = {};

        const std::wstring cmdLine = buildCommandLine(exe, args);

        STARTUPINFOW si{};
        si.cb = sizeof(si);

        PROCESS_INFORMATION pi{};

        std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
        buf.push_back(L'\0');

        const DWORD flags = opt.hideWindow ? CREATE_NO_WINDOW : 0;
        const std::wstring cwdW = opt.workingDir.empty() ? L"" : opt.workingDir.wstring();
        const wchar_t* cwdPtr = opt.workingDir.empty() ? nullptr : cwdW.c_str();

        BOOL ok = CreateProcessW(
            exe.wstring().c_str(), // applicationName
            buf.data(),            // commandLine (mutable)
            nullptr, nullptr,
            FALSE,
            flags,
            nullptr,
            cwdPtr,
            &si,
            &pi
        );

        if (!ok)
        {
            out.started = false;
            out.sysError = GetLastError();
            out.exitCode = (int)out.sysError;
            return false;
        }

        out.started = true;
        CloseHandle(pi.hThread);

        const DWORD timeout = (opt.timeoutMs == 0) ? INFINITE : opt.timeoutMs;
        DWORD waitRes = WaitForSingleObject(pi.hProcess, timeout);

        if (waitRes == WAIT_TIMEOUT)
        {
            out.timedOut = true;
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
        }
        else if (waitRes == WAIT_FAILED)
        {
            out.sysError = GetLastError();
            CloseHandle(pi.hProcess);
            out.exitCode = (int)out.sysError;
            return false;
        }

        DWORD code = 0;
        if (!GetExitCodeProcess(pi.hProcess, &code))
        {
            out.sysError = GetLastError();
            CloseHandle(pi.hProcess);
            out.exitCode = (int)out.sysError;
            return false;
        }

        CloseHandle(pi.hProcess);
        out.exitCode = (int)code;
        return true;
    }

} // namespace svcinst::process::detail
#endif
