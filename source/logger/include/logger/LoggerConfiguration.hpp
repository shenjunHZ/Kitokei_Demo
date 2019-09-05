//
// Created by shenjun on 18-9-26.
//

#pragma once

#include <iostream>
#include <vector>
#include <spdlog/async.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace
{
    std::size_t max_size = 5 * 1024 * 1024;
    std::size_t max_files = 3;
}
class LoggerConfiguration
{
public:
    static LoggerConfiguration& instance()
    {
        static LoggerConfiguration loggerConf;
        return loggerConf;
    }

    const std::vector<spdlog::sink_ptr>& getSinks(std::string& filePath)
    {
        instance();

        if (sinks.empty())
        {
            if (filePath.empty())
            {
                spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
                sinks.emplace_back(std::move(sink));
            }
            else
            {
                spdlog::sink_ptr sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(static_cast<spdlog::filename_t>(filePath), max_size, max_files);
                sinks.emplace_back(std::move(sink));
            }
        }
        return sinks;
    }

private:
    LoggerConfiguration()
    {
        try
        {
            constexpr unsigned loggingTreadPoolSize = 1;
            constexpr unsigned loggingThreadPoolQueueSize = 8192;
            spdlog::init_thread_pool(loggingThreadPoolQueueSize, loggingTreadPoolSize);
            setLoggingErrorHandler();
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
        }
    }

    ~LoggerConfiguration() = default;

    void setLoggingErrorHandler()
    {
        spdlog::set_error_handler([](const std::string& msg) { std::cerr << "SPDLOG ERROR:" << msg << std::endl; });
    }

private:
    static std::vector<spdlog::sink_ptr> sinks;
};

