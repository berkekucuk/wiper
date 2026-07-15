#pragma once

#include <string>

namespace WinREPatcher
{
    /**
     * @brief Mounts winre.wim, embeds a FullDiskWipe startup script, and unmounts.
     *
     * @param winreDir    Path to the directory containing winre.wim.
     * @param diskNumber  Physical disk number to wipe (0-based).
     * @param isSSD       True if the target disk is an SSD (requires TRIM), false for HDD.
     * @return true  Successfully patched.
     * @return false Failed to patch.
     */
    bool Patch(const std::string& winreDir, int diskNumber, bool isSSD);
}
