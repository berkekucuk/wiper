#pragma once

#include "disk_wipe_runner.h"

#include <string>

/** @brief WinRE patching: mounts winre.wim, embeds scripts, and rollback on failure. */
namespace WinREPatcher
{
    /**
     * @brief Mounts winre.wim, embeds a disk wipe startup script, and unmounts.
     *        Before mounting, creates a backup at <wimPath>.bak for rollback.
     *
     * @param winreDir    Path to the directory containing winre.wim.
     * @param diskNumber  Physical disk number to wipe (0-based).
     * @param strategy    The selected wipe strategy.
     * @return true  Successfully patched.
     * @return false Failed to patch.
     */
    bool Patch(const std::string& winreDir, int diskNumber, DiskWiper::DiskWipeStrategy strategy);

    /**
     * @brief Restores winre.wim from the backup created by Patch().
     *        Copies <wimPath>.bak → <wimPath> and deletes the .bak file.
     *        Call this when the user cancels after a successful Patch().
     *
     * @param wimPath Full path to the .wim file (NOT the .bak path).
     * @return true  Restored successfully.
     * @return false Backup not found or copy failed.
     */
    bool Rollback(const std::string& wimPath);
}
