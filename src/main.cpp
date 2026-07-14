#include "countdown.h"
#include "disk_detector.h"
#include "reboot.h"
#include "winre_finder.h"
#include "winre_patcher.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace std;

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
constexpr int kWipePartitionNumber = 2; 

// ---------------------------------------------------------------------------
// Helper: banner
// ---------------------------------------------------------------------------
static void PrintBanner()
{
    cout << "\n";
    cout << "  ===============\n";
    cout << "       Wiper     \n";
    cout << "  ===============\n";

    if (kWipeMode == WinREPatcher::WipeMode::FullDiskWipe)
    {
        cout << "  Mode        : FULL DISK WIPE\n";
        cout << "  Target disk : Disk " << kWipeDiskNumber << "\n";
        cout << "  WARNING     : ALL DATA on the disk will be erased!\n";
    }
    else
    {
        cout << "  Mode        : Partition wipe\n";
        cout << "  Target      : Disk " << kWipeDiskNumber
                  << "  /  Partition " << kWipePartitionNumber << "\n";
    }

    cout << "  ====================================================\n";
    cout << "\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    PrintBanner();

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

    if (kWipeMode == WinREPatcher::WipeMode::PartitionWipe)
    {
        cout << "  Strategy    : "
                  << (isSSD ? "quick format + explicit TRIM (SSD)" 
                            : "format /P:1 random overwrite (HDD)")
                  << "\n\n";
    }
    else
    {
        cout << "  Strategy    : "
                  << (isSSD ? "clean + explicit TRIM (SSD)"       
                            : "diskpart clean all (HDD)")        
                  << "\n\n";
    }

    // ---- 3. Patch winre.wim ----
    cout << "  [3/5] Patching WinRE image...\n\n";

    if (!WinREPatcher::Patch(winrePath,
                             kWipeDiskNumber,
                             kWipePartitionNumber,
                             diskType,
                             kWipeMode))
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
        cout << "  Operation cancelled. System will not be restarted.\n\n";
        cout << "  Press any key to exit...\n";
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
