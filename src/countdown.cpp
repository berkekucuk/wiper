#include "countdown.h"
#include "cli_utils.h"

#include <windows.h>
#include <conio.h>
#include <iostream>

namespace Countdown
{
    bool Run(int seconds)
    {
        std::cout << "  Press " << CLIUtils::Colors::YELLOW << "any key" << CLIUtils::Colors::RESET << " to cancel.\n\n";

        for (int remaining = seconds; remaining > 0; --remaining)
        {
            // Overwrite the current line with the updated countdown
            std::cout << "\r  " << CLIUtils::Colors::CYAN << "Rebooting into WinRE in: "
                      << CLIUtils::Colors::BOLD << remaining << CLIUtils::Colors::RESET << CLIUtils::Colors::CYAN
                      << " second(s)..." << CLIUtils::Colors::RESET << "   " << std::flush;

            // Split each second into 100 ms slices so key detection
            // responds within ~100 ms.
            for (int ms = 0; ms < 10; ++ms)
            {
                if (_kbhit())
                {
                    _getch(); // consume the key
                    std::cout << "\n\n  " << CLIUtils::Colors::YELLOW << "[CANCELLED]" << CLIUtils::Colors::RESET
                         << " Operation stopped by user.\n\n";
                    return false;
                }
                Sleep(100);
            }
        }

        std::cout << "\r  " << CLIUtils::Colors::BGREEN << "Rebooting..." << CLIUtils::Colors::RESET
             << "                                      \n\n";
        return true;
    }
} 