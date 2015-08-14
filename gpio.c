/// @file gpio.c
/// @brief GPIO control module
/// @author Kenny Huang

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ccd.h"
#include "database.h"
#include "device.h"
#include "log.h"

#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define MAX_BUF 64

/// @brief Export creates a new folder for the exported pin, and creates files 
///        for each of its control functions.
/// @param[in] gpio GPIO numbers should be from this list:
///        2, 3, 4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 22, 23, 24, 25, 27
/// @return 0: On success
/// @return -1: On error
int gpio_export(unsigned int gpio);

/// @brief Clears the exported folders and files for the exported pin
/// @param[in] gpio GPIO numbers should be from this list:
///        2, 3, 4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 22, 23, 24, 25, 27
/// @return 0: On success
/// @return -1: On error
int gpio_unexport(unsigned int gpio);

/// @brief Set GPIO input/output direction
/// @param[in] gpio GPIO numbers should be from this list:
///        2, 3, 4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 22, 23, 24, 25, 27
/// @param[in] direction Pin mode: 1=INPUT,2=OUTPUT
/// @return 0: On success
/// @return -1: On error
int gpio_set_dir(unsigned int gpio, unsigned int direction);

/// @brief Set GPIO output value
/// @param[in] gpio GPIO numbers should be from this list:
///        2, 3, 4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 22, 23, 24, 25, 27
/// @param[in] value Digital output value: 0=LOW, 1=HIGH
int gpio_set_value(unsigned int gpio, unsigned int value);

/// @brief Get GPIO input value
/// @param[in] gpio GPIO numbers should be from this list:
///        2, 3, 4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 22, 23, 24, 25, 27
/// @return The input (0/1) of the GPIO pin
int gpio_get_value(unsigned int gpio);

/// @brief Enables or disables the given pin for edge interrupt triggering 
///        on the rising, falling or both edges.
/// @param[in] gpio GPIO numbers should be from this list:
///        2, 3, 4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 22, 23, 24, 25, 27
/// @param[in] edge The edge setting should be from this list:
///            rising, falling, both, none
/// @return 0: On success
/// @return -1: On error
int gpio_set_edge(unsigned int gpio, char *edge);

/// @brief Open a GPIO value mapping file for operation
/// @param[in] gpio GPIO numbers should be from this list:
///        2, 3, 4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 22, 23, 24, 25, 27
/// @return The GPIO value file descriptor
int gpio_fd_open(unsigned int gpio);

/// @brief Close the given GPIO value mapping file descriptor
/// @return 0: On success
/// @return -1: On error
int gpio_fd_close(int fd);

int
gpio_export(unsigned int gpio)
{
  int fd, len;
  char buf[MAX_BUF];
 
  fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
  if (fd < 0) {
    LOG(LOG_ERROR,"GPIO export failure: %s", strerror(errno));
    return fd;
  }
 
  len = snprintf(buf, sizeof(buf), "%d", gpio);
  write(fd, buf, len);
  close(fd);
 
  return 0;
}

int
gpio_unexport(unsigned int gpio)
{
  int fd, len;
  char buf[MAX_BUF];
 
  fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
  if (fd < 0) {
    LOG(LOG_ERROR,"GPIO unexport failure: %s", strerror(errno));
    return fd;
  }
 
  len = snprintf(buf, sizeof(buf), "%d", gpio);
  write(fd, buf, len);
  close(fd);
  return 0;
}

int
gpio_set_dir(unsigned int gpio, unsigned int direction)
{
  int fd;
  char buf[MAX_BUF];
 
  snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR  "/gpio%d/direction", gpio);
 
  fd = open(buf, O_WRONLY);
  if (fd < 0) {
    LOG(LOG_ERROR,"GPIO set direction failure: %s", strerror(errno));
    return fd;
  }

  if (1 == direction)
    write(fd, "in", 3);
  else
    write(fd, "out", 4);

  close(fd);
  return 0;
}

int
gpio_set_value(unsigned int gpio, unsigned int value)
{
  int fd;
  char buf[MAX_BUF];
 
  snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
 
  fd = open(buf, O_WRONLY);
  if (fd < 0) {
    LOG(LOG_ERROR,"GPIO set value failure: %s", strerror(errno));
    return fd;
  }
 
  if (1 == value)
    write(fd, "1", 2);
  else
    write(fd, "0", 2);
 
  close(fd);
  return 0;
}

int
gpio_get_value(unsigned int gpio)
{
  int fd;
  char buf[MAX_BUF];
  char ch;

  snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
 
  fd = open(buf, O_RDONLY);
  if (fd < 0) {
    LOG(LOG_ERROR,"GPIO get value failure: %s", strerror(errno));
    return fd;
  }
 
  read(fd, &ch, 1);
  close(fd);
  
  if (ch != '0') {
    return 1;
  } else {
    return 0;
  }
}

int
gpio_set_edge(unsigned int gpio, char *edge)
{
  int fd;
  char buf[MAX_BUF];

  snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/edge", gpio);
 
  fd = open(buf, O_WRONLY);
  if (fd < 0) {
    LOG(LOG_ERROR,"GPIO set edge failure: %s", strerror(errno));
    return fd;
  }
 
  write(fd, edge, strlen(edge) + 1); 
  close(fd);
  return 0;
}

int
gpio_fd_open(unsigned int gpio)
{
  int fd;
  char buf[MAX_BUF];

  snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
 
  fd = open(buf, O_RDONLY );
  if (fd < 0) {
    LOG(LOG_ERROR,"GPIO fd open failure: %s", strerror(errno));
  }
  return fd;
}

int
gpio_fd_close(int fd)
{
  return close(fd);
}

int gpio_input_trap_proc()
{
  int shutdown_fd = -1;
  int gpio_shutdown;
  char *buf[MAX_BUF];
  fd_set exceptfds;
  int res;
  
  gpio_shutdown = GetGpioNumberByFuncName("shutdownSwitch");
  
  if (-1 != gpio_shutdown) {
    LOG(LOG_NOTICE, "set shutdownSwitch on GPIO %d", gpio_shutdown);
    gpio_export(gpio_shutdown);
    gpio_set_dir(gpio_shutdown, 2);
    gpio_set_value(gpio_shutdown, 1);
    gpio_set_edge(gpio_shutdown, "both");
    //shutdown_fd = gpio_fd_open(gpio_shutdown);
  } else {
    LOG(LOG_NOTICE, "No set shutdownSwitch on GPIO pin");
    return 0;
  }
  
  // Trap an output GPIO is restric by GPIO driver in Linux kernel
  //   https://github.com/torvalds/linux/commit/d468bf9ecaabd3bf3a6134e5a369ced82b1d1ca1
  // /var/log/kern.log shows:
  //    GPIO chip pinctrl-bcm2835: gpio_lock_as_irq: tried to flag a GPIO set as output for IRQ
  //    gpio-18 (sysfs): failed to flag the GPIO for IRQ
  // But with dummy read() it still works, however if we use poll() to trap POLLPRI 
  // system could freeze (could be kernel bugs)
  while (1) {
    shutdown_fd = gpio_fd_open(gpio_shutdown);

    FD_ZERO(&exceptfds);                                                                        
    FD_SET(shutdown_fd, &exceptfds); 
    read(shutdown_fd, buf, MAX_BUF); //dummy read(); any newly opened file is considered changed.
    
    res = select(shutdown_fd+1,
               NULL,               // readfds - not needed
               NULL,               // writefds - not needed                                        
               &exceptfds,
               NULL);              // timeout (never)     
    if (res > 0 && FD_ISSET(shutdown_fd, &exceptfds)) {
      if (gpio_get_value(gpio_shutdown) == 0) {
        LOG(LOG_NOTICE, "get shutdown signal from GPIO %d", gpio_shutdown);
        Shutdown();
      }
    }
    gpio_fd_close(shutdown_fd);
    read(shutdown_fd, buf, MAX_BUF);
  }
  
  return 0;
}

void GpioInputTrapStart()
{
  pthread_t id;
  int ret;
  //Setting thread stack size, Linux default will be around 8MB (ulimit -s)
  int thread_stacksize = PTHREAD_STACK_MIN + 1024 * 16;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, thread_stacksize);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  ret = pthread_create(&id, &attr, (void *) gpio_input_trap_proc, 
                       NULL);

  if (ret != 0) {
    LOG(LOG_ERROR, "GPIO input trap start failed: %s", strerror(errno));
  }

  pthread_attr_destroy(&attr);
}