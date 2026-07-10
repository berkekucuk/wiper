#include "countdown.h"
#include "reboot.h"
#include "winre_finder.h"
#include "winre_patcher.h"

#include <cstdlib>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// Configuration — change this to run a different command in the WinRE shell.
// ---------------------------------------------------------------------------
constexpr const char* kTestCommand = "diskpart";

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
    std::cout << "  [1/4] Locating WinRE...\n\n";

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

    // ---- 2. Patch winre.wim ----
    std::cout << "  [2/4] Patching WinRE image to launch '" << kTestCommand
              << "' on boot...\n\n";

    if (!WinREPatcher::Patch(winrePath, kTestCommand))
    {
        std::cerr << "  [ERROR] Failed to patch WinRE image. Aborting.\n\n";
        std::cout << "  Press any key to exit...\n";
        std::cin.get();
        return EXIT_FAILURE;
    }

    // ---- 3. Countdown ----
    std::cout << "  [3/4] Starting countdown...\n\n";

    if (!Countdown::Run(5))
    {
        std::cout << "  Operation cancelled. System will not be restarted.\n\n";
        std::cout << "  Press any key to exit...\n";
        std::cin.get();
        return EXIT_SUCCESS;
    }

    // ---- 4. Reboot into WinRE ----
    std::cout << "  [4/4] Initiating WinRE reboot...\n\n";

    if (!Reboot::BootIntoWinRE())
    {
        std::cerr << "\n  [ERROR] Reboot could not be initiated.\n\n";
        std::cout << "  Press any key to exit...\n";
        std::cin.get();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
