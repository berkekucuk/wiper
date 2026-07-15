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
    bool Patch(const string& winreDir, int diskNumber, WipeStrategy strategy)
    {
        // ── Step 1: Resolve winre.wim ─────────────────────────────────────────────
        cout << "  [1/6] Resolving winre.wim from: " << winreDir << "\n";
        const string wimPath = WIMResolver::ResolveWimPath(winreDir);

        if (wimPath.empty())
        {
            cerr << "  [ERROR] winre.wim not found.\n";
            return false;
        }
        cout << "  WinRE image : " << wimPath  << "\n";
        cout << "  Wipe target : Disk " << diskNumber << " (FULL DISK)\n";

        cout << "  Strategy    : ";
        switch (strategy)
        {
            case WipeStrategy::ZeroFill:           cout << "Zero Fill (diskpart clean all)"; break;
            case WipeStrategy::QuickFormatAndTrim: cout << "Quick Format + explicit TRIM"; break;
        }
        cout << "\n\n";

        // ── Step 2: Create mount directory ────────────────────────────────────
        cout << "  [2/6] Creating mount directory...\n";
        const string mountDir = DISMUtils::BuildMountDir();
        cout << "  Mount dir   : " << mountDir << "\n";

        if (!CreateDirectoryA(mountDir.c_str(), nullptr))
        {
            cerr << "  [ERROR] CreateDirectory failed "
                      << "(code " << GetLastError() << ")\n";
            return false;
        }

        // ── Step 3: Mount via DISM ──────────────────────────────────────────
        cout << "  [3/6] Mounting WinRE image via DISM (may take up to 60 s)...\n\n";

        if (!DISMUtils::MountImage(wimPath, mountDir))
        {
            return false;
        }
        cout << "\n  Image mounted.\n\n";

        // ── Step 4: Embed scripts ─────────────────────────────────────────────────
        cout << "  [4/6] Embedding disk wipe scripts...\n\n";

        if (!WinREScriptGenerator::GenerateDiskWipeScripts(mountDir, diskNumber, strategy))
        {
            cerr << "  [ERROR] Failed to write scripts. Discarding mount.\n";
            DISMUtils::DiscardMount(mountDir);
            return false;
        }

        // ── Step 5: Patch winpeshl.ini ─────────────────────────────────────────
        cout << "  [5/6] Patching winpeshl.ini...\n";
        if (!WinREScriptGenerator::PatchWinpeshl(mountDir))
        {
            cerr << "  [ERROR] winpeshl.ini patch failed. Discarding mount.\n";
            DISMUtils::DiscardMount(mountDir);
            return false;
        }

        // ── Step 6: Commit and unmount ───────────────────────────────────────────
        cout << "  [6/6] Committing and unmounting (may take up to 60 s)...\n\n";

        if (!DISMUtils::CommitAndUnmount(mountDir))
        {
            return false;
        }

        cout << "\n  winre.wim patched and committed.\n\n";
        return true;
    }
}
