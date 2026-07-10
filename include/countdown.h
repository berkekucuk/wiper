#pragma once

/**
 * @brief Module that manages the countdown timer and user cancellation.
 */
namespace Countdown
{
    /**
     * @brief Counts down from the given number of seconds.
     *        If the user presses any key during the countdown, the operation is cancelled.
     *
     * @param seconds Starting value for the countdown (in seconds).
     * @return true  Countdown completed; proceed with the operation.
     * @return false User cancelled.
     */
    bool Run(int seconds);
}
