#pragma once

#include "disk_wipe_runner.h"

#include <string>

/** @brief Generates and embeds disk-wipe startup scripts into a mounted WinRE image. */
namespace WinREScriptGenerator
{
    /** @brief Generates disk-wipe scripts and embeds them into the mounted WinRE image. */
    bool GenerateDiskWipeScripts(const std::string& mountDir, int diskNumber, DiskWiper::DiskWipeStrategy strategy);

    /** @brief Patches winpeshl.ini to launch the wipe script at WinRE startup. */
    bool PatchWinpeshl(const std::string& mountDir);
}
