#include "countdown.h"
#include "cli_utils.h"

#include <windows.h>
#include <conio.h>
#include <iostream>

using namespace std;
using namespace CLIUtils::Colors;

namespace Countdown
{
    bool Run(int seconds)
    {
        cout << "  Press " << YELLOW << "any key" << RESET << " to cancel.\n\n";

        for (int remaining = seconds; remaining > 0; --remaining)
        {
            // Overwrite the current line with the updated countdown
            cout << "\r  " << CYAN << "Rebooting into WinRE in: "
                      << BOLD << remaining << RESET << CYAN
                      << " second(s)..." << RESET << "   " << flush;

            // Split each second into 100 ms slices so key detection
            // responds within ~100 ms.
            for (int ms = 0; ms < 10; ++ms)
            {
                if (_kbhit())
                {
                    _getch(); // consume the key
                    cout << "\n\n  " << YELLOW << "[CANCELLED]" << RESET
                         << " Operation stopped by user.\n\n";
                    return false;
                }
                Sleep(100);
            }
        }

        cout << "\r  " << BGREEN << "Rebooting..." << RESET
             << "                                      \n\n";
        return true;
    }
} // namespace Countdown
