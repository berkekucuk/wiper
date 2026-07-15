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
        QuickFormatAndTrim  ///< Uses quick format + explicit TRIM to instantly erase cells
    };

    /**
     * @brief Mounts winre.wim, embeds a FullDiskWipe startup script, and unmounts.
     *
     * @param winreDir    Path to the directory containing winre.wim.
     * @param diskNumber  Physical disk number to wipe (0-based).
    bool Patch(const std::string& winreDir,
               int                diskNumber,
               WipeStrategy       strategy);
}
