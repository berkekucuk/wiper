#include "cli_utils.h"
#include "config.h"
#include "disk_wipe_runner.h"
#include "file_wipe_runner.h"

#include <cstdlib>
#include <iostream>

int main()
{
    CLIUtils::EnableColors();

    Config::WipeConfig cfg;
    if (!Config::LoadConfig(cfg))
    {
        std::cerr << "  Press any key to exit...\n";
        std::cin.get();
        return EXIT_FAILURE;
    }

    if (cfg.mode == "diskwipe")
    {
        CLIUtils::PrintBanner("diskwipe");
        DiskWiper::DiskWipeStrategy hddStrategy = DiskWiper::StrategyFromString(cfg.hddDiskwipeStrategy);
        DiskWiper::DiskWipeStrategy ssdStrategy = DiskWiper::StrategyFromString(cfg.ssdDiskwipeStrategy);
        return DiskWiper::RunDiskWipe(cfg.diskNumber, hddStrategy, ssdStrategy);
    }
    else
    {
        CLIUtils::PrintBanner("filewipe");
        FileWiper::FileWipeStrategy hddStrategy = FileWiper::StrategyFromString(cfg.hddFileStrategy);
        FileWiper::FileWipeStrategy ssdStrategy = FileWiper::StrategyFromString(cfg.ssdFileStrategy);
        return FileWiper::RunFileWipe(cfg.wipeList, hddStrategy, ssdStrategy);
    }
}