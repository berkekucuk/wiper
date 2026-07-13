#include "winre_patcher.h"
#include "disk_detector.h"
#include "dism_utils.h"
#include "wim_resolver.h"
#include "winre_script_generator.h"

#include <windows.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

// ---------------------------------------------------------------------------
// WinREPatcher interface implementation
// ---------------------------------------------------------------------------
namespace WinREPatcher
{
    bool Patch(const string&     winreDir,
               int                    diskNumber,
               int                    partitionNumber,
               DiskDetector::DiskType diskType,
               WipeMode               mode)
    {
        const bool isSSD =
            (diskType == DiskDetector::DiskType::SSD_SATA ||
             diskType == DiskDetector::DiskType::SSD_NVME);
        const bool isFullDisk = (mode == WipeMode::FullDiskWipe);

        // ── Resolve winre.wim ─────────────────────────────────────────────────────
        cout << "  Resolving winre.wim from: " << winreDir << "\n";
        const string wimPath = WIMResolver::ResolveWimPath(winreDir);

        if (wimPath.empty())
        {
            cerr << "  [ERROR] winre.wim not found.\n";
            return false;
        }

        const string mountDir = DISMUtils::BuildMountDir();

        cout << "  WinRE image : " << wimPath  << "\n";
        cout << "  Mount dir   : " << mountDir << "\n";
        if (isFullDisk)
        {
            cout << "  Wipe target : Disk " << diskNumber << " (FULL DISK)\n";
            cout << "  Disk type   : " << DiskDetector::ToString(diskType) << "\n";
            cout << "  Mode        : FullDiskWipe — "
                      << (isSSD ? "clean + TRIM" : "diskpart clean all") << "\n\n";
        }
        else
        {
            cout << "  Wipe target : Disk " << diskNumber
                      << "  /  Partition " << partitionNumber << "\n";
            cout << "  Disk type   : " << DiskDetector::ToString(diskType) << "\n";
            cout << "  Mode        : PartitionWipe — "
                      << (isSSD ? "quick format + TRIM" : "format /P:1") << "\n\n";
        }

        // ── Step 1: Create mount directory ────────────────────────────────────
        cout << "  [1/4] Creating mount directory...\n";
        if (!CreateDirectoryA(mountDir.c_str(), nullptr))
        {
            cerr << "  [ERROR] CreateDirectory failed "
                      << "(code " << GetLastError() << ")\n";
            return false;
        }

        // ── Step 2: Mount via DISM ──────────────────────────────────────────
        cout << "  [2/4] Mounting WinRE image via DISM "
                     "(may take up to 60 s)...\n\n";

        if (!DISMUtils::MountImage(wimPath, mountDir))
        {
            return false;
        }
        cout << "\n  Image mounted.\n\n";

        // ── Step 3: Embed scripts ─────────────────────────────────────────────────
        if (isFullDisk)
        {
            cout << "  [3/4] Embedding full-disk wipe scripts "
                      << (isSSD ? "[SSD strategy]" : "[HDD strategy]") << "...\n\n";

            if (!WinREScriptGenerator::GenerateFullDiskWipeScripts(mountDir, diskNumber, diskType))
            {
                cerr << "  [ERROR] Failed to write scripts. Discarding mount.\n";
                DISMUtils::DiscardMount(mountDir);
                return false;
            }
        }
        else
        {
            // PartitionWipe — drive-letter-based format strategy
            cout << "  [3/4] Embedding partition wipe scripts "
                      << (isSSD ? "[SSD strategy]" : "[HDD strategy]") << "...\n\n";

            if (!WinREScriptGenerator::GeneratePartitionWipeScripts(mountDir, diskNumber, partitionNumber, diskType))
            {
                cerr << "  [ERROR] Failed to write scripts. Discarding mount.\n";
                DISMUtils::DiscardMount(mountDir);
                return false;
            }
        }

        // ── Step 4: Patch winpeshl.ini ─────────────────────────────────────────
        cout << "  [4/4] Patching winpeshl.ini...\n";
        if (!WinREScriptGenerator::PatchWinpeshl(mountDir))
        {
            cerr << "  [ERROR] winpeshl.ini patch failed. Discarding mount.\n";
            DISMUtils::DiscardMount(mountDir);
            return false;
        }

        // ── Commit and unmount ───────────────────────────────────────────────────
        cout << "  Committing and unmounting (may take up to 60 s)...\n\n";

        if (!DISMUtils::CommitAndUnmount(mountDir))
        {
            return false;
        }

        cout << "\n  winre.wim patched and committed.\n\n";
        return true;
    }
} // namespace WinREPatcher
