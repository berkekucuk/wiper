#include "reboot.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    /**
     * @brief Grants SE_SHUTDOWN_NAME privilege to the current process token.
     *        This privilege is required for InitiateSystemShutdownEx /
     *        ExitWindowsEx to succeed.
     *
     * @return true  Privilege was granted successfully.
     * @return false Privilege could not be granted.
     */
    bool EnableShutdownPrivilege()
    {
        HANDLE           hToken = nullptr;
        TOKEN_PRIVILEGES tp     = {};
        LUID             luid   = {};

        if (!OpenProcessToken(GetCurrentProcess(),
                              TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                              &hToken))
            return false;

        if (!LookupPrivilegeValueA(nullptr, SE_SHUTDOWN_NAME, &luid))
        {
            CloseHandle(hToken);
            return false;
        }

        tp.PrivilegeCount           = 1;
        tp.Privileges[0].Luid       = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        bool ok = (AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp),
                                         nullptr, nullptr) == TRUE)
                  && (GetLastError() == ERROR_SUCCESS);
        CloseHandle(hToken);
        return ok;
    }

    /**
     * @brief Sets the one-time WinRE boot flag via `reagentc /boottore`.
     *
     * @return true  Command completed successfully (exit code 0).
     * @return false Command failed.
     */
    bool SetWinREBootFlag()
    {
        // reagentc /boottore → boot into WinRE on the next restart
        STARTUPINFOA        si = {};
        PROCESS_INFORMATION pi = {};
        si.cb                  = sizeof(si);

        // Launched via CreateProcess because it requires administrator rights
        char cmdLine[] = "reagentc.exe /boottore";

        BOOL created = CreateProcessA(
            nullptr,
            cmdLine,
            nullptr, nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr, nullptr,
            &si, &pi);

        if (!created)
            return false;

        WaitForSingleObject(pi.hProcess, 10'000); // wait at most 10 seconds

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return (exitCode == 0);
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// Reboot interface implementation
// ---------------------------------------------------------------------------
namespace Reboot
{
    bool BootIntoWinRE()
    {
        // 1. Acquire shutdown privilege
        if (!EnableShutdownPrivilege())
        {
            std::cerr << "  [ERROR] Failed to acquire shutdown privilege. "
                         "Run the program as Administrator.\n";
            return false;
        }

        // 2. Set the one-time WinRE boot flag
        std::cout << "  Sending reagentc /boottore...\n";
        if (!SetWinREBootFlag())
        {
            std::cerr << "  [ERROR] reagentc /boottore failed.\n"
                         "          Make sure the program is running as Administrator\n"
                         "          and that WinRE is enabled on this system.\n";
            return false;
        }
        std::cout << "  WinRE boot flag set successfully.\n";

        // 3. Restart the system
        std::cout << "  Restarting system...\n";

        // Try ExitWindowsEx first (EWX_REBOOT | EWX_FORCE)
        if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE,
                           SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER))
        {
            // Fallback: InitiateSystemShutdownEx
            if (!InitiateSystemShutdownExA(
                    nullptr,          // local machine
                    nullptr,          // no message
                    0,                // no delay
                    TRUE,             // force close apps
                    TRUE,             // restart
                    SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER))
            {
                std::cerr << "  [ERROR] Failed to send restart command. "
                             "Error code: " << GetLastError() << "\n";
                return false;
            }
        }

        return true;
    }
} // namespace Reboot
