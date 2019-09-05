//
// Created by shenjun on 18-9-26.
//

#pragma once

#include "LoggerPriorityTags.hpp"

namespace logger
{
    namespace internals
    {
        constexpr bool globalLogEnabled()
        {
#ifdef ENABLE_LOGGING
            return true;
#else
            return false;
#endif
        }

        constexpr bool logEnabled(const logger::Priority priority)
        {
        switch (priority)
        {
        case logger::Priority::critical:
#ifndef DISABLE_CRITICAL_LOG
            return true;
#else
            return false;
#endif
        case logger::Priority::debug:
#ifndef DISABLE_DEBUG_LOG
            return true;
#else
            return false;
#endif
        case logger::Priority::error:
#ifndef DISABLE_ERROR_LOG
            return true;
#else
            return false;
#endif
        case logger::Priority::info:
#ifndef DISABLE_INFO_LOG
            return true;
#else
            return false;
#endif
        case logger::Priority::trace:
#ifndef DISABLE_TRACE_LOG
            return true;
#else
            return false;
#endif
        case logger::Priority::warn:
#ifndef DISABLE_WARNING_LOG
            return true;
#else
            return false;
#endif
        default:
            return false;
        }
        }
    } // namespace internals
} // namespace logger

