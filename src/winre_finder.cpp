#include "winre_finder.h"
#include "cli_utils.h"
#include <windows.h>

#include <array>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;
using namespace CLIUtils::Colors;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    /**
     * @brief Executes a command-line command and returns its stdout as a string.
     *
     * @param cmd Command to execute.
     * @return stdout output of the command, or an empty string on failure.
     */
    string RunCommand(const string& cmd)
    {
        string result;
        array<char, 256> buffer{};

        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe)
            return result;

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
            result += buffer.data();

        _pclose(pipe);
        return result;
    }

    /**
     * @brief Reads the WinRE path from the Windows Registry.
     *
     * @param outPath Receives the path string on success.
     * @return true  Read was successful.
     * @return false Registry key not found or could not be read.
     */
    bool ReadFromRegistry(string& outPath)
    {
        // WinRE path may be stored under either of these registry locations.
        const char* keys[] = {
            "SOFTWARE\\Microsoft\\RecoveryEnvironment",
            "SYSTEM\\CurrentControlSet\\Control\\MiniNT"
        };
        const char* valueNames[] = {
            "WinRELocation",
            "WinREPath"
        };

        for (int i = 0; i < 2; ++i)
        {
            HKEY hKey = nullptr;
            LONG ret = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keys[i], 0, KEY_READ, &hKey);
            if (ret != ERROR_SUCCESS)
                continue;

            char  buf[MAX_PATH] = {};
            DWORD bufSize = sizeof(buf);
            DWORD type    = 0;

            ret = RegQueryValueExA(hKey, valueNames[i], nullptr, &type,
                                   reinterpret_cast<LPBYTE>(buf), &bufSize);
            RegCloseKey(hKey);

            if (ret == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ))
            {
                outPath = buf;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Parses `reagentc /info` output to extract the WinRE path and status.
     *
     * @param output    Raw output from reagentc.
     * @param outPath   Receives the extracted path.
     * @param outStatus Receives the extracted status.
     * @return true  Parsing succeeded.
     * @return false Relevant lines were not found in the output.
     */
    bool ParseReagentcOutput(const string& output,
                             string&       outPath,
                             string&       outStatus)
    {
        bool foundPath   = false;
        bool foundStatus = false;

        istringstream stream(output);
        string        line;

        while (getline(stream, line))
        {
            // Strip leading and trailing whitespace
            auto trim = [](const string& s) -> string {
                size_t start = s.find_first_not_of(" \t\r\n");
                size_t end   = s.find_last_not_of(" \t\r\n");
                return (start == string::npos) ? "" : s.substr(start, end - start + 1);
            };

            string trimmed = trim(line);

            // "Windows RE location:" line
            if (!foundPath && trimmed.find("Windows RE location:") != string::npos)
            {
                size_t colon = trimmed.find(':');
                if (colon != string::npos)
                {
                    outPath   = trim(trimmed.substr(colon + 1));
                    foundPath = !outPath.empty();
                }
            }

            // "Windows RE status:" line
            if (!foundStatus && trimmed.find("Windows RE status:") != string::npos)
            {
                size_t colon = trimmed.find(':');
                if (colon != string::npos)
                {
                    outStatus   = trim(trimmed.substr(colon + 1));
                    foundStatus = !outStatus.empty();
                }
            }
        }

        return foundPath;
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// WinREFinder interface implementation
// ---------------------------------------------------------------------------
namespace WinREFinder
{
    bool FindLocation(string& outPath, string& outStatus)
    {
        outStatus = "Unknown";

        // 1. Try reagentc /info first (most reliable source)
        // Use the full system path to prevent PATH-based binary hijacking.
        string reagentOutput = RunCommand("C:\\Windows\\System32\\reagentc.exe /info 2>&1");
        if (!reagentOutput.empty())
        {
            if (ParseReagentcOutput(reagentOutput, outPath, outStatus))
                return true;
        }

        // 2. Fallback: read from Registry
        if (ReadFromRegistry(outPath))
            return true;

        return false;
    }

    void PrintInfo(const string& path, const string& status)
    {
        cout << "\n";
        cout << "  " << CYAN << "+--------------------------------------------------+" << RESET << "\n";
        cout << "  " << CYAN << "|" << RESET << "        Windows Recovery Environment (WinRE)      " << CYAN << "|" << RESET << "\n";
        cout << "  " << CYAN << "+--------------------------------------------------+" << RESET << "\n";
        cout << "  " << CYAN << "|" << RESET << " Status : " << status << "\n";
        cout << "  " << CYAN << "|" << RESET << " Path   : " << path   << "\n";
        cout << "  " << CYAN << "+--------------------------------------------------+" << RESET << "\n";
        cout << "\n";
    }
} // namespace WinREFinder
