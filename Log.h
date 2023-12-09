#ifndef _LOG_H
#define _LOG_H

void log_init(const std::string loggerName, const std::string fileName, unsigned int maxFileSize, unsigned int maxFiles);
void log_finish();
void log_trace(std::string str);
void log_debug(std::string str);
void log_info(std::string str);
void log_warn(std::string str);
void log_error(std::string str);

#endif
