/// @file log.h
/// @brief Log handler module header
/// @author Kenny Huang


#ifndef __HEADER_LOG__
#define __HEADER_LOG__

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define __ACT_FILE__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define _A_ __ACT_FILE__, __LINE__, __PRETTY_FUNCTION__

#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#define __LOG_DEBUG    0
#define LOG_DEBUG      __LOG_DEBUG, _A_

#ifdef LOG_NOTICE
#undef LOG_NOTICE
#endif
#define __LOG_NOTICE   1
#define LOG_NOTICE     __LOG_NOTICE, _A_

#ifdef LOG_WARNING
#undef LOG_WARNING
#endif
#define __LOG_WARNING  2
#define LOG_WARNING    __LOG_WARNING, _A_

#ifdef LOG_ERROR
#undef LOG_ERROR
#endif
#define __LOG_ERROR    3
#define LOG_ERROR      __LOG_ERROR, _A_

#ifdef NO_LOG
#define SetLogConfig(...)
#define StartLogger(...)
#define LOG(...)
#else

/// @defgroup log Log module
/// @{

/// @brief A config callback function to setup log's configuration
/// @param[in] name The config item name
/// @param[in] value The config item value
/// @return 0: On success
/// @return -1: On error
int SetLogConfig(const char *name, const char *value);

/// @brief Start the logger. Open the log file if the configuration require log
///        it to file. And switch to screen log mode if fail to open.
/// @return 0: On success
/// @return -1: On error
int StartLogger(void);

/// @brief Record an item of log
/// @param[in] level The log message level
/// @param[in] file The source code file name
/// @param[in] line The line number of the source code
/// @param[in] function The function name which generates this log message
/// @param[in] fmt The log message's format
void LOG(unsigned int level, const char *file, int line, const char *function, 
         const char *fmt, ...);

/// @}

#endif //NO_LOG

#endif
