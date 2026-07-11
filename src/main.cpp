#include "countdown.h"
#include "disk_detector.h"
#include "reboot.h"
#include "winre_finder.h"
#include "winre_patcher.h"

#include <cstdlib>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// Wipe target configuration
// Use Windows diskpart 'list disk' + 'list partition' to identify these.
// WARNING: The specified partition will be wiped (all data erased).
// ---------------------------------------------------------------------------
constexpr int kWipeDiskNumber      = 0;  // Disk 0  (single physical disk)
constexpr int kWipePartitionNumber = 5;  // Partition 5  

// ---------------------------------------------------------------------------
// Helper: banner
// ---------------------------------------------------------------------------
static void PrintBanner()
{
    std::cout << "\n";
    std::cout << "  ============================\n";
    std::cout << "         WinRE Boot Launcher  \n";
    std::cout << "  ============================\n";
    std::cout << "  Wipe target : Disk " << kWipeDiskNumber
              << "  /  Partition "       << kWipePartitionNumber << "\n";
    std::cout << "  ============================\n";
    std::cout << "\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    PrintBanner();

    // ---- 1. Locate WinRE ----
    std::cout << "  [1/5] Locating WinRE...\n\n";

    std::string winrePath;
    std::string winreStatus;

    if (!WinREFinder::FindLocation(winrePath, winreStatus))
    {
        std::cerr << "  [ERROR] WinRE location could not be determined.\n"
                     "          WinRE may not be installed or enabled,\n"
                     "          or the program may not be running as Administrator.\n\n";
        std::cout << "  Press any key to exit...\n";
        std::cin.get();
        return EXIT_FAILURE;
    }

    WinREFinder::PrintInfo(winrePath, winreStatus);

    // ---- 2. Detect disk type ----
    std::cout << "  [2/5] Detecting disk type for Disk " << kWipeDiskNumber << "...\n\n";

    const DiskDetector::DiskType diskType = DiskDetector::Detect(kWipeDiskNumber);

    std::cout << "  Disk " << kWipeDiskNumber << " : "
              << DiskDetector::ToString(diskType) << "\n";

    // Print the wipe strategy that will be used
    const bool isSSD =
        (diskType == DiskDetector::DiskType::SSD_SATA ||
         diskType == DiskDetector::DiskType::SSD_NVME);

    std::cout << "  Strategy    : "
              << (isSSD ? "quick format + TRIM (SSD)"
                        : "format /P:1 random overwrite (HDD)")
              << "\n\n";

    // ---- 3. Patch winre.wim ----
    std::cout << "  [3/5] Patching WinRE image "
                 "(Disk " << kWipeDiskNumber
              << "  /  Partition " << kWipePartitionNumber << ")...\n\n";

    if (!WinREPatcher::Patch(winrePath,
                             kWipeDiskNumber,
                             kWipePartitionNumber,
                             diskType))
    {
        std::cerr << "  [ERROR] Failed to patch WinRE image. Aborting.\n\n";
        std::cout << "  Press any key to exit...\n";
        std::cin.get();
        return EXIT_FAILURE;
    }

    // ---- 4. Countdown ----
    std::cout << "  [4/5] Starting countdown...\n\n";

    if (!Countdown::Run(10))
    {
        std::cout << "  Operation cancelled. System will not be restarted.\n\n";
        std::cout << "  Press any key to exit...\n";
        std::cin.get();
        return EXIT_SUCCESS;
    }

    // ---- 5. Reboot into WinRE ----
    std::cout << "  [5/5] Initiating WinRE reboot...\n\n";

    if (!Reboot::BootIntoWinRE())
    {
        std::cerr << "\n  [ERROR] Reboot could not be initiated.\n\n";
        std::cout << "  Press any key to exit...\n";
        std::cin.get();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
