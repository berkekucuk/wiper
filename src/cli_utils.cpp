#include "cli_utils.h"

#include <windows.h>
#include <iostream>

namespace CLIUtils
{
    void EnableColors()
    {
        // Enable ANSI/VT100 processing on both stdout and stderr.
        // Required on Windows 10+ cmd.exe / conhost; no-op on Windows Terminal.
        for (DWORD handle : {STD_OUTPUT_HANDLE, STD_ERROR_HANDLE})
        {
            HANDLE h = GetStdHandle(handle);
            DWORD  mode = 0;
            if (GetConsoleMode(h, &mode))
                SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    const char* StrategyToString(const std::string& s)
    {
        if (s == "zerofill")    return "Zero Fill (1 pass)";
        if (s == "randomfill")  return "Random Fill (1 pass)";
        if (s == "dod3")        return "DOD 5220.22-M (3 passes)";
        if (s == "trim")        return "Trim (no overwrite)";
        if (s == "quickformat") return "Quick Format";
        return s.c_str();
    }

    void PrintBanner(const std::string& mode)
    {
        bool diskWipe = (mode == "diskwipe");
        std::cout << "\n";
        std::cout << CLIUtils::Colors::BCYAN << "  ====================================================\n" << CLIUtils::Colors::RESET;
        if (diskWipe)
            std::cout << CLIUtils::Colors::BCYAN << "                   Wiper  -  Disk Wipe\n"                           << CLIUtils::Colors::RESET;
        else
            std::cout << CLIUtils::Colors::BCYAN << "                   Wiper  -  File Wipe\n"               << CLIUtils::Colors::RESET;
        std::cout << CLIUtils::Colors::BCYAN << "  ====================================================\n" << CLIUtils::Colors::RESET;
        std::cout << "\n";
        if (diskWipe)
            std::cout << CLIUtils::Colors::BRED << "  WARNING : ALL DATA on the target disk will be erased!\n" << CLIUtils::Colors::RESET;
        else
            std::cout << CLIUtils::Colors::BRED << "  WARNING : Specified files will be permanently erased!\n" << CLIUtils::Colors::RESET;
        std::cout << "\n";
    }
}
