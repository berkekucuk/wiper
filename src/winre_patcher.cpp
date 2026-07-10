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
     * @brief Runs a command in the current console (output visible) and waits.
     *
     * @param cmd       Command line to execute.
     * @param timeoutMs Maximum wait in milliseconds.
     * @return true  Exited with code 0.
     * @return false Could not start, timed out, or non-zero exit.
     */
    bool RunVisible(const std::string& cmd, DWORD timeoutMs = 120'000)
    {
        STARTUPINFOA        si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);

        std::string buf = cmd;
        BOOL created = CreateProcessA(
            nullptr, buf.data(),
            nullptr, nullptr,
            TRUE, 0,            // inherit handles → output in our console
            nullptr, nullptr,
            &si, &pi);

        if (!created)
        {
            std::cerr << "  [ERROR] CreateProcess failed: " << cmd
                      << " (code " << GetLastError() << ")\n";
            return false;
        }

        WaitForSingleObject(pi.hProcess, timeoutMs);

        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return (exitCode == 0);
    }

    /** @brief Builds %TEMP%\winre_mount_<PID>. */
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

    /** @brief Scans A-Z for <drive>:\Recovery\WindowsRE\winre.wim. */
    std::string ScanDrivesForWim()
    {
        for (char d = 'A'; d <= 'Z'; ++d)
        {
            std::string p;
            p += d;
            p += ":\\Recovery\\WindowsRE\\winre.wim";
            if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES)
                return p;
        }
        return {};
    }

    /**
     * @brief Resolves the full path to winre.wim.
     *        Tries the provided directory first, then scans drives as fallback.
     */
    std::string ResolveWimPath(const std::string& winreDir)
    {
        auto exists = [](const std::string& p) {
            return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES;
        };

        // Already a .wim path?
        constexpr std::string_view ext = ".wim";
        if (winreDir.size() >= ext.size() &&
            winreDir.compare(winreDir.size() - ext.size(), ext.size(), ext) == 0 &&
            exists(winreDir))
        {
            return winreDir;
        }

        // Append winre.wim to directory
        std::string candidate = winreDir;
        if (!candidate.empty() && candidate.back() != '\\')
            candidate += '\\';
        candidate += "winre.wim";

        if (exists(candidate))
            return candidate;

        // Fallback: scan drive letters
        std::cout << "  Primary path not directly accessible; scanning drives...\n";
        return ScanDrivesForWim();
    }

    /**
     * @brief Writes the diskpart command script into the mounted WinRE image.
     *        Path inside the image: Windows\System32\wipe_script.txt
     *        At WinRE runtime this becomes: X:\Windows\System32\wipe_script.txt
     *
     * @param mountDir      Root of the mounted WinRE image.
     * @param diskNumber     Disk to target.
     * @param partitionNumber Partition to format.
     * @return true  File written successfully.
     * @return false File could not be created.
     */
    bool WriteWipeScript(const std::string& mountDir,
                         int diskNumber, int partitionNumber)
    {
        std::string path = mountDir + "\\Windows\\System32\\wipe_script.txt";
        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open())
        {
            std::cerr << "  [ERROR] Cannot create wipe_script.txt\n";
            return false;
        }

        out << "select disk "      << diskNumber      << "\r\n";
        out << "select partition " << partitionNumber << "\r\n";
        out << "format fs=ntfs quick label=\"WIPED\"\r\n";
        out << "exit\r\n";

        std::cout << "  wipe_script.txt written:\n"
                  << "    select disk "      << diskNumber      << "\n"
                  << "    select partition " << partitionNumber << "\n"
                  << "    format fs=ntfs quick label=\"WIPED\"\n"
                  << "    exit\n\n";
        return true;
    }

    /**
     * @brief Writes the launcher batch script into the mounted WinRE image.
     *        Path inside the image: Windows\System32\wipe_launch.cmd
     *        At WinRE runtime: X:\Windows\System32\wipe_launch.cmd
     *
     *        Flow:
     *          1. Print a status banner.
     *          2. Run diskpart /s wipe_script.txt  (no user input needed).
     *          3. Wait 5 seconds.
     *          4. wpeutil reboot  → restart back into Windows.
     *
     * @param mountDir      Root of the mounted WinRE image.
     * @param diskNumber     Disk number shown in the banner.
     * @param partitionNumber Partition number shown in the banner.
     * @return true  File written successfully.
     * @return false File could not be created.
     */
    bool WriteLaunchCmd(const std::string& mountDir,
                        int diskNumber, int partitionNumber)
    {
        std::string path = mountDir + "\\Windows\\System32\\wipe_launch.cmd";
        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open())
        {
            std::cerr << "  [ERROR] Cannot create wipe_launch.cmd\n";
            return false;
        }

        out << "@echo off\r\n";
        out << "echo.\r\n";
        out << "echo  =====================================================\r\n";
        out << "echo   WinRE Auto-Wipe  ^|  Disk " << diskNumber
            << "  Partition " << partitionNumber << "\r\n";
        out << "echo  =====================================================\r\n";
        out << "echo.\r\n";
        out << "echo  Running diskpart wipe script...\r\n";
        out << "echo  Target: Disk " << diskNumber
            << "  /  Partition " << partitionNumber << "\r\n";
        out << "echo.\r\n";
        // Run diskpart non-interactively with the embedded script
        out << "diskpart /s X:\\Windows\\System32\\wipe_script.txt\r\n";
        out << "echo.\r\n";
        out << "echo  Wipe complete. Rebooting in 5 seconds...\r\n";
        // ping delay: n+1 pings ≈ n seconds
        out << "ping 127.0.0.1 -n 6 > nul\r\n";
        out << "wpeutil reboot\r\n";

        std::cout << "  wipe_launch.cmd written "
                     "(diskpart /s → wpeutil reboot).\n\n";
        return true;
    }

    /**
     * @brief Updates winpeshl.ini inside the mounted WinRE image to launch
     *        wipe_launch.cmd instead of the standard recovery UI.
     *
     * @param mountDir Root of the mounted WinRE image.
     * @return true  File written successfully.
     * @return false File could not be opened for writing.
     */
    bool PatchWinpeshl(const std::string& mountDir)
    {
        std::string iniPath = mountDir + "\\Windows\\System32\\winpeshl.ini";

        // Show current content
        std::string original;
        {
            std::ifstream in(iniPath);
            if (in.is_open())
            {
                std::ostringstream oss;
                oss << in.rdbuf();
                original = oss.str();
            }
        }
        std::cout << "  --- winpeshl.ini (before) ---\n"
                  << (original.empty() ? "  (empty)\n" : original) << "\n";

        std::ofstream out(iniPath, std::ios::trunc);
        if (!out.is_open())
        {
            std::cerr << "  [ERROR] Cannot open winpeshl.ini for writing.\n";
            return false;
        }

        // Launch cmd.exe running our batch script; X:\ is the WinRE RAM drive
        out << "[LaunchApps]\r\n";
        out << "X:\\Windows\\System32\\cmd.exe, /c X:\\Windows\\System32\\wipe_launch.cmd\r\n";

        std::cout << "  --- winpeshl.ini (after) ---\n"
                  << "  [LaunchApps]\n"
                  << "  X:\\Windows\\System32\\cmd.exe, "
                     "/c X:\\Windows\\System32\\wipe_launch.cmd\n\n";
        return true;
    }

    /** @brief Best-effort discard of a failed DISM mount. */
    void DiscardMount(const std::string& mountDir)
    {
        RunVisible(
            "dism.exe /Unmount-Image /MountDir:\"" + mountDir + "\" /Discard",
            60'000);
        RemoveDirectoryA(mountDir.c_str());
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// WinREPatcher interface implementation
// ---------------------------------------------------------------------------
namespace WinREPatcher
{
    bool Patch(const std::string& winreDir, int diskNumber, int partitionNumber)
    {
        // ── Resolve winre.wim ─────────────────────────────────────────────────
        std::cout << "  Resolving winre.wim from: " << winreDir << "\n";
        const std::string wimPath = ResolveWimPath(winreDir);

        if (wimPath.empty())
        {
            std::cerr << "  [ERROR] winre.wim not found.\n";
            return false;
        }

        const std::string mountDir = BuildMountDir();

        std::cout << "  WinRE image : " << wimPath  << "\n";
        std::cout << "  Mount dir   : " << mountDir << "\n";
        std::cout << "  Wipe target : Disk " << diskNumber
                  << "  /  Partition " << partitionNumber << "\n\n";

        // ── Step 1: Create mount directory ────────────────────────────────────
        std::cout << "  [1/4] Creating mount directory...\n";
        if (!CreateDirectoryA(mountDir.c_str(), nullptr))
        {
            std::cerr << "  [ERROR] CreateDirectory failed "
                      << "(code " << GetLastError() << ")\n";
            return false;
        }

        // ── Step 2: Mount via DISM ────────────────────────────────────────────
        std::cout << "  [2/4] Mounting WinRE image via DISM "
                     "(may take up to 60 s)...\n\n";

        std::string mountCmd =
            "dism.exe /Mount-Image"
            " /ImageFile:\"" + wimPath + "\""
            " /Index:1"
            " /MountDir:\"" + mountDir + "\"";

        if (!RunVisible(mountCmd, 120'000))
        {
            std::cerr << "\n  [ERROR] DISM mount failed.\n";
            RemoveDirectoryA(mountDir.c_str());
            return false;
        }
        std::cout << "\n  Image mounted.\n\n";

        // ── Step 3: Embed wipe_script.txt and wipe_launch.cmd ─────────────────
        std::cout << "  [3/4] Embedding wipe scripts...\n\n";

        if (!WriteWipeScript(mountDir, diskNumber, partitionNumber) ||
            !WriteLaunchCmd (mountDir, diskNumber, partitionNumber))
        {
            std::cerr << "  [ERROR] Failed to write scripts. Discarding mount.\n";
            DiscardMount(mountDir);
            return false;
        }

        // ── Step 4: Patch winpeshl.ini ────────────────────────────────────────
        std::cout << "  [4/4] Patching winpeshl.ini...\n";
        if (!PatchWinpeshl(mountDir))
        {
            std::cerr << "  [ERROR] winpeshl.ini patch failed. Discarding mount.\n";
            DiscardMount(mountDir);
            return false;
        }

        // ── Commit and unmount ────────────────────────────────────────────────
        std::cout << "  Committing and unmounting (may take up to 60 s)...\n\n";

        std::string unmountCmd =
            "dism.exe /Unmount-Image"
            " /MountDir:\"" + mountDir + "\""
            " /Commit";

        if (!RunVisible(unmountCmd, 120'000))
        {
            std::cerr << "\n  [ERROR] DISM commit failed.\n"
                         "          Run 'dism /Cleanup-Mountpoints' to clean up.\n";
            RemoveDirectoryA(mountDir.c_str());
            return false;
        }

        std::cout << "\n  winre.wim patched and committed.\n\n";

        RemoveDirectoryA(mountDir.c_str());
        return true;
    }
} // namespace WinREPatcher
