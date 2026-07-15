#include "cli_utils.h"

#include <iostream>

using namespace std;

namespace CLIUtils
{
    const char* StrategyToString(WinREPatcher::WipeStrategy s)
    {
        if (s == WinREPatcher::WipeStrategy::ZeroFill) return "Zero Fill (clean all)";
        if (s == WinREPatcher::WipeStrategy::RandomFill) return "Random Fill (format /P:1)";
        return "Quick Format + TRIM";
    }

    void PrintBanner(int wipeDiskNumber, WinREPatcher::WipeStrategy hddStrategy, WinREPatcher::WipeStrategy ssdStrategy)
    {
        cout << "\n";
        cout << "  ===============\n";
        cout << "       Wiper     \n";
        cout << "  ===============\n";

        cout << "  Target disk : Disk " << wipeDiskNumber << "\n";
        cout << "  HDD Strategy: " << StrategyToString(hddStrategy) << "\n";
        cout << "  SSD Strategy: " << StrategyToString(ssdStrategy) << "\n";

        cout << "  WARNING     : ALL DATA on the disk will be erased!\n";
        cout << "  ====================================================\n";
        cout << "\n";
    }
}
