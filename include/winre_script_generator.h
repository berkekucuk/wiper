#pragma once

#include <string>
#include "disk_detector.h"
#include "winre_patcher.h"

namespace WinREScriptGenerator
{


    /**
     * @brief Embeds all necessary scripts for a disk wipe into the mounted WinRE image.
     */
    bool GenerateDiskWipeScripts(const std::string& mountDir,
                                 int diskNumber,
                                 WinREPatcher::WipeStrategy strategy);

    /**
     * @brief Updates winpeshl.ini inside the mounted WinRE image to launch
     *        wipe_launch.cmd instead of the standard recovery UI.
     */
    bool PatchWinpeshl(const std::string& mountDir);
}
