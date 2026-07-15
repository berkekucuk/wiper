#include "countdown.h"

#include <windows.h>
#include <conio.h>
#include <iostream>

using namespace std;

namespace Countdown
{
    bool Run(int seconds)
    {
        cout << "  Press any key to cancel.\n\n";

        for (int remaining = seconds; remaining > 0; --remaining)
        {
            // Overwrite the current line with the updated countdown
            cout << "\r  Rebooting into WinRE in: "
                      << remaining
                      << " second(s)...   " << flush;

            // Split each second into 100 ms slices so key detection
            // responds within ~100 ms.
            for (int ms = 0; ms < 10; ++ms)
            {
                if (_kbhit())
                {
                    _getch(); // consume the key
                    cout << "\n\n  [CANCELLED] Operation stopped by user.\n\n";
                    return false;
                }
                Sleep(100);
            }
        }

        cout << "\r  Rebooting...                                      \n\n";
        return true;
    }
} // namespace
