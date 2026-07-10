#include "countdown.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <conio.h>

#include <iostream>

namespace Countdown
{
    bool Run(int seconds)
    {
        std::cout << "  Press any key to cancel.\n\n";

        for (int remaining = seconds; remaining > 0; --remaining)
        {
            // Overwrite the current line with the updated countdown
            std::cout << "\r  Rebooting into WinRE in: "
                      << remaining
                      << " second(s)...   " << std::flush;

            // Split each second into 100 ms slices so key detection
            // responds within ~100 ms.
            for (int ms = 0; ms < 10; ++ms)
            {
                if (_kbhit())
                {
                    _getch(); // consume the key
                    std::cout << "\n\n  [CANCELLED] Operation stopped by user.\n\n";
                    return false;
                }
                Sleep(100);
            }
        }

        std::cout << "\r  Rebooting...                                      \n\n";
        return true;
    }
} // namespace Countdown
