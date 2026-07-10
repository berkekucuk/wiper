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
     * @brief Runs a command in the current console (output visible to the user)
     *        and waits for it to finish.
     *
     * @param cmd       Command line to execute.
     * @param timeoutMs Maximum time to wait in milliseconds.
     * @return true  Process exited with code 0.
     * @return false Could not start, timed out, or non-zero exit.
     */
    bool RunVisible(const std::string& cmd, DWORD timeoutMs = 120'000)
    {
        STARTUPINFOA        si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);

        std::string buf = cmd;
        BOOL created = CreateProcessA(
            nullptr,
            buf.data(),
            nullptr, nullptr,
            TRUE,   // inherit handles → DISM output appears in our console
            0,
            nullptr, nullptr,
            &si, &pi);

        if (!created)
        {
            std::cerr << "  [ERROR] CreateProcess failed: " << cmd
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
     * @brief Builds a unique DISM mount point path under %TEMP%.
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
     * @brief Scans drive letters A-Z for <drive>:\Recovery\WindowsRE\winre.wim
     *        as a fallback when the device path is not directly accessible.
     */
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

        // Append winre.wim
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
     * @brief Patches winpeshl.ini inside the mounted WinRE image.
     *
     * WinRE's startup is controlled by winpeshl.ini, NOT startnet.cmd.
     * The default entry launches reagent.exe (the recovery GUI).
     * We replace it with cmd.exe /k <command> so the terminal opens first.
     * After the user types 'exit', the WinRE session ends and Windows resumes.
     *
     * @param mountDir Root of the mounted WinRE image.
     * @param command  Command to run inside the WinRE shell.
     * @return true  File written successfully.
     * @return false File could not be opened for writing.
     */
    bool PatchWinpeshl(const std::string& mountDir, const std::string& command)
    {
        std::string iniPath = mountDir + "\\Windows\\System32\\winpeshl.ini";

        // Read current content for display
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

        // Guard: already patched with the same command?
        if (original.find("cmd.exe, /k " + command) != std::string::npos)
        {
            std::cout << "  winpeshl.ini already contains the target command. "
                         "No change needed.\n";
            return true;
        }

        std::cout << "  --- winpeshl.ini (before) ---\n"
                  << (original.empty() ? "  (empty / not found)\n" : original)
                  << "\n";

        // Write patched version — replace entire file
        std::ofstream out(iniPath, std::ios::trunc);
        if (!out.is_open())
        {
            std::cerr << "  [ERROR] Cannot open winpeshl.ini for writing.\n"
                         "          Path: " << iniPath << "\n";
            return false;
        }

        out << "[LaunchApps]\r\n";
        out << "cmd.exe, /k " << command << "\r\n";

        std::cout << "  --- winpeshl.ini (after) ---\n"
                  << "  [LaunchApps]\n"
                  << "  cmd.exe, /k " << command << "\n\n";

        return true;
    }

    /** @brief Best-effort: discards a failed DISM mount and removes the directory. */
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
    bool Patch(const std::string& winreDir, const std::string& command)
    {
        // ── Resolve .wim path ─────────────────────────────────────────────────
        std::cout << "  Resolving winre.wim from: " << winreDir << "\n";
        const std::string wimPath = ResolveWimPath(winreDir);

        if (wimPath.empty())
        {
            std::cerr << "  [ERROR] winre.wim not found on any drive.\n";
            return false;
        }

        const std::string mountDir = BuildMountDir();

        std::cout << "  WinRE image : " << wimPath  << "\n";
        std::cout << "  Mount dir   : " << mountDir << "\n\n";

        // ── Step 1: Create mount directory ────────────────────────────────────
        std::cout << "  [1/3] Creating mount directory...\n";
        if (!CreateDirectoryA(mountDir.c_str(), nullptr))
        {
            std::cerr << "  [ERROR] Could not create mount directory "
                      << "(code " << GetLastError() << ")\n";
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

        if (!RunVisible(mountCmd, 120'000))
        {
            std::cerr << "\n  [ERROR] DISM mount failed.\n";
            RemoveDirectoryA(mountDir.c_str());
            return false;
        }
        std::cout << "\n  Image mounted successfully.\n\n";

        // ── Step 3: Patch winpeshl.ini ────────────────────────────────────────
        // winpeshl.ini controls what WinRE launches on boot (NOT startnet.cmd).
        // The default entry starts reagent.exe (the recovery GUI); we replace it
        // with cmd.exe so the terminal opens immediately.
        std::cout << "  [3/3] Patching winpeshl.ini...\n";
        if (!PatchWinpeshl(mountDir, command))
        {
            std::cerr << "  [ERROR] winpeshl.ini patch failed. Discarding mount.\n";
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

        if (!RunVisible(unmountCmd, 120'000))
        {
            std::cerr << "\n  [ERROR] DISM commit failed.\n"
                         "          Run 'dism /Cleanup-Mountpoints' to clean up.\n";
            RemoveDirectoryA(mountDir.c_str());
            return false;
        }

        std::cout << "\n  winre.wim patched and committed successfully.\n\n";

        RemoveDirectoryA(mountDir.c_str());
        return true;
    }
} // namespace WinREPatcher
