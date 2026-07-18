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

using namespace std;

// Target physical disk number (diskpart: 'list disk')
constexpr int kWipeDiskNumber = 0;

// Configurable wipe strategies for each disk type
constexpr WinREPatcher::WipeStrategy kHddStrategy = WinREPatcher::WipeStrategy::ZeroFill;
constexpr WinREPatcher::WipeStrategy kSsdStrategy = WinREPatcher::WipeStrategy::QuickFormatAndTrim;

int main()
{
    CLIUtils::PrintBanner(kWipeDiskNumber, kHddStrategy, kSsdStrategy);

    // ---- 1. Locate WinRE ----
    cout << "  [1/5] Locating WinRE...\n\n";

    string winrePath;
    string winreStatus;

    if (!WinREFinder::FindLocation(winrePath, winreStatus))
    {
        cerr << "  [ERROR] WinRE location could not be determined.\n"
                     "          WinRE may not be installed or enabled,\n"
                     "          or the program may not be running as Administrator.\n\n";
        cout << "  Press any key to exit...\n";
        cin.get();
        return EXIT_FAILURE;
    }

    WinREFinder::PrintInfo(winrePath, winreStatus);

    
    // ---- 2. Detect disk type ----
    cout << "  [2/5] Detecting disk type for Disk " << kWipeDiskNumber << "...\n\n";

    DiskDetector::DiskType diskType = DiskDetector::Detect(kWipeDiskNumber);

    const bool isSSD =
        (diskType == DiskDetector::DiskType::SSD_SATA ||
         diskType == DiskDetector::DiskType::SSD_NVME);

    cout << "  Disk " << kWipeDiskNumber << " : "
              << DiskDetector::ToString(diskType) << "\n";

    WinREPatcher::WipeStrategy activeStrategy = (isSSD) ? kSsdStrategy : kHddStrategy;

    cout << "  Selected    : "
              << CLIUtils::StrategyToString(activeStrategy)
              << "\n\n";


    // ---- 3. Patch winre.wim ----
    cout << "  [3/5] Patching WinRE image...\n\n";

    if (!WinREPatcher::Patch(winrePath, kWipeDiskNumber, activeStrategy))
    {
        cerr << "  [ERROR] Failed to patch WinRE image. Aborting.\n\n";
        cout << "  Press any key to exit...\n";
        cin.get();
        return EXIT_FAILURE;
    }


    // ---- 4. Countdown ----
    cout << "  [4/5] Starting countdown...\n\n";

    if (!Countdown::Run(10))
    {
        cout << "  Operation cancelled. Restoring original winre.wim...\n\n";

        // Resolve the exact .wim path so Rollback() can find the .bak
        const string wimPath = WIMResolver::ResolveWimPath(winrePath);
        if (!wimPath.empty())
        {
            WinREPatcher::Rollback(wimPath);
        }
        else
        {
            cerr << "  [WARN] Could not resolve winre.wim path for rollback.\n"
                      "         Backup file (.bak) may remain alongside winre.wim.\n";
        }

        cout << "  System will not be restarted.\n\n"
             << "  Press any key to exit...\n";
        cin.get();
        return EXIT_SUCCESS;
    }


    // ---- 5. Reboot into WinRE ----
    cout << "  [5/5] Initiating WinRE reboot...\n\n";

    if (!Reboot::BootIntoWinRE())
    {
        cerr << "\n  [ERROR] Reboot could not be initiated.\n\n";
        cout << "  Press any key to exit...\n";
        cin.get();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
