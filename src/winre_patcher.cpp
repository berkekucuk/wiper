#include "winre_patcher.h"
#include "dism_utils.h"
#include "wim_resolver.h"
#include "winre_script_generator.h"
#include "cli_utils.h"

#include <windows.h>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    /** @brief Returns a writable backup path in %TEMP%, keyed by PID. */
    std::string BuildBackupPath()
    {
        char tmp[MAX_PATH] = {};
        GetTempPathA(sizeof(tmp), tmp);

        std::string path = tmp;
        if (!path.empty() && path.back() == '\\')
            path.pop_back();

        path += "\\winre_wiper_backup_";
        path += std::to_string(GetCurrentProcessId());
        path += ".wim";
        return path;
    }
} 

// ---------------------------------------------------------------------------
// WinREPatcher interface implementation
// ---------------------------------------------------------------------------
namespace WinREPatcher
{
    bool Patch(const std::string& winreDir, int diskNumber, DiskWiper::DiskWipeStrategy strategy)
    {
        // ── Step 1: Resolve winre.wim ─────────────────────────────────────────────
        std::cout << "  " << CLIUtils::Colors::CYAN << "[1]" << CLIUtils::Colors::RESET
             << " Resolving winre.wim...\n";
        const std::string wimPath = WIMResolver::ResolveWimPath(winreDir);

        if (wimPath.empty())
        {
            std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET << " winre.wim not found.\n";
            return false;
        }

        // ── Step 1b: Backup winre.wim to %TEMP% before any modification ──────────
        // The original winre.wim lives on a recovery partition accessed via a
        // \\?\GLOBALROOT\device\... path that is not directly writable. Writing
        // the backup to %TEMP% avoids that restriction entirely.
        const std::string bakPath = BuildBackupPath();

        if (!CopyFileA(wimPath.c_str(), bakPath.c_str(), /*bFailIfExists=*/FALSE))
        {
            std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                 << " Could not create winre.wim backup (code "
                 << GetLastError() << "). Aborting for safety.\n";
            return false;
        }

        std::cout << "  Source       : " << wimPath  << "\n";
        std::cout << "  Backup       : " << bakPath  << "\n";
        std::cout << "\n";

        // ── Step 2: Create mount directory ────────────────────────────────────
        std::cout << "  " << CLIUtils::Colors::CYAN << "[2]" << CLIUtils::Colors::RESET << " Creating mount directory...\n";
        const std::string mountDir = DISMUtils::BuildMountDir();
        std::cout << "  Mount dir    : " << mountDir << "\n";

        if (!CreateDirectoryA(mountDir.c_str(), nullptr))
        {
            std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                 << " CreateDirectory failed (code " << GetLastError() << ")\n";
            return false;
        }

        // ── Step 3: Mount via DISM ──────────────────────────────────────────
        std::cout << "\n  " << CLIUtils::Colors::CYAN << "[3]" << CLIUtils::Colors::RESET
             << " Mounting WinRE image via DISM (may take up to 60 s)...\n\n";

        if (!DISMUtils::MountImage(wimPath, mountDir))
        {
            return false;
        }
        std::cout << "\n  Image mounted.\n\n";

        // ── Step 4: Embed scripts ─────────────────────────────────────────────────
        std::cout << "  " << CLIUtils::Colors::CYAN << "[4]" << CLIUtils::Colors::RESET << " Embedding disk wipe scripts...\n\n";

        if (!WinREScriptGenerator::GenerateDiskWipeScripts(mountDir, diskNumber, strategy))
        {
            std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                 << " Failed to write scripts. Discarding mount.\n";
            DISMUtils::DiscardMount(mountDir);
            return false;
        }

        // ── Step 5: Patch winpeshl.ini ─────────────────────────────────────────
        std::cout << "  " << CLIUtils::Colors::CYAN << "[5]" << CLIUtils::Colors::RESET << " Patching winpeshl.ini...\n";
        if (!WinREScriptGenerator::PatchWinpeshl(mountDir))
        {
            std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                 << " winpeshl.ini patch failed. Discarding mount.\n";
            DISMUtils::DiscardMount(mountDir);
            return false;
        }

        // ── Step 6: Commit and unmount ───────────────────────────────────────────
        std::cout << "  " << CLIUtils::Colors::CYAN << "[6]" << CLIUtils::Colors::RESET
             << " Committing and unmounting (may take up to 60 s)...\n\n";

        if (!DISMUtils::CommitAndUnmount(mountDir))
        {
            return false;
        }

        std::cout << "\n  " << CLIUtils::Colors::BGREEN << "winre.wim patched and committed." << CLIUtils::Colors::RESET << "\n\n";

        // The backup in %TEMP% stays until:
        //   - Rollback() restores and deletes it (user cancelled countdown), or
        //   - System reboots into WinRE and wipes the disk (backup becomes irrelevant).
        return true;
    }

    bool Rollback(const std::string& wimPath)
    {
        // Derive the same %TEMP% backup path that Patch() created.
        const std::string bakPath = BuildBackupPath();

        if (GetFileAttributesA(bakPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            std::cerr << "  " << CLIUtils::Colors::YELLOW << "[ROLLBACK]" << CLIUtils::Colors::RESET
                 << " Backup not found: " << bakPath << "\n";
            return false;
        }

        std::cout << "  " << CLIUtils::Colors::YELLOW << "[ROLLBACK]" << CLIUtils::Colors::RESET
             << " Restoring original winre.wim from backup...\n";

        // Overwrite the patched .wim with the original backup.
        if (!CopyFileA(bakPath.c_str(), wimPath.c_str(), /*bFailIfExists=*/FALSE))
        {
            std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                 << " Rollback CopyFile failed (code " << GetLastError() << ").\n"
                 << "           Backup : " << bakPath << "\n"
                 << "           Target : " << wimPath << "\n"
                 << "  Manually copy the backup file to restore WinRE.\n";
            return false;
        }

        DeleteFileA(bakPath.c_str());
        std::cout << "  " << CLIUtils::Colors::BGREEN << "[ROLLBACK]" << CLIUtils::Colors::RESET
             << " winre.wim restored successfully. Backup deleted.\n\n";
        return true;
    }
}
