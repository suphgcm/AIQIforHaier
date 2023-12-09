#include <string>
#include "log.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/logger.h"

std::shared_ptr<spdlog::logger> Logger;

void log_init(const std::string loggerName, const std::string fileName, unsigned int maxFileSize, unsigned int maxFiles)
{
   Logger = spdlog::rotating_logger_mt(loggerName, fileName, maxFileSize, maxFiles);
}

void log_finish()
{
    Logger = nullptr;
}

void log_trace(std::string str)
{
    Logger->trace(str);
}

void log_debug(std::string str)
{
    Logger->debug(str);
}

void log_info(std::string str)
{
    Logger->info(str);
}

void log_warn(std::string str)
{
    Logger->warn(str);
}

void log_error(std::string str)
{
    Logger->error(str);
}