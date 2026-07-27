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

    const char* StrategyToString(DiskWiper::DiskWipeStrategy s)
    {
        if (s == DiskWiper::DiskWipeStrategy::ZeroFill)          return "Zero Fill (clean all)";
        if (s == DiskWiper::DiskWipeStrategy::RandomFill)        return "Random Fill (format /P:1)";
        return "Quick Format";
    }

    void PrintBanner(int wipeDiskNumber,
                     DiskWiper::DiskWipeStrategy hddStrategy,
                     DiskWiper::DiskWipeStrategy ssdStrategy)
    {
        std::cout << "\n";
        std::cout << CLIUtils::Colors::BCYAN << "  ====================================================\n" << CLIUtils::Colors::RESET;
        std::cout << CLIUtils::Colors::BCYAN << "                        Wiper\n"                           << CLIUtils::Colors::RESET;
        std::cout << CLIUtils::Colors::BCYAN << "  ====================================================\n" << CLIUtils::Colors::RESET;
        std::cout << "\n";
        std::cout << "  Target disk  : Disk " << wipeDiskNumber << "\n";
        std::cout << "  HDD strategy : " << StrategyToString(hddStrategy) << "\n";
        std::cout << "  SSD strategy : " << StrategyToString(ssdStrategy) << "\n";
        std::cout << "\n";
        std::cout << CLIUtils::Colors::BRED << "  WARNING : ALL DATA on the target disk will be erased!\n" << CLIUtils::Colors::RESET;
        std::cout << "\n";
    }

    const char* FileWipeStrategyToString(FileWiper::FileWipeStrategy s)
    {
        if (s == FileWiper::FileWipeStrategy::ZeroFill)   return "Zero Fill (1 pass)";
        if (s == FileWiper::FileWipeStrategy::RandomFill) return "Random Fill (1 pass)";
        return "DOD 5220.22-M (3 passes)";
    }

    void PrintFileWipeBanner(FileWiper::FileWipeStrategy strategy)
    {
        std::cout << "\n";
        std::cout << CLIUtils::Colors::BCYAN << "  ====================================================\n" << CLIUtils::Colors::RESET;
        std::cout << CLIUtils::Colors::BCYAN << "                   Wiper  -  File Wipe\n"               << CLIUtils::Colors::RESET;
        std::cout << CLIUtils::Colors::BCYAN << "  ====================================================\n" << CLIUtils::Colors::RESET;
        std::cout << "\n";
        std::cout << "  Strategy : " << FileWipeStrategyToString(strategy) << "\n";
        std::cout << "\n";
        std::cout << CLIUtils::Colors::BRED << "  WARNING : Specified files will be permanently erased!\n" << CLIUtils::Colors::RESET;
        std::cout << "\n";
    }
}
