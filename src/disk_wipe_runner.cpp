#include "disk_wipe_runner.h"
#include "cli_utils.h"
#include "countdown.h"
#include "disk_detector.h"
#include "reboot.h"
#include "winre_finder.h"
#include "winre_patcher.h"
#include "wim_resolver.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace DiskWiper
{
    DiskWipeStrategy StrategyFromString(const std::string& s)
    {
        if (s == "zerofill") return DiskWipeStrategy::ZeroFill;
        if (s == "randomfill") return DiskWipeStrategy::RandomFill;
        if (s == "quickformat") return DiskWipeStrategy::QuickFormat;
        return DiskWipeStrategy::ZeroFill;
    }

    int RunDiskWipe(int diskNumber, DiskWipeStrategy hddStrategy, DiskWipeStrategy ssdStrategy)
    {
        CLIUtils::PrintBanner(diskNumber, hddStrategy, ssdStrategy);

        WinREFinder::EnsureEnabled();

        std::cout << CLIUtils::Colors::BCYAN << "  [1/5] " << CLIUtils::Colors::RESET << "Locating WinRE...\n\n";

        std::string winrePath;
        std::string winreStatus;

        if (!WinREFinder::FindLocation(winrePath, winreStatus))
        {
            std::cerr << CLIUtils::Colors::BRED << "  [ERROR] " << CLIUtils::Colors::RESET
                << "WinRE location could not be determined.\n"
                << "          WinRE may not be installed or enabled,\n"
                << "          or the program may not be running as Administrator.\n\n";
            std::cout << "  Press any key to exit...\n";
            std::cin.get();
            return EXIT_FAILURE;
        }

        WinREFinder::PrintInfo(winrePath, winreStatus);

        std::cout << CLIUtils::Colors::BCYAN << "  [2/5] " << CLIUtils::Colors::RESET
            << "Detecting disk type for Disk " << diskNumber << "...\n\n";

        DiskDetector::DiskType diskType = DiskDetector::Detect(diskNumber);

        const bool isSSD =
            (diskType == DiskDetector::DiskType::SSD_SATA ||
            diskType == DiskDetector::DiskType::SSD_NVME);

        std::cout << "  Disk type    : " << DiskDetector::ToString(diskType) << "\n";

        DiskWiper::DiskWipeStrategy activeStrategy = (isSSD) ? ssdStrategy : hddStrategy;
        std::cout << "  Strategy     : " << CLIUtils::StrategyToString(activeStrategy) << "\n\n";


        std::cout << CLIUtils::Colors::BCYAN << "  [3/5] " << CLIUtils::Colors::RESET << "Patching WinRE image...\n\n";

        if (!WinREPatcher::Patch(winrePath, diskNumber, activeStrategy))
        {
            std::cerr << CLIUtils::Colors::BRED << "  [ERROR] " << CLIUtils::Colors::RESET
                << "Failed to patch WinRE image. Aborting.\n\n";
            std::cout << "  Press any key to exit...\n";
            std::cin.get();
            return EXIT_FAILURE;
        }

        std::cout << CLIUtils::Colors::BCYAN << "  [4/5] " << CLIUtils::Colors::RESET << "Starting countdown...\n\n";

        if (!Countdown::Run(10))
        {
            std::cout << "  Operation cancelled. Restoring original winre.wim...\n\n";

            const std::string wimPath = WIMResolver::ResolveWimPath(winrePath);
            const bool restored = !wimPath.empty() && WinREPatcher::Rollback(wimPath);
            if (!restored)
            {
                std::cerr << CLIUtils::Colors::YELLOW << "  [WARN] " << CLIUtils::Colors::RESET
                        << "Could not restore winre.wim. Backup file may remain in %TEMP%.\n";
            }

            std::cout << "  System will not be restarted.\n\n"
                << "  Press any key to exit...\n";
            std::cin.get();
            return restored ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        std::cout << CLIUtils::Colors::BCYAN << "  [5/5] " << CLIUtils::Colors::RESET << "Initiating WinRE reboot...\n\n";

        if (!Reboot::BootIntoWinRE())
        {
            std::cerr << "\n" << CLIUtils::Colors::BRED << "  [ERROR] " << CLIUtils::Colors::RESET
                << "Reboot could not be initiated. Restoring original winre.wim...\n\n";

            const std::string wimPath = WIMResolver::ResolveWimPath(winrePath);
            if (wimPath.empty() || !WinREPatcher::Rollback(wimPath))
            {
                std::cerr << CLIUtils::Colors::BRED << "  [ERROR] " << CLIUtils::Colors::RESET
                        << "Rollback failed. Do not restart until WinRE has been restored.\n\n";
            }

            std::cout << "  Press any key to exit...\n";
            std::cin.get();
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

}
