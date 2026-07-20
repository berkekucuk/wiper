#include "winre_script_generator.h"
#include "cli_utils.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    // Path prefix for files embedded inside the mounted WinRE image.
    // At boot time WinRE maps its System32 as X:\Windows\System32.
    constexpr const char* kWinRESys32 = "X:\\Windows\\System32\\";

    bool WriteFile(const std::string& path, const std::string& content)
    {
        std::ofstream s(path, std::ios::trunc);
        if (!s.is_open()) return false;
        s << content;
        return true;
    }

    std::string GetPrepScript(int diskNumber)
    {
        return "select disk " + std::to_string(diskNumber) + "\r\n"
               "clean\r\n"
               "create partition primary\r\n"
               "assign letter=Z\r\n"
               "exit\r\n";
    }

    std::string GetCleanupScript(int diskNumber)
    {
        return "select disk " + std::to_string(diskNumber) + "\r\n"
               "clean\r\n"
               "exit\r\n";
    }

    struct WipeStep
    {
        std::string description;
        std::string commands;
    };

    bool BuildAndWriteLaunchCmd(const std::string& path,
                                int diskNumber,
                                const std::string& title,
                                const std::string& warning,
                                const std::vector<WipeStep>& steps)
    {
        std::ostringstream s;
        s << "@echo off\r\n"
          << "echo.\r\n"
          << "echo  ====================================================\r\n"
          << "echo   WinRE Full Disk Wipe (" << title << ") ^| Disk " << diskNumber << "\r\n"
          << "echo   " << warning << "\r\n"
          << "echo  ====================================================\r\n"
          << "echo.\r\n";

        int totalSteps = static_cast<int>(steps.size());
        
        for (size_t i = 0; i < steps.size(); ++i)
        {
            s << "echo  [" << (i + 1) << "/" << totalSteps << "] " << steps[i].description << "\r\n"
              << steps[i].commands
              << "echo.\r\n";
        }

        s << "echo  Wipe complete. Shutting down in 5 seconds...\r\n"
          << "ping 127.0.0.1 -n 6 > nul\r\n"
          << "wpeutil shutdown\r\n";

        return WriteFile(path, s.str());
    }
} // namespace

// ---------------------------------------------------------------------------
// WinREScriptGenerator interface implementation
// ---------------------------------------------------------------------------
namespace WinREScriptGenerator
{
    bool GenerateDiskWipeScripts(const std::string& mountDir, int diskNumber, WinREPatcher::WipeStrategy strategy)
    {
        // Validate disk number range before writing any scripts.
        // diskpart accepts 0-127; anything outside is invalid.
        if (diskNumber < 0 || diskNumber > 127)
        {
            std::cerr << "  [ERROR] Invalid disk number: " << diskNumber
                      << ". Must be in range 0-127.\n";
            return false;
        }

        const std::string sys32 = mountDir + "\\Windows\\System32\\";
        const std::string launchCmdPath = sys32 + "wipe_launch.cmd";

        switch (strategy)
        {
        case WinREPatcher::WipeStrategy::ZeroFill:
            {
                std::string dpScript = "select disk " + std::to_string(diskNumber) + "\r\nclean all\r\nexit\r\n";
                if (!WriteFile(sys32 + "dp_zerofill.txt", dpScript)) return false;

                std::vector<WipeStep> steps = {
                    { "Running diskpart clean all...\r\necho  This may take 30-60 minutes for large drives.",
                      "diskpart /s " + std::string(kWinRESys32) + "dp_zerofill.txt\r\n" }
                };

                return BuildAndWriteLaunchCmd(launchCmdPath, diskNumber, 
                                              "HDD Zero Fill", 
                                              "ALL DATA will be permanently erased!", 
                                              steps);
            }

        case WinREPatcher::WipeStrategy::RandomFill:
            {
                if (!WriteFile(sys32 + "dp_randomfill_prep.txt", GetPrepScript(diskNumber))) return false;
                if (!WriteFile(sys32 + "dp_randomfill_cleanup.txt", GetCleanupScript(diskNumber))) return false;

                std::vector<WipeStep> steps = {
                    { "Preparing Disk (removing partitions ^& assigning Z:)...",
                      "diskpart /s " + std::string(kWinRESys32) + "dp_randomfill_prep.txt\r\n" },
                    { "Formatting with random data (/P:1)...\r\necho  This may take SEVERAL HOURS for large drives.",
                      "format Z: /FS:NTFS /P:1 /Y\r\n" },
                    { "Final cleanup (returning disk to RAW state)...",
                      "diskpart /s " + std::string(kWinRESys32) + "dp_randomfill_cleanup.txt\r\n" }
                };

                return BuildAndWriteLaunchCmd(launchCmdPath, diskNumber, 
                                              "HDD Random Fill", 
                                              "ALL DATA will be overwritten with random bits!", 
                                              steps);
            }

        case WinREPatcher::WipeStrategy::QuickFormatAndTrim:
            {
                if (!WriteFile(sys32 + "dp_ssd_prep.txt", GetPrepScript(diskNumber))) return false;
                if (!WriteFile(sys32 + "dp_ssd_cleanup.txt", GetCleanupScript(diskNumber))) return false;

                std::vector<WipeStep> steps = {
                    { "Preparing SSD (removing partitions ^& assigning Z:)...",
                      "diskpart /s " + std::string(kWinRESys32) + "dp_ssd_prep.txt\r\n" },
                    { "Quick format + Explicit TRIM (wipes NAND cells)...",
                      "format Z: /FS:NTFS /Q /Y\r\n"
                      "defrag Z: /L\r\n" },
                    { "Final cleanup (returning disk to RAW state)...",
                      "diskpart /s " + std::string(kWinRESys32) + "dp_ssd_cleanup.txt\r\n" }
                };

                return BuildAndWriteLaunchCmd(launchCmdPath, diskNumber, 
                                              "SSD", 
                                              "ALL DATA will be permanently erased via TRIM!", 
                                              steps);
            }
        }

        return true;
    }

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
        std::cout << "  " << CLIUtils::Colors::YELLOW << "--- winpeshl.ini (before) ---" << CLIUtils::Colors::RESET << "\n"
                  << (original.empty() ? "  (empty)\n" : original) << "\n";

        std::ofstream out(iniPath, std::ios::trunc);
        if (!out.is_open())
        {
            std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                 << " Cannot open winpeshl.ini for writing.\n";
            return false;
        }

        out << "[LaunchApps]\r\n";
        out << "X:\\Windows\\System32\\cmd.exe, /c X:\\Windows\\System32\\wipe_launch.cmd\r\n";

        std::cout << "  " << CLIUtils::Colors::YELLOW << "--- winpeshl.ini (after) ---" << CLIUtils::Colors::RESET << "\n"
                  << "  [LaunchApps]\n"
                  << "  X:\\Windows\\System32\\cmd.exe, "
                     "/c X:\\Windows\\System32\\wipe_launch.cmd\n\n";
        return true;
    }
}
