#include <string>
#include "log.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/logger.h"

std::shared_ptr<spdlog::logger> Logger;
std::atomic<bool> keep_flushing(true);
std::thread flush_thread;

void flush_thread_func(std::chrono::seconds interval) {
    while (keep_flushing.load()) {
        std::this_thread::sleep_for(interval);
        Logger->flush();
    }
}

void log_init(const std::string loggerName, const std::string fileName, unsigned int maxFileSize, unsigned int maxFiles)
{
   Logger = spdlog::rotating_logger_mt(loggerName, fileName, maxFileSize, maxFiles);
   Logger->set_level(spdlog::level::info);
   flush_thread = std::thread(flush_thread_func, std::chrono::seconds(3));
}

void log_finish()
{
    keep_flushing.store(false);

    if (flush_thread.joinable()) {
        flush_thread.join();
    }
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