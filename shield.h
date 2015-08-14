/// @file shield.h
/// @brief Shield I/O control module header
/// @author Kenny Huang

#include <bcm2835.h>

#ifndef __HEADER_SHIELD__
#define __HEADER_SHIELD__

//*****************************************************************************
/// @name Specify the maximum of pin number
//*****************************************************************************
/// @{
#define MAX_PIN_NUMBER 18
#define MAX_ANALOGPIN_NUMBER 8

/// @}

/// @name Shield register control pin mapping to Raspberry Pi
/// @{

#define GPIO_GEN0 GPIO17
#define GPIO_GEN1 GPIO18
#define GPIO_GEN2 GPIO27
#define GPIO_GEN3 GPIO22
#define GPIO_GEN4 GPIO23
#define GPIO_GEN5 GPIO24
#define GPIO_GEN6 GPIO25

/// @}

/// @name Shield register control pin mapping to bcm2835 c library
/// @sa http://www.airspayce.com/mikem/bcm2835/group__constants.html#ga63c029bd6500167152db4e57736d0939
/// @{

#define GPIO17 RPI_V2_GPIO_P1_11
#define GPIO18 RPI_V2_GPIO_P1_12
#define GPIO27 RPI_V2_GPIO_P1_13
#define GPIO22 RPI_V2_GPIO_P1_15
#define GPIO23 RPI_V2_GPIO_P1_16
#define GPIO24 RPI_V2_GPIO_P1_18
#define GPIO25 RPI_V2_GPIO_P1_22

/// @}

#define OUTPUT  BCM2835_GPIO_FSEL_OUTP ///< Output pin function mapping to bcm2835 c library
#define INPUT   BCM2835_GPIO_FSEL_INPT ///< Input pin function mapping to bcm2835 c library

/// @cond
#define TRUE  (1==1)
#define FALSE (0==1)
/// @endcond

/// @brief Digital output pin mapping
enum PinDigitalOut {
  /* Digital output register 1 */
  CN11_2 = 1, ///< 1
  CN11_4,     ///< 2
  CN12_2,     ///< 3
  CN12_4,     ///< 4
  CN13_2,     ///< 5
  CN13_4,     ///< 6
  CN14_2,     ///< 7
  CN14_4,     ///< 8
  CN15_2,     ///< 9
  CN15_4,     ///< 10
  CN16_2,     ///< 11
  CN16_4,     ///< 12
  CN17_2,     ///< 13
  CN17_4,     ///< 14
  CN18_2,     ///< 15
  CN18_4,     ///< 16
  /* Digital output register 2 */
  CN19_2,     ///< 17
  CN19_4,     ///< 18
  CN20_2,     ///< 19
  CN20_4 = 20 ///< 20
};

/// @brief Digital input pin mapping
enum PinDigitalIn {
  CN11_5 = 1, ///< 1
  CN12_5,     ///< 2
  CN13_5,     ///< 3
  CN14_5,     ///< 4
  CN15_5,     ///< 5
  CN16_5,     ///< 6
  CN17_5,     ///< 7
  CN18_5,     ///< 8
  CN19_5,     ///< 9
  CN20_5      ///< 10
};

/// @brief Digital port mode mapping
enum DigitalMode {
  DIO = 0, ///< 0, Digital I/O
  SPI,     ///< 1, Serial Peripheral Interface
  SIO      ///< 2, Serial I/O
};

/// @brief Shield control mode switch mapping
enum ShieldMode {
  INIT_ALL_LOW = 0x00, ///< 0x00, Set all controllable GPIO pin to output LOW
  NONE         = 0x70, ///< 0x70, Do nothing, no SPI signal between shield and Raspberry Pi at all
  INTERNAL_REG = 0x78, ///< 0x78, Change the shield internal register mode
  OUTPUT_REG_1 = 0x79, ///< 0x79, Configure digital output register 1 mode
  OUTPUT_REG_2 = 0x7A, ///< 0x7A, Configure digital output register 2 mode
  INPUT_REG    = 0x7B, ///< 0x79, Configure digital input register mode
  DAC          = 0x7E, ///< 0x7E, Digital-to-analog converter (Analog output) mode
  RTC          = 0x7D, ///< 0x7D, Real-time clock access mode
  ADC          = 0x7F  ///< 0x7F, Analog-to-digital converter (Analog input) mode
};

/// All LED indicator variables
typedef struct tIndicator {
  /// @{
  /// @name Indicator switches
  int ind_os;                 ///< OS indicator enable(1) / disabe(0) switch
  int ind_agent;              ///< AGENT indicator enable(1) / disabe(0) switch
  int ind_net;                ///< NET indicator enable(1) / disabe(0) switch 

  /// @}
  /// @{
  /// @name Indicator current status
  int ind_os_pin_status;      ///< OS indicator current output voltage status (0/1)
  int ind_agent_pin_status;   ///< AGENT indicator current output voltage status (0/1)
  int ind_net_pin_status;     ///< NET indicator current output voltage status (0/1)
    
  /// @}
  /// @{
  /// @name Indicator pin configuration
  char *ind_os_pin;           ///< OS indicator pin's parameter name string, ex. "shield.cn11.2"
  char *ind_agent_pin;        ///< AGENT indicator pin's parameter name string, ex. "shield.cn11.2"
  char *ind_net_pin;          ///< NET indicator pin's parameter name string, ex. "shield.cn11.2"

  /// @}
} tIndicator;

tIndicator inds;

/// @defgroup shiled Shield I/O control module module
/// @{

/// @brief Initialize SPI communication setting of shiled including bcm2835 c 
///        library
/// @return 1/@ref TRUE : On success
/// @return 0/@ref FALSE : on error
int ShieldLibInit();

/// @brief Close SPI communication setting of shiled and release bcm2835 c 
///        library resources
void ShieldLibClose();

/// @brief Set digital pin mode
/// @param[in] pin_number The digital I/O port number
/// @param[in] mode The target port mode [@ref DigitalMode]
/// @return 0 : On success
/// @return -1 : on error
int PinMode(unsigned int pin_number, int mode);

/// @brief Write a HIGH(1) or a LOW(0) value to a digital output pin
/// @param[in] pin_number The digital output pin number [@ref PinDigitalOut]
/// @param[in] value The output value(0/1)
/// @return 0 : On success
/// @return -1 : on error
int DigitalWrite(unsigned int pin_number, int value);

/// @brief Reads the value from a specified digital pin, either HIGH or LOW
/// @param[in] pin_number The digital input pin number [@ref PinDigitalIn]
/// @return 1/HIGH or 0/LOW
int DigitalRead(unsigned int pin_number);

#ifndef PURE_RPI_V2
/// @brief Set/Switch shield control mode
/// @param[in] mode The target shield mode [@ref ShieldMode]
void SetShieldMode(int mode);

/// @brief Get current shield control mode
/// @return The number of mode[@ref ShieldMode]
int GetShieldMode();

/// @brief Check whether shield can be switch mode sucessfully or not,
///        and the device will be rebooted 3 times maximum if it's not.
/// @param[in] mode The number of current shield mode [@ref ShieldMode]
void VerifyAndFixShieldMode(const int mode);
#endif // PURE_RPI_V2

/// @brief Set SPI clock speed
/// @param[in] speed The target of SPI [clock speed](http://www.airspayce.com/mikem/bcm2835/group__constants.html#gaf2e0ca069b8caef24602a02e8a00884e)
void SetSpiSpeed(const unsigned int speed);

/// @brief Set SPI delay seconds between two SPI access function, to give 
///        hardware time to write register
/// @param[in] usecs Microseconds delay time
void SetSpiDelay(const unsigned int usecs);

/// @brief Load SPI delay seconds from configuration
/// @return The microseconds of delay time
unsigned int LoadSpiDelay();

/// @brief Load SPI clock speed from configuration
/// @return The SPI [clock speed](http://www.airspayce.com/mikem/bcm2835/group__constants.html#gaf2e0ca069b8caef24602a02e8a00884e)
unsigned int LoadSpiSpeed();

#ifndef PURE_RPI_V2
/// @brief Read the ADC value from the specified analog port
/// @param[in] port_num The number (0~7) of the analog input port
/// @return ADC value (0 to 4095)
unsigned int AnalogRead(unsigned int port_num);

/// @brief Output volatage to Analog pin(#2)
/// @param[in] adc_addr Output group of port address 0:CN3~CN6, 1:CN7~CN10
/// @param[in] mv_value Output mV value (0~2230)
void AnalogWrite(unsigned int adc_addr, unsigned int mv_value);
#endif // PURE_RPI_V2

/// @brief Set digital pin mode by specified parameter name
/// @param[in] param The parameter name of digital port, ex. shield.cn11
/// @param[in] value The target port mode value [@ref DigitalMode]
int RpcChangeMode(const char *param, const char *value);

/// @brief Write a HIGH(1) or a LOW(0) value to a digital output pin by 
///        specified parameter name
/// @param[in] param The parameter name of digital output pin, ex. shield.cn11.2
/// @param[in] value The output value(0/1)
/// @return 0 : On success
/// @return -1 : on error
int RpcPinWrite(const char *param, char *value);

/// @brief Reads the value from a specified digital port, either HIGH or LOW 
///        by specified parameter name
/// @param[in] param The parameter name of digital port, ex. shield.cn11
/// @return 1/HIGH or 0/LOW
int RpcDigitalRead(const char *param);

/// @}

#endif //__HEADER_SHIELD__