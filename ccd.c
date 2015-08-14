/// @file ccd.c
/// @brief CCD barcode scanner module
/// @author Kenny Huang

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <limits.h>
#include <locale.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "act_file.h"
#include "ccd.h"
#include "database.h"
#include "http.h"
#include "ipc.h"
#include "log.h"
#include "main.h"
#include "controller.h"
#include "webcam.h"

#define THREAD_STACK_SIZE PTHREAD_STACK_MIN+1024*32
#define BARCODE_LENGTH 31

/// @name Barcode value variables
/// @{

char *barcode_vid; ///< USB scanner VID definition
char *barcode_pid; ///< USB scanner PID definitions
char barcode_value[EMPLOYEE_ID_LENGTH]; ///< The latest received barcode value
char barcode_id[EMPLOYEE_ID_LENGTH];    ///< Employee id

/// @}

/// @name Scanner control flag variables
/// @{

int position = 0;     ///< Position of a scanned barcode value array
int ccd_locker = 0;   ///< The scanner locker to prevent any input
int mode = CCD_NONE;  ///< CCD barcode scanner mode

/// @}

///HID keyboard map table
const char key_rep[48][2] = {
  "A", "B", "C", "D", //0x04 Keyboard a and A
  "E", "F", "G", "H",
  "I", "J", "K", "L",
  "M", "N", "O", "P",
  "Q", "R", "S", "T",
  "U", "V", "W", "X",
  "Y", "Z", "1", "2",
  "3", "4", "5", "6",
  "7", "8", "9", "0",
  "",  "",  "",  "", //0x28 Keyboard Return (ENTER), 0x29 Keyboard ESCAPE, 0x2A Keyboard DELETE (Backspace), 0x2B Keyboard Tab
  " ", "-", "+", "[",
  "]", "|", "~", ":"
};

//*****************************************************************************
/// @name Private functions
//*****************************************************************************
/// @{

/// @brief Handling barcode scanner USB connection and reading data.
void ccd_monitor();

/// @brief Translate HID keyboard code to ASCII
/// @param[in] ch HID raw keyboard code
/// @param[in] pos Position number of barcode value
void translate(char ch, int pos);

/// @}

//*****************************************************************************
// Translate HID keyboard input byte to ASCII
// Return barcode number sequence count
//*****************************************************************************
void
translate(char ch, int pos)
{
  switch (ch) {
    case 0x28: { //The end of barcode readings, 0x28 Keyboard Return (ENTER)
      barcode_value[pos] = '\0';
      
      if (CCD_NONE != mode) {
        CcdLock();
        LOG(LOG_DEBUG, "Barcode (ID): %s", barcode_value);
        strcpy (barcode_id, barcode_value);
        ControllerIpcSend(NULL, "{\"cmd\":\"id\"}");
        if (CCD_TEST == mode)
          CcdUnLock();
      }
      CcdReset();
      break;
    }
    default: {
      barcode_value[pos] = key_rep[ch - 4][0]; //Map start from 0x04(A), need to minus 4
      break;
    }
  }
}

void
ccd_monitor()
{
  int  fd;
  int  bc_fd;
  char dev_path[16] = {0};
  struct udev_device *dev;
  struct udev_monitor *mon;
  struct udev *udev;
  struct udev_enumerate *enumerate;
  struct udev_list_entry *devices, *dev_list_entry;
  struct udev_device *parent_dev;
  udev = udev_new();

  if (!udev) {
    LOG(LOG_ERROR, "Can't create udev");
    exit(1);
  }

RELOAD_DEVICE:
  // Set up a monitor to monitor hidraw devices
  mon = udev_monitor_new_from_netlink(udev, "udev");
  udev_monitor_filter_add_match_subsystem_devtype(mon, "hidraw", NULL);
  udev_monitor_enable_receiving(mon);
  fd = udev_monitor_get_fd(mon);

  while (strcmp(dev_path, "\0") == 0) {
    // Create a list of the devices in the 'hidraw' subsystem
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "hidraw");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(dev_list_entry, devices) {
      const char *path;
      path = udev_list_entry_get_name(dev_list_entry);
      parent_dev = udev_device_new_from_syspath(udev, path);
      dev = udev_device_new_from_syspath(udev, path);
      dev = udev_device_get_parent_with_subsystem_devtype(
              dev,
              "usb",
              "usb_device");

      // Use this udev path if VID/PID contains in the parameter value
      if (strcasestr(barcode_vid, udev_device_get_sysattr_value(dev, "idVendor")) &&
          strcasestr(barcode_pid, udev_device_get_sysattr_value(dev, "idProduct"))) {
        LOG(LOG_DEBUG, "CCD device path: %s", udev_device_get_devnode(parent_dev));
        sprintf(dev_path, "%s", udev_device_get_devnode(parent_dev));
      } else {
        CmdSendInUdp( "{\"name\":\"error\",\"value\":\"-32125\"}");
      }

      udev_device_unref(dev);
      udev_device_unref(parent_dev);
    }
    udev_enumerate_unref(enumerate);
    sleep(2);
  }

  bc_fd = open(dev_path, O_RDONLY); //| O_NONBLOCK);
  char buffer[8];
  memset(buffer, 0, sizeof(buffer));


  while (1) {
    fd_set fds;
    fd_set bc_fds;
    struct timeval tv;
    int bc_ret;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    FD_ZERO(&bc_fds);
    FD_SET(bc_fd, &bc_fds);
    int max = fd > bc_fd ? fd : bc_fd;
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    select(max + 1, &bc_fds, NULL, NULL, &tv);

    // Check if barcode fd receives data
    if (FD_ISSET(bc_fd, &bc_fds)) {
      bc_ret = read(bc_fd, buffer, 4);

      if (1 == ccd_locker) {
        position = 0;
        memset(buffer, 0, sizeof(buffer));
        continue;
      }

      if (bc_ret < 0 || buffer[2] < 0x04) {
        //do nothing
      } else {
        //Translate the second byte to ASCII char only, ignore others
        translate(buffer[2], position); 
        ++ position;

        if (position > BARCODE_LENGTH) { //Check overflow
          LOG(LOG_ERROR, "Barcode value is too long");
          memset(buffer, 0, sizeof(buffer));
          position = 0;
        }
      }
    }

    // Check if USB fd receives data
    if (FD_ISSET(fd, &fds)) {
      dev = udev_monitor_receive_device(mon);

      if (dev) {
        LOG(LOG_NOTICE, "USB message: %s", udev_device_get_action(dev));

        if (0 == strcasecmp("add", udev_device_get_action(dev)) || //Device is connected to the system
            0 == strcasecmp("remove", udev_device_get_action(dev)) ||
            0 == strcasecmp("change", udev_device_get_action(dev))) { //Something about the device changed
          close(bc_fd);
          close(fd);
          goto RELOAD_DEVICE;
        }

        udev_device_unref(dev);
      }
    }
  }

  udev_unref(udev);
  return;
}

void
StartCcdMonitor()
{
  pthread_t id = pthread_self();
  pthread_attr_t attr;
  struct sched_param params;
  int ret;
  int thread_stacksize;
  LoadBarcodeConfig();
  //NOTE: Setting thread stack size, Linux default will be around 8MB (ulimit -s)
  thread_stacksize = THREAD_STACK_SIZE;
  params.sched_priority = sched_get_priority_max(SCHED_FIFO);
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, thread_stacksize);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  ret = pthread_setschedparam(id, SCHED_FIFO, &params);

  if (ret != 0) {
    LOG(LOG_ERROR, "Fail to set thread realtime priority");
  }

  ret = pthread_create(&id, &attr, (void *) ccd_monitor, NULL);
  pthread_attr_destroy(&attr);

  if (ret != 0) {
    LOG(LOG_ERROR, "CCD monitor thread start failed: %s", strerror(errno));
  }
}

void
LoadBarcodeConfig()
{
  barcode_pid = GetParamString("hr.barcode.pid");
  barcode_vid = GetParamString("hr.barcode.vid");
}

void
CcdReset()
{
  barcode_value[0] = '\0';
  position = -1;
}

void
SetCcdMode(unsigned int set_mode)
{
  mode = set_mode;
}

int
GetCcdMode()
{
 return mode;
}

void
CcdLock()
{
  LOG(LOG_NOTICE, "Lock CCD");
  ccd_locker = 1;
}

void
CcdUnLock()
{
  LOG(LOG_NOTICE, "Unlock CCD");
  ccd_locker = 0;
}
