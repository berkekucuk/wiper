#pragma once

#include "winre_patcher.h"

namespace CLIUtils
{
    const char* StrategyToString(WinREPatcher::WipeStrategy s);
    void PrintBanner(int wipeDiskNumber, WinREPatcher::WipeStrategy hddStrategy, WinREPatcher::WipeStrategy ssdStrategy);
}
