#pragma once

#include <string>

/** @brief Resolves the full path to winre.wim, with fallback drive scanning. */
namespace WIMResolver
{
    /** @brief Resolves winre.wim path. Tries the provided directory first, then scans drives. */
    std::string ResolveWimPath(const std::string& winreDir);
}
