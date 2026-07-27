#include "winre_script_generator.h"
#include "cli_utils.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    bool WriteFile(const std::string& path, const std::string& content)
    {
        std::ofstream s(path, std::ios::trunc);
        if (!s.is_open()) return false;
        s << content;
        return true;
    }

    std::string GetWipeScript(int diskNumber, DiskWiper::DiskWipeStrategy strategy)
    {
        std::string title;
        std::string warning;
        std::string commands;

        switch (strategy)
        {
        case DiskWiper::DiskWipeStrategy::ZeroFill:
            title = "HDD Zero Fill";
            warning = "ALL DATA will be permanently erased!";
            commands =
                "echo  [1/1] Running diskpart clean all...\r\n"
                "echo  This may take 30-60 minutes for large drives.\r\n"
                "(echo select disk " + std::to_string(diskNumber) + "& echo clean all& echo exit) | diskpart > nul\r\n"
                "echo.\r\n";
            break;

        case DiskWiper::DiskWipeStrategy::RandomFill:
            title = "HDD Random Fill";
            warning = "ALL DATA will be overwritten with random bits!";
            commands =
                "echo  [1/3] Preparing Disk (removing partitions ^& assigning Z:)...\r\n"
                "(echo select disk " + std::to_string(diskNumber) + "& echo clean& echo create partition primary& echo assign letter=Z& echo exit) | diskpart > nul\r\n"
                "echo.\r\n"
                "echo  [2/3] Formatting with random data (/P:1)...\r\n"
                "echo  This may take SEVERAL HOURS for large drives.\r\n"
                "format Z: /FS:NTFS /P:1 /Y\r\n"
                "echo.\r\n"
                "echo  [3/3] Final cleanup (returning disk to RAW state)...\r\n"
                "(echo select disk " + std::to_string(diskNumber) + "& echo clean& echo exit) | diskpart > nul\r\n"
                "echo.\r\n";
            break;

        case DiskWiper::DiskWipeStrategy::QuickFormat:
            title = "SSD";
            warning = "Partition metadata will be removed!";
            commands =
                "echo  [1/3] Preparing SSD (removing partitions ^& assigning Z:)...\r\n"
                "(echo select disk " + std::to_string(diskNumber) + "& echo clean& echo create partition primary& echo assign letter=Z& echo exit) | diskpart > nul\r\n"
                "echo.\r\n"
                "echo  [2/3] Quick formatting...\r\n"
                "format Z: /FS:NTFS /Q /Y\r\n"
                "echo.\r\n"
                "echo  [3/3] Final cleanup (returning disk to RAW state)...\r\n"
                "(echo select disk " + std::to_string(diskNumber) + "& echo clean& echo exit) | diskpart > nul\r\n"
                "echo.\r\n";
            break;
        }

        std::ostringstream s;
        s << "@echo off\r\n"
          << "echo.\r\n"
          << "echo  ====================================================\r\n"
          << "echo   WinRE Full Disk Wipe (" << title << ") ^| Disk " << diskNumber << "\r\n"
           << "echo   " << warning << "\r\n"
           << "echo  ====================================================\r\n"
           << "echo.\r\n"
           << commands
           << "echo  Wipe completed.\r\n";

        return s.str();
    }
} 

// ---------------------------------------------------------------------------
// WinREScriptGenerator interface implementation
// ---------------------------------------------------------------------------
namespace WinREScriptGenerator
{
    bool GenerateDiskWipeScripts(const std::string& mountDir, int diskNumber, DiskWiper::DiskWipeStrategy strategy)
    {
        // Validate disk number range before writing any scripts.
        // diskpart accepts 0-127; anything outside is invalid.
        if (diskNumber < 0 || diskNumber > 127)
        {
            std::cerr << "  [ERROR] Invalid disk number: " << diskNumber
                      << ". Must be in range 0-127.\n";
            return false;
        }

        const std::string launchCmdPath = mountDir + "\\Windows\\System32\\wipe_launch.cmd";
        return WriteFile(launchCmdPath, GetWipeScript(diskNumber, strategy));
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
        out.close();

        if (!out)
        {
            std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                      << " Failed to write winpeshl.ini.\n";
            return false;
        }

        std::string updated;
        {
            std::ifstream in(iniPath);
            if (in.is_open())
            {
                std::ostringstream oss;
                oss << in.rdbuf();
                updated = oss.str();
            }
        }

        std::cout << "  " << CLIUtils::Colors::YELLOW << "--- winpeshl.ini (after) ---" << CLIUtils::Colors::RESET << "\n"
                  << (updated.empty() ? "  (empty)\n" : updated) << "\n";
        return true;
    }
}
