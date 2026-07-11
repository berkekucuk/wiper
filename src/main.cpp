#include "countdown.h"
#include "disk_detector.h"
#include "reboot.h"
#include "winre_finder.h"
#include "winre_patcher.h"

#include <cstdlib>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// Wipe mode:
//   WipeMode::PartitionWipe  →  silences a single partition (default, safe)
//   WipeMode::FullDiskWipe   →  zeros the ENTIRE physical disk (irreversible,
//                                system will shut down instead of rebooting)
constexpr WinREPatcher::WipeMode kWipeMode = WinREPatcher::WipeMode::PartitionWipe;

// Target physical disk number (diskpart: 'list disk')
constexpr int kWipeDiskNumber = 0;

// Target partition number — used only in PartitionWipe mode
// (diskpart: 'select disk N' then 'list partition')
constexpr int kWipePartitionNumber = 5;  // Partition 5 → D: drive (~200 GB)

// ---------------------------------------------------------------------------
// Helper: banner
// ---------------------------------------------------------------------------
static void PrintBanner()
{
    std::cout << "\n";
    std::cout << "  ===============\n";
    std::cout << "       Wiper     \n";
    std::cout << "  ===============\n";

    if (kWipeMode == WinREPatcher::WipeMode::FullDiskWipe)
    {
        std::cout << "  Mode        : FULL DISK WIPE (clean all)\n";
        std::cout << "  Target disk : Disk " << kWipeDiskNumber << "\n";
        std::cout << "  WARNING     : ALL DATA on the disk will be erased!\n";
    }
    else
    {
        std::cout << "  Mode        : Partition wipe\n";
        std::cout << "  Target      : Disk " << kWipeDiskNumber
                  << "  /  Partition " << kWipePartitionNumber << "\n";
    }

    std::cout << "  ====================================================\n";
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

    // ---- 2. Detect disk type (PartitionWipe only) ----
    DiskDetector::DiskType diskType = DiskDetector::DiskType::Unknown;

    if (kWipeMode == WinREPatcher::WipeMode::PartitionWipe)
    {
        std::cout << "  [2/5] Detecting disk type for Disk " << kWipeDiskNumber << "...\n\n";

        diskType = DiskDetector::Detect(kWipeDiskNumber);

        const bool isSSD =
            (diskType == DiskDetector::DiskType::SSD_SATA ||
             diskType == DiskDetector::DiskType::SSD_NVME);

        std::cout << "  Disk " << kWipeDiskNumber << " : "
                  << DiskDetector::ToString(diskType) << "\n";
        std::cout << "  Strategy    : "
                  << (isSSD ? "quick format + TRIM (SSD)"
                            : "format /P:1 random overwrite (HDD)")
                  << "\n\n";
    }
    else
    {
        // FullDiskWipe: disk type irrelevant, clean all zeros everything
        std::cout << "  [2/5] Disk type detection skipped (FullDiskWipe uses 'clean all').\n\n";
    }

    // ---- 3. Patch winre.wim ----
    std::cout << "  [3/5] Patching WinRE image...\n\n";

    if (!WinREPatcher::Patch(winrePath,
                             kWipeDiskNumber,
                             kWipePartitionNumber,
                             diskType,
                             kWipeMode))
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
