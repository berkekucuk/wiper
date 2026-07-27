#include "cli_utils.h"
#include "config.h"
#include "disk_wipe_runner.h"
#include "file_wipe_runner.h"

#include <cstdlib>
#include <iostream>
#include <locale>

int main()
{
    CLIUtils::EnableColors();

    std::locale::global(std::locale(""));

    Config::WipeConfig cfg;
    if (!Config::LoadConfig(cfg))
    {
        std::cerr << "  Press any key to exit...\n";
        std::cin.get();
        return EXIT_FAILURE;
    }

    if (cfg.mode == "diskwipe")
    {
        DiskWiper::DiskWipeStrategy hddStrategy = DiskWiper::StrategyFromString(cfg.hddStrategy);
        DiskWiper::DiskWipeStrategy ssdStrategy = DiskWiper::StrategyFromString(cfg.ssdStrategy);
        return DiskWiper::RunDiskWipe(cfg.diskNumber, hddStrategy, ssdStrategy);
    }
    else
    {
        FileWiper::FileWipeStrategy fileStrategy = FileWiper::StrategyFromString(cfg.fileStrategy);
        return FileWiper::RunFileWipe(cfg.wipeList, fileStrategy);
    }
}