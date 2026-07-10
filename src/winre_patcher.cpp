#include "winre_patcher.h"

#include <windows.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    /**
     * @brief Runs a command invisibly and waits for it to exit.
     *
     * @param cmd       Command line to execute.
     * @param timeoutMs Maximum wait time in milliseconds.
     * @return true  Command exited with code 0.
     * @return false Process could not be created, timed out, or returned non-zero.
     */
    bool RunAndWait(const std::string& cmd, DWORD timeoutMs = 120'000)
    {
        STARTUPINFOA        si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);

        // CreateProcessA requires a mutable buffer
        std::string buf = cmd;

        BOOL created = CreateProcessA(
            nullptr,
            buf.data(),
            nullptr, nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr, nullptr,
            &si, &pi);

        if (!created)
            return false;

        WaitForSingleObject(pi.hProcess, timeoutMs);

        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return (exitCode == 0);
    }

    /**
     * @brief Builds a unique temp directory path for the DISM mount point.
     *        Pattern: %TEMP%\winre_mount_<PID>
     */
    std::string BuildMountDir()
    {
        char tmp[MAX_PATH] = {};
        GetTempPathA(sizeof(tmp), tmp);

        std::string dir = tmp;
        if (!dir.empty() && dir.back() == '\\')
            dir.pop_back();

        dir += "\\winre_mount_";
        dir += std::to_string(GetCurrentProcessId());
        return dir;
    }

    /**
     * @brief Resolves the full path to winre.wim from the WinRE directory path.
     *        Appends "winre.wim" if the path does not already end in ".wim".
     */
    std::string ResolveWimPath(const std::string& winreDir)
    {
        constexpr std::string_view ext = ".wim";
        if (winreDir.size() >= ext.size() &&
            winreDir.compare(winreDir.size() - ext.size(), ext.size(), ext) == 0)
        {
            return winreDir; // already a .wim path
        }

        std::string wim = winreDir;
        if (!wim.empty() && wim.back() != '\\')
            wim += '\\';
        wim += "winre.wim";
        return wim;
    }

    /**
     * @brief Prepends `cmd.exe /k <command>` to startnet.cmd inside the mounted image.
     *
     * @param mountDir  Root of the mounted WinRE image.
     * @param command   Command to inject.
     * @return true  File written successfully.
     * @return false Could not open the file for writing.
     */
    bool InjectIntoStartnet(const std::string& mountDir, const std::string& command)
    {
        std::string startnetPath = mountDir + "\\Windows\\System32\\startnet.cmd";

        // Read the existing content (typically just "wpeinit")
        std::string existing;
        {
            std::ifstream in(startnetPath);
            if (in.is_open())
            {
                std::ostringstream oss;
                oss << in.rdbuf();
                existing = oss.str();
            }
        }

        // Write the patched version
        std::ofstream out(startnetPath, std::ios::trunc);
        if (!out.is_open())
            return false;

        out << "cmd.exe /k " << command << "\r\n";
        out << existing;

        return true;
    }

    /**
     * @brief Attempts to unmount and discard all pending DISM mount points.
     *        Called as a best-effort cleanup on failure paths.
     *
     * @param mountDir Mount directory to discard.
     */
    void DiscardMount(const std::string& mountDir)
    {
        std::string cmd = "dism.exe /Unmount-Image /MountDir:\"" + mountDir + "\" /Discard";
        RunAndWait(cmd, 120'000);
        RemoveDirectoryA(mountDir.c_str());
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// WinREPatcher interface implementation
// ---------------------------------------------------------------------------
namespace WinREPatcher
{
    bool Patch(const std::string& winreDir, const std::string& command)
    {
        const std::string wimPath  = ResolveWimPath(winreDir);
        const std::string mountDir = BuildMountDir();

        std::cout << "  WinRE image : " << wimPath  << "\n";
        std::cout << "  Mount dir   : " << mountDir << "\n\n";

        // ── Step 1: Create the mount directory ───────────────────────────────
        std::cout << "  [1/3] Creating mount directory...\n";
        if (!CreateDirectoryA(mountDir.c_str(), nullptr))
        {
            std::cerr << "  [ERROR] Could not create mount directory.\n"
                         "          Path: " << mountDir << "\n";
            return false;
        }

        // ── Step 2: Mount the WinRE image via DISM ───────────────────────────
        std::cout << "  [2/3] Mounting WinRE image via DISM "
                     "(this may take up to 60 seconds)...\n";

        std::string mountCmd =
            "dism.exe /Mount-Image"
            " /ImageFile:\"" + wimPath + "\""
            " /Index:1"
            " /MountDir:\"" + mountDir + "\"";

        if (!RunAndWait(mountCmd, 120'000))
        {
            std::cerr << "  [ERROR] DISM mount failed.\n"
                         "          Verify the image path and that the program\n"
                         "          is running as Administrator.\n";
            RemoveDirectoryA(mountDir.c_str());
            return false;
        }
        std::cout << "  Image mounted successfully.\n\n";

        // ── Step 3: Patch startnet.cmd ────────────────────────────────────────
        std::cout << "  [3/3] Patching startnet.cmd...\n";
        if (!InjectIntoStartnet(mountDir, command))
        {
            std::cerr << "  [ERROR] Could not write to startnet.cmd. Discarding mount.\n";
            DiscardMount(mountDir);
            return false;
        }
        std::cout << "  Injected: cmd.exe /k " << command << "\n\n";

        // ── Commit + unmount ──────────────────────────────────────────────────
        std::cout << "  Committing and unmounting "
                     "(this may take up to 60 seconds)...\n";

        std::string unmountCmd =
            "dism.exe /Unmount-Image"
            " /MountDir:\"" + mountDir + "\""
            " /Commit";

        if (!RunAndWait(unmountCmd, 120'000))
        {
            std::cerr << "  [ERROR] DISM commit failed.\n"
                         "          Run 'dism /Cleanup-Mountpoints' to clean up manually.\n";
            RemoveDirectoryA(mountDir.c_str());
            return false;
        }
        std::cout << "  winre.wim patched and committed.\n\n";

        RemoveDirectoryA(mountDir.c_str());
        return true;
    }
} // namespace WinREPatcher
