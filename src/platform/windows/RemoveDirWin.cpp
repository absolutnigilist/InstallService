#ifdef _WIN32

#include "service_installer/Platform.hpp"
#include <windows.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>



namespace svcinst::platform {

    namespace fs = std::filesystem;
    //------------------------------------------------------------
    //  Safety: не дать снести "опасные" пути типа C:\ или корня
    //------------------------------------------------------------
    static bool isDangerousPath(const fs::path& p)
    {
        if (p.empty()) return true;

        std::error_code ec;
        fs::path abs = fs::absolute(p, ec);
        if (ec) abs = p;

        const fs::path root = abs.root_path();
        if (!root.empty() && abs == root) return true; // C:\

        const auto w = abs.wstring();
        if (w.size() <= 3) return true; // "C:\"

        return false;
    }

    //------------------------------------------------------------
    //  Снять ReadOnly/Hidden/System (часто мешают remove_all на Win)
    //------------------------------------------------------------
    static void makeNormalAttributes(const fs::path& p)
    {
        const std::wstring w = p.wstring();
        DWORD attr = GetFileAttributesW(w.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) return;

        DWORD newAttr = attr;
        newAttr &= ~FILE_ATTRIBUTE_READONLY;
        newAttr &= ~FILE_ATTRIBUTE_HIDDEN;
        newAttr &= ~FILE_ATTRIBUTE_SYSTEM;

        if (newAttr != attr)
            (void)SetFileAttributesW(w.c_str(), newAttr);
    }

    static void clearAttributesRecursive(const fs::path& root)
    {
        std::error_code ec;
        if (!fs::exists(root, ec)) return;

        makeNormalAttributes(root);

        fs::directory_options opt = fs::directory_options::skip_permission_denied;
        fs::recursive_directory_iterator it(root, opt, ec);
        fs::recursive_directory_iterator end;

        while (!ec && it != end)
        {
            makeNormalAttributes(it->path());
            it.increment(ec);
        }
    }

    //------------------------------------------------------------
    //  Попытка удалить сразу
    //------------------------------------------------------------
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

        clearAttributesRecursive(dir);

        ec.clear();
        (void)fs::remove_all(dir, ec);
        if (!ec) return true;

        if (error) *error = "remove_all failed for '" + dir.string() + "': " + ec.message();
        return false;
    }

    //------------------------------------------------------------
    //  Отложенное удаление: cmd.exe в фоне
    //------------------------------------------------------------
    static fs::path cmdExePath()
    {
        wchar_t sysDir[MAX_PATH]{};
        UINT n = GetSystemDirectoryW(sysDir, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return fs::path(L"cmd.exe");
        return fs::path(sysDir) / L"cmd.exe";
    }

    static std::wstring q(const std::wstring& s)
    {
        return L"\"" + s + L"\"";
    }

    static bool deferRemoveWithCmd(const fs::path& installDir,
        const fs::path& dataRoot,
        std::string* error)
    {
        std::wstring inner = L"timeout /t 2 /nobreak >nul";

        if (!installDir.empty() && !isDangerousPath(installDir))
            inner += L" & rmdir /s /q " + q(installDir.wstring());

        if (!dataRoot.empty() && !isDangerousPath(dataRoot))
            inner += L" & rmdir /s /q " + q(dataRoot.wstring());

        // cmd.exe /C ""timeout ... & rmdir ... ""
        std::wstring cmdLine = q(cmdExePath().wstring()) + L" /C \"\"" + inner + L"\"\"";

        std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
        buf.push_back(L'\0');

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        BOOL ok = CreateProcessW(
            nullptr,
            buf.data(),
            nullptr, nullptr,
            FALSE,
            CREATE_NO_WINDOW | DETACHED_PROCESS,
            nullptr,
            nullptr,
            &si,
            &pi
        );

        if (!ok)
        {
            if (error)
                *error = "CreateProcessW(cmd.exe) failed, error=" + std::to_string((int)GetLastError());
            return false;
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    //------------------------------------------------------------
    //  Публичные функции из Platform.hpp
    //------------------------------------------------------------
    bool removeInstallDir(const fs::path& installDir, std::string* error,bool fromInno)
    {
        if (fromInno)
            return true; // Inno ��� ������ {app}, � helper �� ������� installDir
        std::string err;
        if (removeTreeNow(installDir, &err))
            return true;

        std::string err2;
        if (deferRemoveWithCmd(installDir, fs::path{}, &err2))
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
        if (deferRemoveWithCmd(fs::path{}, dataRoot, &err2))
            return true;

        if (error) *error = err + "; defer failed: " + err2;
        return false;
    }

} // namespace svcinst::platform

#endif // _WIN32
