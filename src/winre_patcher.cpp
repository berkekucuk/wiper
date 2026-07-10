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
     * @brief Runs a command in the current console (output is visible to the user)
     *        and waits for it to finish.
     *
     * @param cmd       Command line string to execute.
     * @param timeoutMs Maximum time to wait in milliseconds.
     * @return true  Process exited with code 0.
     * @return false Process could not be created, timed out, or exited non-zero.
     */
    bool RunVisible(const std::string& cmd, DWORD timeoutMs = 120'000)
    {
        STARTUPINFOA        si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);

        // Inherit the current console handles so DISM output is shown inline.
        std::string buf = cmd;
        BOOL created = CreateProcessA(
            nullptr,
            buf.data(),
            nullptr, nullptr,
            TRUE,              // inherit handles → output appears in our console
            0,                 // no CREATE_NO_WINDOW — we want to see output
            nullptr, nullptr,
            &si, &pi);

        if (!created)
        {
            std::cerr << "  [ERROR] CreateProcess failed for: " << cmd
                      << "  (code " << GetLastError() << ")\n";
            return false;
        }

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
     * @brief Scans all available drive letters (A–Z) for the standard WinRE image
     *        path: <drive>:\Recovery\WindowsRE\winre.wim
     *
     * @return Full path to winre.wim if found, or an empty string.
     */
    std::string ScanDrivesForWim()
    {
        for (char drive = 'A'; drive <= 'Z'; ++drive)
        {
            std::string candidate;
            candidate += drive;
            candidate += ":\\Recovery\\WindowsRE\\winre.wim";

            if (GetFileAttributesA(candidate.c_str()) != INVALID_FILE_ATTRIBUTES)
                return candidate;
        }
        return {};
    }

    /**
     * @brief Resolves the path to winre.wim given the WinRE directory path.
     *
     * Priority:
     *   1. If the input already ends in ".wim" and the file exists → use it.
     *   2. Append "winre.wim" to the directory; if the file exists → use it.
     *   3. Scan all drive letters for <drive>:\Recovery\WindowsRE\winre.wim.
     *
     * @param winreDir Directory path from WinREFinder (may be a device path).
     * @return Resolved path, or empty string if not found.
     */
    std::string ResolveWimPath(const std::string& winreDir)
    {
        // Helper: check if a file exists
        auto exists = [](const std::string& p) {
            return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES;
        };

        // 1. Already a .wim path?
        constexpr std::string_view ext = ".wim";
        if (winreDir.size() >= ext.size() &&
            winreDir.compare(winreDir.size() - ext.size(), ext.size(), ext) == 0 &&
            exists(winreDir))
        {
            return winreDir;
        }

        // 2. Append winre.wim to the directory
        std::string candidate = winreDir;
        if (!candidate.empty() && candidate.back() != '\\')
            candidate += '\\';
        candidate += "winre.wim";

        if (exists(candidate))
            return candidate;

        std::cout << "  Primary path not accessible; scanning drives...\n";

        // 3. Scan drive letters
        return ScanDrivesForWim();
    }

    /**
     * @brief Prepends `cmd.exe /k <command>` to startnet.cmd inside the mounted image.
     *
     * @param mountDir Root of the mounted WinRE image.
     * @param command  Command to inject.
     * @return true  File written successfully.
     * @return false File could not be opened for writing.
     */
    bool InjectIntoStartnet(const std::string& mountDir, const std::string& command)
    {
        std::string startnetPath = mountDir + "\\Windows\\System32\\startnet.cmd";

        // Read existing content (typically just "wpeinit")
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

        // Print what we are about to write for transparency
        std::cout << "  --- startnet.cmd (before) ---\n" << existing << "\n";

        std::ofstream out(startnetPath, std::ios::trunc);
        if (!out.is_open())
            return false;

        out << "cmd.exe /k " << command << "\r\n";
        out << existing;
        out.close();

        // Confirm
        std::cout << "  --- startnet.cmd (after) ---\n"
                  << "  cmd.exe /k " << command << "\n"
                  << existing << "\n";

        return true;
    }

    /**
     * @brief Attempts to discard a failed DISM mount as a cleanup step.
     */
    void DiscardMount(const std::string& mountDir)
    {
        std::string cmd = "dism.exe /Unmount-Image /MountDir:\"" + mountDir + "\" /Discard";
        RunVisible(cmd, 60'000);
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
        // ── Resolve the .wim file path ────────────────────────────────────────
        std::cout << "  Resolving winre.wim path from: " << winreDir << "\n";
        const std::string wimPath = ResolveWimPath(winreDir);

        if (wimPath.empty())
        {
            std::cerr << "  [ERROR] winre.wim could not be found.\n"
                         "          Searched the provided path and drives A-Z.\n";
            return false;
        }

        const std::string mountDir = BuildMountDir();

        std::cout << "  WinRE image : " << wimPath  << "\n";
        std::cout << "  Mount dir   : " << mountDir << "\n\n";

        // ── Step 1: Create mount directory ────────────────────────────────────
        std::cout << "  [1/3] Creating mount directory...\n";
        if (!CreateDirectoryA(mountDir.c_str(), nullptr))
        {
            std::cerr << "  [ERROR] Could not create mount directory: "
                      << mountDir << "  (code " << GetLastError() << ")\n";
            return false;
        }

        // ── Step 2: Mount via DISM ────────────────────────────────────────────
        std::cout << "  [2/3] Mounting WinRE image via DISM "
                     "(this may take up to 60 seconds)...\n\n";

        std::string mountCmd =
            "dism.exe /Mount-Image"
            " /ImageFile:\"" + wimPath + "\""
            " /Index:1"
            " /MountDir:\"" + mountDir + "\"";

        std::cout << "  CMD: " << mountCmd << "\n\n";

        if (!RunVisible(mountCmd, 120'000))
        {
            std::cerr << "\n  [ERROR] DISM mount failed (see output above).\n"
                         "          Ensure the program is running as Administrator.\n";
            RemoveDirectoryA(mountDir.c_str());
            return false;
        }
        std::cout << "\n  Image mounted successfully.\n\n";

        // ── Step 3: Patch startnet.cmd ────────────────────────────────────────
        std::cout << "  [3/3] Patching startnet.cmd...\n";
        if (!InjectIntoStartnet(mountDir, command))
        {
            std::cerr << "  [ERROR] Could not write startnet.cmd. Discarding mount.\n";
            DiscardMount(mountDir);
            return false;
        }

        // ── Commit and unmount ────────────────────────────────────────────────
        std::cout << "  Committing and unmounting "
                     "(this may take up to 60 seconds)...\n\n";

        std::string unmountCmd =
            "dism.exe /Unmount-Image"
            " /MountDir:\"" + mountDir + "\""
            " /Commit";

        std::cout << "  CMD: " << unmountCmd << "\n\n";

        if (!RunVisible(unmountCmd, 120'000))
        {
            std::cerr << "\n  [ERROR] DISM commit failed (see output above).\n"
                         "          Run 'dism /Cleanup-Mountpoints' to clean up.\n";
            RemoveDirectoryA(mountDir.c_str());
            return false;
        }

        std::cout << "\n  winre.wim patched and committed successfully.\n\n";

        RemoveDirectoryA(mountDir.c_str());
        return true;
    }
} // namespace WinREPatcher
