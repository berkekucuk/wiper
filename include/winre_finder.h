#pragma once

#include <string>

/** @brief Functions for locating and managing the Windows Recovery Environment (WinRE). */
namespace WinREFinder
{
    /** @brief Resolves the WinRE path via reagentc /info, falls back to Registry. */
    bool FindLocation(std::string& outPath, std::string& outStatus);

    /** @brief Prints WinRE path and status in a formatted box. */
    void PrintInfo(const std::string& path, const std::string& status);

    /** @brief Enables WinRE via reagentc /enable if currently disabled. */
    bool EnsureEnabled();
}
