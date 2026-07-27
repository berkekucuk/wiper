#pragma once

#include <string>

/** @brief Disk-wipe operations: strategy selection and full-disk wipe via WinRE patching. */
namespace DiskWiper
{
    /** @brief Disk-wipe strategy enumeration. */
    enum class DiskWipeStrategy
    {
        ZeroFill,
        RandomFill,
        QuickFormat
    };

    /** @brief Converts a string to DiskWipeStrategy (default: ZeroFill). */
    DiskWipeStrategy StrategyFromString(const std::string& s);

    /** @brief Runs the full disk-wipe routine: locates WinRE, patches it, and reboots. */
    int RunDiskWipe(int diskNumber, DiskWipeStrategy hddStrategy, DiskWipeStrategy ssdStrategy);
}
