#include "winre_patcher.h"
#include "dism_utils.h"
#include "wim_resolver.h"
#include "winre_script_generator.h"
#include "cli_utils.h"

#include <windows.h>
#include <iostream>
#include <string>

using namespace std;
using namespace CLIUtils::Colors;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    /**
     * @brief Returns a writable backup path in %TEMP%, keyed by PID.
     *        e.g. C:\Users\user\AppData\Local\Temp\winre_wiper_backup_1234.wim
     *
     * Using %TEMP% avoids writing to the read-only/unmounted recovery partition
     * where winre.wim lives (\\?\GLOBALROOT\device\... paths are not writable
     * via CopyFileA).
     */
    string BuildBackupPath()
    {
        char tmp[MAX_PATH] = {};
        GetTempPathA(sizeof(tmp), tmp);

        string path = tmp;
        if (!path.empty() && path.back() == '\\')
            path.pop_back();

        path += "\\winre_wiper_backup_";
        path += to_string(GetCurrentProcessId());
        path += ".wim";
        return path;
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// WinREPatcher interface implementation
// ---------------------------------------------------------------------------
namespace WinREPatcher
{
    bool Patch(const string& winreDir, int diskNumber, WipeStrategy strategy)
    {
        // ── Step 1: Resolve winre.wim ─────────────────────────────────────────────
        cout << "  " << CYAN << "[1/6]" << RESET
             << " Resolving winre.wim...\n";
        const string wimPath = WIMResolver::ResolveWimPath(winreDir);

        if (wimPath.empty())
        {
            cerr << "  " << BRED << "[ERROR]" << RESET << " winre.wim not found.\n";
            return false;
        }

        // ── Step 1b: Backup winre.wim to %TEMP% before any modification ──────────
        // The original winre.wim lives on a recovery partition accessed via a
        // \\?\GLOBALROOT\device\... path that is not directly writable. Writing
        // the backup to %TEMP% avoids that restriction entirely.
        const string bakPath = BuildBackupPath();

        if (!CopyFileA(wimPath.c_str(), bakPath.c_str(), /*bFailIfExists=*/FALSE))
        {
            cerr << "  " << BRED << "[ERROR]" << RESET
                 << " Could not create winre.wim backup (code "
                 << GetLastError() << "). Aborting for safety.\n";
            return false;
        }

        cout << "  Source       : " << wimPath  << "\n";
        cout << "  Backup       : " << bakPath  << "\n";
        cout << "  Wipe target  : Disk " << diskNumber << " (FULL DISK)\n";
        cout << "  Strategy     : ";
        switch (strategy)
        {
            case WipeStrategy::ZeroFill:           cout << "Zero Fill (diskpart clean all)"; break;
            case WipeStrategy::RandomFill:         cout << "Random Fill (format /P:1)";      break;
            case WipeStrategy::QuickFormatAndTrim: cout << "Quick Format + explicit TRIM";   break;
        }
        cout << "\n\n";

        // ── Step 2: Create mount directory ────────────────────────────────────
        cout << "  " << CYAN << "[2/6]" << RESET << " Creating mount directory...\n";
        const string mountDir = DISMUtils::BuildMountDir();
        cout << "  Mount dir    : " << mountDir << "\n";

        if (!CreateDirectoryA(mountDir.c_str(), nullptr))
        {
            cerr << "  " << BRED << "[ERROR]" << RESET
                 << " CreateDirectory failed (code " << GetLastError() << ")\n";
            return false;
        }

        // ── Step 3: Mount via DISM ──────────────────────────────────────────
        cout << "\n  " << CYAN << "[3/6]" << RESET
             << " Mounting WinRE image via DISM (may take up to 60 s)...\n\n";

        if (!DISMUtils::MountImage(wimPath, mountDir))
        {
            return false;
        }
        cout << "\n  Image mounted.\n\n";

        // ── Step 4: Embed scripts ─────────────────────────────────────────────────
        cout << "  " << CYAN << "[4/6]" << RESET << " Embedding disk wipe scripts...\n\n";

        if (!WinREScriptGenerator::GenerateDiskWipeScripts(mountDir, diskNumber, strategy))
        {
            cerr << "  " << BRED << "[ERROR]" << RESET
                 << " Failed to write scripts. Discarding mount.\n";
            DISMUtils::DiscardMount(mountDir);
            return false;
        }

        // ── Step 5: Patch winpeshl.ini ─────────────────────────────────────────
        cout << "  " << CYAN << "[5/6]" << RESET << " Patching winpeshl.ini...\n";
        if (!WinREScriptGenerator::PatchWinpeshl(mountDir))
        {
            cerr << "  " << BRED << "[ERROR]" << RESET
                 << " winpeshl.ini patch failed. Discarding mount.\n";
            DISMUtils::DiscardMount(mountDir);
            return false;
        }

        // ── Step 6: Commit and unmount ───────────────────────────────────────────
        cout << "\n  " << CYAN << "[6/6]" << RESET
             << " Committing and unmounting (may take up to 60 s)...\n\n";

        if (!DISMUtils::CommitAndUnmount(mountDir))
        {
            return false;
        }

        cout << "\n  " << BGREEN << "winre.wim patched and committed." << RESET << "\n\n";

        // The backup in %TEMP% stays until:
        //   - Rollback() restores and deletes it (user cancelled countdown), or
        //   - System reboots into WinRE and wipes the disk (backup becomes irrelevant).
        return true;
    }

    bool Rollback(const string& wimPath)
    {
        // Derive the same %TEMP% backup path that Patch() created.
        const string bakPath = BuildBackupPath();

        if (GetFileAttributesA(bakPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            cerr << "  " << YELLOW << "[ROLLBACK]" << RESET
                 << " Backup not found: " << bakPath << "\n";
            return false;
        }

        cout << "  " << YELLOW << "[ROLLBACK]" << RESET
             << " Restoring original winre.wim from backup...\n";

        // Overwrite the patched .wim with the original backup.
        if (!CopyFileA(bakPath.c_str(), wimPath.c_str(), /*bFailIfExists=*/FALSE))
        {
            cerr << "  " << BRED << "[ERROR]" << RESET
                 << " Rollback CopyFile failed (code " << GetLastError() << ").\n"
                 << "           Backup : " << bakPath << "\n"
                 << "           Target : " << wimPath << "\n"
                 << "  Manually copy the backup file to restore WinRE.\n";
            return false;
        }

        DeleteFileA(bakPath.c_str());
        cout << "  " << BGREEN << "[ROLLBACK]" << RESET
             << " winre.wim restored successfully. Backup deleted.\n\n";
        return true;
    }
}
