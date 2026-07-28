#pragma once

#include "disk_wipe_runner.h"
#include "file_wipe_runner.h"

/** @brief Console utilities: ANSI colors, banners, and string formatting. */
namespace CLIUtils
{
    // -------------------------------------------------------------------------
    // ANSI color / style escape codes.
    // Call EnableColors() once at startup to activate them on the Windows console.
    // -------------------------------------------------------------------------
    namespace Colors
    {
        constexpr const char* RESET   = "\033[0m";
        constexpr const char* BOLD    = "\033[1m";
        constexpr const char* RED     = "\033[31m";
        constexpr const char* BRED    = "\033[1;31m";
        constexpr const char* GREEN   = "\033[32m";
        constexpr const char* BGREEN  = "\033[1;32m";
        constexpr const char* YELLOW  = "\033[33m";
        constexpr const char* CYAN    = "\033[36m";
        constexpr const char* BCYAN   = "\033[1;36m";
        constexpr const char* MAGENTA = "\033[35m";
    }

    /**
     * @brief Enables ANSI virtual terminal processing on stdout and stderr.
     *        Must be called once before any colored output is printed.
     */
    void EnableColors();

    /** @brief Returns a human-readable string for a DiskWipeStrategy. */
    const char* StrategyToString(DiskWiper::DiskWipeStrategy s);

    /** @brief Returns a human-readable string for a FileWipeStrategy. */
    const char* FileWipeStrategyToString(FileWiper::FileWipeStrategy s);

    /** @brief Prints the initial banner. */
    void PrintBanner(int wipeDiskNumber,
                     DiskWiper::DiskWipeStrategy hddStrategy,
                     DiskWiper::DiskWipeStrategy ssdStrategy);

    /** @brief Prints the banner for the file-wipe mode. */
    void PrintFileWipeBanner(FileWiper::FileWipeStrategy strategy);
}
