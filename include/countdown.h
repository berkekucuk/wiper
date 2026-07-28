#pragma once

/** @brief Module that manages the countdown timer and user cancellation. */
namespace Countdown
{
    /** @brief Counts down from the given seconds. Returns false if user pressed a key. */
    bool Run(int seconds);
}
