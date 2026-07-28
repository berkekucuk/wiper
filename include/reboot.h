#pragma once

/** @brief Module that handles rebooting the system into WinRE. */
namespace Reboot
{
    /** @brief Reboots into WinRE via reagentc /boottore and the Windows shutdown API. */
    bool BootIntoWinRE();
}
