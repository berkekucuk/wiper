#pragma once

#include <string>

namespace WinREPatcher
{
    /**
     * @brief Defines how the target disk should be wiped.
     */
    enum class WipeStrategy
    {
        ZeroFill,           ///< Uses 'diskpart clean all' (1-pass zero fill) on every sector
        RandomFill,         ///< Creates partition, uses 'format /P:1' (zero + random data), then cleans
        QuickFormatAndTrim  ///< Uses quick format + explicit TRIM to instantly erase cells
    };

    /**
     * @brief Mounts winre.wim, embeds a disk wipe startup script, and unmounts.
     *        Before mounting, creates a backup at <wimPath>.bak for rollback.
     *
     * @param winreDir    Path to the directory containing winre.wim.
     * @param diskNumber  Physical disk number to wipe (0-based).
     * @param strategy    The selected wipe strategy (ZeroFill or QuickFormatAndTrim).
     * @return true  Successfully patched.
     * @return false Failed to patch.
     */
    bool Patch(const std::string& winreDir, int diskNumber, WipeStrategy strategy);

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
