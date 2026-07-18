#include "dism_utils.h"

#include <iostream>

using namespace std;

namespace DISMUtils
{
    bool RunVisible(const string& cmd, DWORD timeoutMs)
    {
        STARTUPINFOA        si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);

        string buf = cmd;
        BOOL created = CreateProcessA(
            nullptr, buf.data(),
            nullptr, nullptr,
            TRUE, 0,            // inherit handles → output in our console
            nullptr, nullptr,
            &si, &pi);

        if (!created)
        {
            cerr << "  [ERROR] CreateProcess failed: " << cmd
                      << " (code " << GetLastError() << ")\n";
            return false;
        }

        DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);

        if (waitResult == WAIT_TIMEOUT)
        {
            cerr << "  [ERROR] Process timed out after " << (timeoutMs / 1000)
                      << " s. Forcibly terminating: " << cmd << "\n";
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return false;
        }

        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // STILL_ACTIVE (259) means the process hasn't actually exited — treat as failure.
        if (exitCode == STILL_ACTIVE)
        {
            cerr << "  [ERROR] Process reported STILL_ACTIVE after wait: " << cmd << "\n";
            return false;
        }

        return (exitCode == 0);
    }

    string BuildMountDir()
    {
        char tmp[MAX_PATH] = {};
        GetTempPathA(sizeof(tmp), tmp);

        string dir = tmp;
        if (!dir.empty() && dir.back() == '\\')
            dir.pop_back();

        dir += "\\winre_mount_";
        dir += to_string(GetCurrentProcessId());
        return dir;
    }

    void DiscardMount(const string& mountDir)
    {
        RunVisible(
            "dism.exe /Unmount-Image /MountDir:\"" + mountDir + "\" /Discard",
            60'000);
        RemoveDirectoryA(mountDir.c_str());
    }

    bool MountImage(const string& wimPath, const string& mountDir)
    {
        string mountCmd =
            "dism.exe /Mount-Image"
            " /ImageFile:\"" + wimPath + "\""
            " /Index:1"
            " /MountDir:\"" + mountDir + "\"";

        if (!RunVisible(mountCmd, 120'000))
        {
            cerr << "\n  [ERROR] DISM mount failed.\n";
            RemoveDirectoryA(mountDir.c_str());
            return false;
        }
        return true;
    }

    bool CommitAndUnmount(const string& mountDir)
    {
        string unmountCmd =
            "dism.exe /Unmount-Image"
            " /MountDir:\"" + mountDir + "\""
            " /Commit";

        if (!RunVisible(unmountCmd, 120'000))
        {
            cerr << "\n  [ERROR] DISM commit failed.\n"
                         "          Run 'dism /Cleanup-Mountpoints' to clean up.\n";
            RemoveDirectoryA(mountDir.c_str());
            return false;
        }
        
        RemoveDirectoryA(mountDir.c_str());
        return true;
    }
}
