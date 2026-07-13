#pragma once

#include <string>

namespace WIMResolver
{
    /**
     * @brief Resolves the full path to winre.wim.
     *        Tries the provided directory first, then scans drives as fallback.
     *
     * @param winreDir The path output by ReAgentC (or user override).
     * @return std::string The full path to winre.wim, or empty if not found.
     */
    std::string ResolveWimPath(const std::string& winreDir);
}
