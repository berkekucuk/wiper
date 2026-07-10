#include "countdown.h"
#include "reboot.h"
#include "winre_finder.h"

#include <cstdlib>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// Helper: banner
// ---------------------------------------------------------------------------
static void PrintBanner()
{
    std::cout << "\n";
    std::cout << "  ====================================================\n";
    std::cout << "         WinRE Boot Launcher  |  github.com/bkucuk    \n";
    std::cout << "  ====================================================\n";
    std::cout << "\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    PrintBanner();

    // ---- 1. Locate WinRE ----
    std::cout << "  [1/3] Locating WinRE...\n\n";

    std::string winrePath;
    std::string winreStatus;

    if (!WinREFinder::FindLocation(winrePath, winreStatus))
    {
        std::cerr << "  [ERROR] WinRE location could not be determined.\n"
                     "          WinRE may not be installed or enabled,\n"
                     "          or the program may not be running as Administrator.\n\n";
        std::cout << "  Press any key to exit...\n";
        std::cin.get();
        return EXIT_FAILURE;
    }

    WinREFinder::PrintInfo(winrePath, winreStatus);

    // ---- 2. Countdown ----
    std::cout << "  [2/3] Starting countdown...\n\n";

    if (!Countdown::Run(5))
    {
        // User cancelled
        std::cout << "  Operation cancelled. System will not be restarted.\n\n";
        std::cout << "  Press any key to exit...\n";
        std::cin.get();
        return EXIT_SUCCESS;
    }

    // ---- 3. Reboot into WinRE ----
    std::cout << "  [3/3] Initiating WinRE reboot...\n\n";

    if (!Reboot::BootIntoWinRE())
    {
        std::cerr << "\n  [ERROR] Reboot could not be initiated.\n\n";
        std::cout << "  Press any key to exit...\n";
        std::cin.get();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
