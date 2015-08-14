/// @file gpio.h
/// @brief GPIO control module
/// @author Kenny Huang

/// @brief Start a GPIO input trapper thread to monitor the value change by 
///        interrupts. The monitored files are in path: /sys/class/gpio/
void GpioInputTrapStart();