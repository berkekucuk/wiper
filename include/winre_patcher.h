#pragma once

#include "disk_wipe_runner.h"

#include <string>

/** @brief WinRE patching: mounts winre.wim, embeds scripts, and rollback on failure. */
namespace WinREPatcher
{
    /** @brief Mounts winre.wim, embeds disk-wipe startup script, and unmounts. Creates .bak backup before. */
    bool Patch(const std::string& winreDir, int diskNumber, DiskWiper::DiskWipeStrategy strategy);

    /** @brief Restores winre.wim from <wimPath>.bak. Call this on user cancel after Patch(). */
    bool Rollback(const std::string& wimPath);
}
