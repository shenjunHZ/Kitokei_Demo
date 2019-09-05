//
// Created by shenjun on 18-9-26.
//
#pragma once

#include "LoggerConfiguration.hpp"
#include "LoggerFwd.hpp"
#include "LoggerImpl.hpp"

#define LOG_STR_H(x) #x
#define LOG_STR_HELPER(x) LOG_STR_H(x)

#define LOG_TRACE_MSG(...) \
    LOG_MSG_IMPL(logger::getLogger(), trace, "[" __FILE__ ":" LOG_STR_HELPER(__LINE__) "] " __VA_ARGS__)
#define LOG_DEBUG_MSG(...) LOG_MSG_IMPL(logger::getLogger(), debug, "[" __FILE__ ":" LOG_STR_HELPER(__LINE__) "] " __VA_ARGS__)
#define LOG_INFO_MSG(logger, ...) LOG_MSG_IMPL((logger), info, "[" __FILE__ ":" LOG_STR_HELPER(__LINE__) "] " __VA_ARGS__)
#define LOG_WARNING_MSG(...) LOG_MSG_IMPL(logger::getLogger(), warn, __VA_ARGS__)
#define LOG_ERROR_MSG(...) LOG_MSG_IMPL(logger::getLogger(), error, __VA_ARGS__)
#define LOG_CRITICAL_MSG(logger, ...) LOG_MSG_IMPL((logger), critical, __VA_ARGS__)
#define LOG_MESSAGE(action, message) \
    LOG_MSG_IMPL(logger::getLogger(), trace, "[" __FILE__ ":" LOG_STR_HELPER(__LINE__) "] {}\n{}", action, message)

namespace logger
{
    spdlog::logger& getLogger(std::string filePath = "");
} // namespace logger