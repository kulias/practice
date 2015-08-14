/// @file log.c
/// @brief Log handler module
/// @author Kenny Huang

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef NO_LOG

#include "log.h"
#include "act_file.h"
#include "act_string.h"

#define DEFAULT_LOG_LIMIT 1 * 1024 *1024
#define LOG_SIZE          4096
#define FILE_PATH_LEN     128

enum {
  TO_FILE,
  TO_SCREEN,
  TO_BOTH,
  TO_NONE
};

/// @brief Log configuration struct
static struct LogConfig {
  FILE *fp;
  char fn[FILE_PATH_LEN]; ///< Log file full path
  unsigned char rotate;   ///< Auto rotate flag
  unsigned char backup;   ///< How many backup files should be kept
  unsigned char level;    ///< Log lower levels from this value
  unsigned long limit;    ///< The limitation of log file size
  unsigned long cur;      ///< The size of the current log file
  int mode;               ///< Log mode
} LogConfig = {
  NULL,
  "/home/pi/.act/actclient.log",
  BOOLEAN_FALSE,
  1, /* Backup 5 files at most */
  __LOG_DEBUG,
  TO_BOTH,
  DEFAULT_LOG_LIMIT,
  0
};


struct {
  char color[32];
  char no_color[16];
} log_descriptions[] = {

  {"\033[1;32mDEBUG", "DEBUG"},
  {"\033[1;34mNOTICE", "NOTICE"},
  {"\033[1;33mWARNING", "WARNING"},
  {"\033[1;35mERROR", "ERROR"},
};


int SetLogConfig(const char *name, const char *value)
{
  int res = 0;

  if (strcasecmp(name, "LogMode") == 0) {
    if (strcasecmp(value, "SCREEN") == 0) {
      LogConfig.mode = TO_SCREEN;
    } else if (strcasecmp(value, "FILE") == 0) {
      LogConfig.mode = TO_FILE;
    } else if (strcasecmp(value, "NONE") == 0) {
      LogConfig.mode = TO_NONE;
    } else if (strcasecmp(value, "BOTH") == 0) {
      LogConfig.mode = TO_BOTH;
    } else {
      LogConfig.mode = TO_BOTH;
      res = -1;
    }
  } else if (strcasecmp(name, "LogLimit") == 0) {
    res = atoi(value);

    if (res <= 0) {
      LOG(LOG_WARNING, "Invalid file size limit: %s", value);
    } else {
      const char *unit;
      LogConfig.limit = res;
      unit = value;

      while (*unit && (isdigit(*unit) || *unit < 33)) {
        unit++;
      }

      if (*unit) {
        if (strcasecmp(unit, "kb") == 0 || strcasecmp(unit, "k") == 0) {
          LogConfig.limit = res * 1024;
        } else if (strcasecmp(unit, "mb") == 0 || strcasecmp(unit, "m") == 0) {
          LogConfig.limit = res * 1024 * 1024;
        } else if (strcasecmp(unit, "gb") == 0 || strcasecmp(unit, "g") == 0) {
          LogConfig.limit = res * 1024 * 1024 * 1024;
        } else {
          LOG(LOG_WARNING, "Invalid file size limit unit: %s", unit);
          res = -1;
        }
      }

      if (LogConfig.limit <  DEFAULT_LOG_LIMIT) {
        /* The smallest limit for log file */
        LOG(LOG_NOTICE, "The file size limit is too small, set to default value: %d", DEFAULT_LOG_LIMIT);
        LogConfig.limit = DEFAULT_LOG_LIMIT;
        res = -1;
      }
    }
  } else if (strcasecmp(name, "LogBackup") == 0) {
    LogConfig.backup = atoi(value);

    if (LogConfig.backup < 1) {
      LOG(LOG_WARNING, "Invalid log backup number: %d", LogConfig.backup);
      LogConfig.backup = 1;
      res = -1;
    }
  } else if (strcasecmp(name, "LogLevel") == 0) {
    if (strcasecmp(value, "NOTICE") == 0) {
      LogConfig.level = __LOG_NOTICE;
    } else if (strcasecmp(value, "WARNING") == 0) {
      LogConfig.level = __LOG_WARNING;
    } else if (strcasecmp(value, "ERROR") == 0) {
      LogConfig.level = __LOG_ERROR;
    } else if (strcasecmp(value, "DEBUG") == 0) {
      LogConfig.level = __LOG_DEBUG;
    } else {
      LogConfig.level = __LOG_DEBUG;
      res = -1;
    }
  } else if (strcasecmp(name, "LogFileName") == 0) {
    snprintf(LogConfig.fn, sizeof(LogConfig.fn), "%s", value);
  } else if (strcasecmp(name, "LogAutoRotate") == 0) {
    LogConfig.rotate = StringToBoolean(value);
  } else {
  }

  return res;
}

int StartLogger()
{
  int res = 0;

  if (LogConfig.mode == TO_FILE || LogConfig.mode == TO_BOTH) {
    LogConfig.fp = fopen(LogConfig.fn, "a");

    if (LogConfig.fp == NULL) {
      LOG(LOG_WARNING, "Open log file failed!");
      LogConfig.mode = TO_SCREEN;
    } else {
      chmod(LogConfig.fn, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH); //chmod 666
      LogConfig.cur = ftell(LogConfig.fp);
    }
  }

  return res;
}

void LOG(unsigned int level, const char *file, int line, const char *function, 
         const char *fmt, ...)
{
  static char *buffer = NULL;
  static const int buf_size = LOG_SIZE;

  if (level >= LogConfig.level && LogConfig.mode != TO_NONE) {
    int rc;
    va_list ap;
    char date[64];
    time_t t;
    struct tm *tm;

    if (buffer == NULL) {
      buffer = malloc(buf_size);
    }

    if (buffer == NULL) {
      return;
    }

    t = time(NULL);
    tm = localtime(&t);
    strftime(date, sizeof(date) - 1, "%b %e %T", tm);
    va_start(ap, fmt);
    vsnprintf(buffer, buf_size, fmt, ap);
    va_end(ap);

    if (LogConfig.mode != TO_FILE) {
#ifdef VT100_COMPAT
#define FMT_COLOR "\033[37;40m[%s] %s \033[37m%s()@%s:%d => \033[0;37;40m%s\n\033[0m"
#endif
#define FMT_NO_COLOR "[%s] %s %s()@%s:%d => %s\n"
#ifdef VT100_COMPAT
      fprintf(stderr, FMT_COLOR, date, log_descriptions[level].color, function, 
              file, line, buffer);
#else
      fprintf(stderr, FMT_NO_COLOR, date, log_descriptions[level].no_color, 
              function, file, line, buffer);
#endif
      fflush(stderr);
    }

    if (LogConfig.fp) {
      rc = fprintf(LogConfig.fp, FMT_NO_COLOR, date, 
                   log_descriptions[level].no_color, 
                   function, file, line, buffer);

      if (rc > 0) {
        fflush(LogConfig.fp);
        LogConfig.cur += rc;

        if (LogConfig.cur >= LogConfig.limit) {
          fclose(LogConfig.fp);
          LogConfig.fp = NULL;

          if (LogConfig.rotate == 1) {
            unsigned int i;
            int j;
            char new_name[FILE_PATH_LEN + 1];
            char old_name[FILE_PATH_LEN + 1];

            for (i = 1; i < LogConfig.backup; i++) {
              FILE *tmp;
              snprintf(new_name, sizeof(new_name), "%s.%d", LogConfig.fn, i);

              if ((tmp = FileOpen(new_name, "r")) == NULL) {
                if (errno == ENOENT) {
                  i += 1;
                  break;
                } else {
                  LogConfig.mode = TO_SCREEN;
                  return;
                }
              } else {
                fclose(tmp);
              }
            }

            for (j = i - 2; j > 0; j--) {
              snprintf(old_name, sizeof(old_name), "%s.%d", 
                       LogConfig.fn, j);
              snprintf(new_name, sizeof(new_name), "%s.%d", 
                       LogConfig.fn, j + 1);
              FileRename(old_name, new_name);
            }

            snprintf(new_name, sizeof(new_name), "%s.1", LogConfig.fn);
            FileRename(LogConfig.fn, new_name);
          } else {
            remove(LogConfig.fn);
          }

          StartLogger();
        }
      }
    }
  }
}

#endif //NO_LOG
