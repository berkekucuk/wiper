#pragma once

/**
 * @brief Module that handles rebooting the system into WinRE.
 */
namespace Reboot
{
    /**
     * @brief Reboots the system into the Windows Recovery Environment (WinRE).
     *        Sets the one-time WinRE boot flag via `reagentc /boottore`,
     *        then triggers a system restart through the Windows shutdown API.
     *
     * @return true  Commands were dispatched successfully (system is restarting).
     * @return false An error occurred; reboot was not initiated.
     */
    bool BootIntoWinRE();
}
