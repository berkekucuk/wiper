#include "config.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    std::string Trim(const std::string& s)
    {
        auto left = std::find_if_not(s.begin(), s.end(),
            [](unsigned char c) { return std::isspace(c) != 0; });
        auto right = std::find_if_not(s.rbegin(), s.rend(),
            [](unsigned char c) { return std::isspace(c) != 0; }).base();
        return (left < right) ? std::string(left, right) : "";
    }

    void ToLower(std::string& s)
    {
        for (auto& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::string ExeDir()
    {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string path = exePath;
        auto pos = path.find_last_of("\\/");
        if (pos != std::string::npos)
            path.resize(pos);
        return path;
    }

    bool ParseConfig(std::ifstream& in, Config::WipeConfig& config)
    {
        std::string line;
        while (std::getline(in, line))
        {
            line = Trim(line);
            if (line.empty() || line[0] == '#')
                continue;

            auto eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            std::string key = Trim(line.substr(0, eq));
            std::string val = Trim(line.substr(eq + 1));
            auto comment = val.find('#');
            if (comment != std::string::npos)
                val = Trim(val.substr(0, comment));

            ToLower(key);

            if (key == "mode")
            {
                ToLower(val);
                config.mode = val;
            }
            else if (key == "disk_number")
            {
                if (val.empty())
                {
                    std::cerr << "  [ERROR] 'disk_number' is empty in config file.\n";
                    return false;
                }
                try
                {
                    config.diskNumber = std::stoi(val);
                }
                catch (const std::exception&)
                {
                    std::cerr << "  [ERROR] Invalid 'disk_number' value: " << val << "\n";
                    return false;
                }
            }
            else if (key == "hdd_diskwipe_strategy")
            {
                ToLower(val);
                config.hddDiskwipeStrategy = val;
            }
            else if (key == "ssd_diskwipe_strategy")
            {
                ToLower(val);
                config.ssdDiskwipeStrategy = val;
            }
            else if (key == "wipe_list")
            {
                config.wipeList = val;
            }
            else if (key == "hdd_filewipe_strategy")
            {
                ToLower(val);
                config.hddFileStrategy = val;
            }
            else if (key == "ssd_filewipe_strategy")
            {
                ToLower(val);
                config.ssdFileStrategy = val;
            }
            else
            {
                std::cerr << "  [WARN] Unknown config key: " << key << "\n";
            }
        }

        if (config.mode.empty())
        {
            std::cerr << "  [ERROR] 'mode' not set in config file.\n";
            return false;
        }

        if (config.hddDiskwipeStrategy.empty()) config.hddDiskwipeStrategy = "zerofill";
        if (config.ssdDiskwipeStrategy.empty()) config.ssdDiskwipeStrategy = "quickformat";
        if (config.hddFileStrategy.empty())     config.hddFileStrategy     = "zerofill";
        if (config.ssdFileStrategy.empty())     config.ssdFileStrategy     = "trim";

        return true;
    }

    bool TryLoad(const std::string& tryPath, Config::WipeConfig& config)
    {
        std::ifstream in(tryPath);
        if (!in.is_open())
            return false;
        return ParseConfig(in, config);
    }
}

// ---------------------------------------------------------------------------
// Config interface implementation
// ---------------------------------------------------------------------------
namespace Config
{
    bool LoadConfig(WipeConfig& config, const std::string& path)
    {
        if (path.find_first_of("\\/") != std::string::npos)
            return TryLoad(path, config);

        std::string exeDir = ExeDir();
        if (TryLoad(exeDir + "\\" + path, config))
            return true;

        auto pos = exeDir.find_last_of("\\/");
        if (pos != std::string::npos)
        {
            std::string parent = exeDir.substr(0, pos) + "\\" + path;
            if (TryLoad(parent, config))
                return true;
        }

        std::cerr << "  [ERROR] Could not open config file: " << path << "\n";
        return false;
    }
}
