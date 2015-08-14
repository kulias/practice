/// @file shield.c
/// @brief Shield I/O control module
/// @author Kenny Huang

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "database.h"
#include "log.h"
#include "shield.h"

#define MAX_SAMPLING_RETRY_TIME 3
#define MAX_REBOOT_RETRY_TIME   3

#define getBit(x,n) ((x >> (n-1)) & 1)

/// @cond

// ADC chip registers
uint8_t internalRegister[] = {0xFF, 0xFF};
uint8_t outputRegister1[] = {0x00, 0x00};
uint8_t outputRegister2[] = {0x00, 0x00};
uint8_t adcConversion[] = {0x86, 0x8E, 0x96, 0x9E, 0xA6, 0xAE, 0xB6, 0xBE};

unsigned int spi_delay_usecs = 0;
unsigned int gpio_delay_usecs = 500;
unsigned int spi_speed = 0;

// Map to binary system
// Usage: "%s%s", bit_rep[byte >> 4], bit_rep[byte & 0x0F]
const char bit_rep[16][5] = {
  "0000", "0001", "0010", "0011",
  "0100", "0101", "0110", "0111",
  "1000", "1001", "1010", "1011",
  "1100", "1101", "1110", "1111",
};

// Reverse from enum to string for log
const char digi_mode_rep[3][4] = {
  "DIO", "SPI", "SIO"
};

const char digi_port_rep[10][5] = {
  "CN11", "CN12", "CN13", "CN14", "CN15",
  "CN16", "CN17", "CN18", "CN19", "CN20"
};

const char digi_out_rep[20][7] = {
  "CN11_2", "CN11_4", "CN12_2", "CN12_4", "CN13_2",
  "CN13_4", "CN14_2", "CN14_4", "CN15_2", "CN15_4",
  "CN16_2", "CN16_4", "CN17_2", "CN17_4", "CN18_2",
  "CN18_4", "CN19_2", "CN19_4", "CN20_2", "CN20_4",
};
/// @endcond

int PinMode(unsigned int pin_number, int mode)
{
#ifndef PURE_RPI_V2
  //CN11~CN20 = pin_number:1~10
  if (pin_number > 10 || pin_number < 1) {
    LOG(LOG_ERROR, "Pin number shall in range 1 to 10, current: %d", 
        pin_number);
    return -1;
  }

  /*  Internal Register of shield
      -----internalRegister[0]------
      Bit15   x
      ...     ...
      Bit10   x
      Bit09   1:CN20 SIO  0:CN20 DIO
      Bit08   x           0:CN19 DIO
      -----internalRegister[1]------
      Bit07   1:CN18 SPI  0:CN18 DIO
      Bit06   1:CN17 SPI  0:CN17 DIO
      Bit05   1:CN16 SPI  0:CN16 DIO
      Bit04   1:CN15 SPI  0:CN15 DIO
      Bit03   1:CN14 SPI  0:CN14 DIO
      Bit02   1:CN13 SPI  0:CN13 DIO
      Bit01   1:CN12 SPI  0:CN12 DIO
      Bit00   1:CN11 SPI  0:CN11 DIO
   ***********************************/
  if (mode == DIO) {
    if (pin_number < 9) {
      internalRegister[1] &= ~(1 << (pin_number - 1)); //Set bit to 0
    } else {
      internalRegister[0] &= ~(1 << (pin_number - 9)); //Set bit to 0
    }
  } else if (mode == SPI) {
    if (pin_number < 9) {
      internalRegister[1] |= 1 << (pin_number - 1); //Set bit to 1
    } else {
      LOG(LOG_ERROR, "Cannot use SPI on port %d", pin_number);
      SetShieldMode(NONE);
      VerifyAndFixShieldMode(NONE);
      return -1;
    }
  } else if (mode == SIO) {
    if (pin_number == 10) {
      internalRegister[0] |= 1 << (pin_number - 1); //Set bit to 1
    } else {
      LOG(LOG_ERROR, "Cannot use SIO on port %d", pin_number);
      SetShieldMode(NONE);
      VerifyAndFixShieldMode(NONE);
      return -1; //Current only CN20 works
    }
  }

  //Enter internal register setting mode
  SetShieldMode(INTERNAL_REG);
  bcm2835_spi_writenb((char *)internalRegister, 2);
  LOG(LOG_DEBUG, "set port[%s] mode[%s]: %s%s %s%s", 
      digi_port_rep[pin_number - 1], digi_mode_rep[mode],
      bit_rep[internalRegister[0] >> 4], bit_rep[internalRegister[0] & 0x0F],
      bit_rep[internalRegister[1] >> 4], bit_rep[internalRegister[1] & 0x0F]);
#else // Using GPIO without I/O shield
  if (BCM2835_GPIO_FSEL_INPT == mode) {
    bcm2835_gpio_fsel(pin_number, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(pin_number, BCM2835_GPIO_PUD_UP);
  } else if (BCM2835_GPIO_FSEL_OUTP == mode) {
    bcm2835_gpio_fsel(pin_number, BCM2835_GPIO_FSEL_OUTP);
  } else {
    return -1;
  }
#endif // PURE_RPI_V2
  usleep(spi_delay_usecs);
  return 0;
}

int DigitalWrite(unsigned int pin_number, int value)
{
#ifndef PURE_RPI_V2
  /*  Digital Ouput Register 1
              HIGH      LOW
      -----outputRegister1[0]------
      Bit15   1:CN18_4  0:CN18_4
      Bit14   1:CN18_2  0:CN18_2
      Bit13   1:CN17_4  0:CN17_4
      Bit12   1:CN17_2  0:CN17_2
      Bit11   1:CN16_4  0:CN16_4
      Bit10   1:CN16_2  0:CN16_2
      Bit09   1:CN15_4  0:CN15_4
      Bit08   1:CN15_2  0:CN15_2
      -----outputRegister1[1]------
      Bit07   1:CN14_4  0:CN14_4
      Bit06   1:CN14_2  0:CN14_2
      Bit05   1:CN13_4  0:CN13_4
      Bit04   1:CN13_2  0:CN13_2
      Bit03   1:CN12_4  0:CN12_4
      Bit02   1:CN12_2  0:CN12_2
      Bit01   1:CN11_4  0:CN11_4
      Bit00   1:CN11_2  0:CN11_2
   ***********************************/

  /*  Digital Ouput Register 2
             HIGH      LOW
     -----outputRegister2[0]------
     Bit15   x
     ...     ...
     Bit08   x
     -----outputRegister2[1]------
     Bit07   x
     Bit06   x
     Bit05   x
     Bit04   x
     Bit03   1:CN20_4  0:CN20_4
     Bit02   1:CN20_2  0:CN20_2
     Bit01   1:CN19_4  0:CN19_4
     Bit00   1:CN19_2  0:CN19_2
  ***********************************/
  if (pin_number <= 8) { //CN11_2~CN14_4
    if (value == HIGH) {
      outputRegister1[1] |= 1 << (pin_number - 1); //Set bit to 1
    } else {
      outputRegister1[1] &= ~(1 << (pin_number - 1)); //Set bit to 0
    }

    /* Enter ouput register 1(GPIO_GEN6..0 = 111 1001) */
    SetShieldMode(OUTPUT_REG_1);
    bcm2835_spi_writenb((char *)outputRegister1, 2);
    usleep(spi_delay_usecs);
    LOG(LOG_DEBUG, "set pin[%s] outputRegister1: %s%s %s%s", 
        digi_out_rep[pin_number - 1],
        bit_rep[outputRegister1[0] >> 4], bit_rep[outputRegister1[0] & 0x0F],
        bit_rep[outputRegister1[1] >> 4], bit_rep[outputRegister1[1] & 0x0F]);
    SetShieldMode(NONE);
    VerifyAndFixShieldMode(NONE);
    return 0;
  } else if (pin_number >= 9 && pin_number <= 16) { //CN15_2~CN18_4
    if (value == HIGH) {
      outputRegister1[0] |= 1 << (pin_number - 9); //Set bit to 1
    } else {
      outputRegister1[0] &= ~(1 << (pin_number - 9)); //Set bit to 0
    }

    /* Enter ouput register 1(GPIO_GEN6..0 = 111 1001) */
    SetShieldMode(OUTPUT_REG_1);
    bcm2835_spi_writenb((char *)outputRegister1, 2);
    usleep(spi_delay_usecs);
    LOG(LOG_DEBUG, "set pin[%s] outputRegister1: %s%s %s%s", 
        digi_out_rep[pin_number - 1],
        bit_rep[outputRegister1[0] >> 4], bit_rep[outputRegister1[0] & 0x0F],
        bit_rep[outputRegister1[1] >> 4], bit_rep[outputRegister1[1] & 0x0F]);
    SetShieldMode(NONE);
    VerifyAndFixShieldMode(NONE);
    return 0;
  } else if (pin_number >= 17 && pin_number <= 20) { ////CN19_2~CN20_4
    if (value == HIGH) {
      outputRegister2[1] |= 1 << (pin_number - 17); //Set bit to 1
    } else {
      outputRegister2[1] &= ~(1 << (pin_number - 17)); //Set bit to 0
    }

    /* Enter ouput register 2(GPIO_GEN6..0 = 111 1010) */
    SetShieldMode(OUTPUT_REG_2);
    bcm2835_spi_writenb((char *)outputRegister2, 2);
    usleep(spi_delay_usecs);
    LOG(LOG_DEBUG, "set pin[%s] outputRegister2: %s%s %s%s", 
        digi_out_rep[pin_number - 1],
        bit_rep[outputRegister2[0] >> 4], bit_rep[outputRegister2[0] & 0x0F],
        bit_rep[outputRegister2[1] >> 4], bit_rep[outputRegister2[1] & 0x0F]);
    SetShieldMode(NONE);
    VerifyAndFixShieldMode(NONE);
    return 0;
  } else {
    SetShieldMode(NONE);
    VerifyAndFixShieldMode(NONE);
    LOG(LOG_ERROR, "Pin number shall in range 1 to 20, current: %d", 
        pin_number);
    return -1;
  }

  SetShieldMode(NONE);
  VerifyAndFixShieldMode(NONE);
  return 0;
#else // Using GPIO without I/O shield
  bcm2835_gpio_fsel(pin_number, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_write(pin_number, value);
  usleep(spi_delay_usecs);
  return 0;
#endif // PURE_RPI_V2
}

int DigitalRead(unsigned int pin_number)
{
#ifndef PURE_RPI_V2
  /***** Digital Input Register ******
  -----inputRegister[1]------
      Bit15   x
      Bit14   x
      Bit13   x
      Bit12   x
      Bit11   x
      Bit10   x
      Bit09   CN20
      Bit08   CN19
  -----inputRegister[0]------
      Bit07   CN18
      Bit06   CN17
      Bit05   CN16
      Bit04   CN15
      Bit03   CN14
      Bit02   CN13
      Bit01   CN12
      Bit00   CN11
  ***********************************/
  uint8_t inputRegister[] = {0x00, 0x00};
  SetShieldMode(INPUT_REG);
  bcm2835_spi_transfern((char *)inputRegister, 2);
  //LOG(LOG_ERROR, "inputRegister: 0x%02X 0x%02X", inputRegister[0], inputRegister[1] );
  usleep(spi_delay_usecs);
  SetShieldMode(NONE);
  VerifyAndFixShieldMode(NONE);

  if (pin_number <= 8) {
    return getBit(inputRegister[1], pin_number);
  }

  if (pin_number <= 10) {
    return getBit(inputRegister[0], pin_number - 8);
  }
#else // Using GPIO wihtout I/O shield
  bcm2835_gpio_fsel(pin_number, BCM2835_GPIO_FSEL_INPT);
  bcm2835_gpio_set_pud(pin_number, BCM2835_GPIO_PUD_UP);
  return (int)bcm2835_gpio_lev(pin_number);
#endif //PURE_RPI_V2
  return -1;
}

int
RpcDigitalRead(const char *param)
{
  char port_char[2];
  int port_num;
  int res;
  // Get string "shield.cn12" -to-> "2" = port_char[0]
  strncpy(port_char, param + strlen("shield.cnx"), 1); 
  port_char[1] = '\0';
  port_num = atoi(port_char);

  if (0 == port_num) {
    port_num = 10;
  }

  // Enable DIO mode on the port
  if (-1 == PinMode(port_num, DIO)) {
    return -1;
  }

  res = DigitalRead(port_num);
  return res;
}


int
RpcChangeMode(const char *param, const char *value)
{
  char port_str[3];
  int port_num;
  int mode = SPI; //Default mode is DIO
  int res;
  strncpy(port_str, param + strlen("shield.cn"), 2);
  port_str[2] = '\0';
  port_num = atoi(port_str) - 10; //CN11~CN20 = port_num:1~10

  if (0 == strncmp("dio", value, 1)) {
    mode = DIO;
  } else if (0 == strncmp("spi", value, 2)) {
    mode = SPI;
  } else if (0 == strncmp("sio", value, 2)) {
    mode = SIO;
  }

  res = PinMode(port_num, mode);
  //LOG(LOG_ERROR,"%s %s --> %d %d", param, value, port_num, mode);
  return res;
}

#define ADC_MAXIMUM 4095
unsigned int
AnalogRead(unsigned int port_num)
{
  uint8_t rbuf[] = {0 , 0}; //buffer for receiving data
  int result;

  if (port_num > 7) {
    LOG(LOG_ERROR, "Analog port assignment is out of range");
    return 0;
  }

  SetShieldMode(ADC);
  bcm2835_spi_transfer(adcConversion[port_num]);
  usleep(spi_delay_usecs);
  bcm2835_spi_transfern((char *)rbuf, 2);
  usleep(spi_delay_usecs);
  result = (rbuf[0] << 8) + rbuf[1];

  if (result > ADC_MAXIMUM) { //Value is over 12bit can hold
    LOG(LOG_WARNING, "Analog %d/CN%d: %4d (Impossible value, masked to 12bit)", 
        port_num + 1, port_num + 3, result);
    result = ((rbuf[0] & 0x0F) << 8) + rbuf[1];
    return result;
  }

  LOG(LOG_DEBUG, "Analog %d/CN%d: %02X %02X->%4d %4dmV", port_num + 1, 
      port_num + 3, rbuf[0], rbuf[1], result, result * 2500 / 4096);
  SetShieldMode(NONE);
  VerifyAndFixShieldMode(NONE);
  return result;
}

void
AnalogWrite(unsigned int adc_addr, unsigned int mv_value)
{
  uint8_t buf[] = {0x00, 0x00};
  uint8_t adc_cmd[] = {0x30, 0x00, 0x00};
  /********DAC LTC2622 Input Word******
   COMMAND     | ADDRESS     | DATA (12 BITS + 4 DON'T-CARE BITS)
   C3 C2 C1 C0 | A3 A2 A1 A0 | D11 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0 | X X X X
   -----------------------------------------------------------------------------
   0  0  1  1  |-> COMMANDWrite to and Update (Power Up) n
               | 0  0  0  0  |-> VOUT_A (Pin2 of CN3~CN6)
               | 0  0  0  1  |-> VOUT_B (Pin2 of CN7~CN10)
               | 1  1  1  1  |-> All
   ***********************************/
  float dac;

  /* Boundary check */
  if (mv_value > 2230) {
    LOG(LOG_WARNING, "The maximum analog voltage output is 2230mV");
    mv_value = 2230;
  }

  if (adc_addr > 1) {
    LOG(LOG_WARNING, "The maximum DAC address is 1");
    adc_addr = 1;
  }

  dac = (float)mv_value * 1.8363228; /* 4095/2230 = 1.8363228 */
  adc_cmd[0] = adc_cmd[0] + adc_addr;
  adc_cmd[1] = dac / 16;
  adc_cmd[2] = ((int)floor(dac + 0.5) % 16) << 4;
  //LOG(LOG_ERROR,"adc_cmd[0]=0x%02x", (int)adc_cmd[0]);
  //LOG(LOG_ERROR,"adc_cmd[1]=0x%02x", (int)adc_cmd[1]);
  //LOG(LOG_ERROR,"adc_cmd[2]=0x%02x", (int)adc_cmd[2]);
  SetShieldMode(ADC);
  bcm2835_spi_writenb("0x68", 1);
  usleep(spi_delay_usecs);
  bcm2835_spi_writenb("0x86", 1);
  usleep(spi_delay_usecs);
  bcm2835_spi_transfern((char *)buf, 2);
  usleep(spi_delay_usecs);
  SetShieldMode(DAC);
  bcm2835_spi_writenb((char *)adc_cmd, 3);
  usleep(spi_delay_usecs);
  LOG(LOG_NOTICE, "output %4dmV on VOUT_%s", mv_value, 
      (0 == adc_addr) ? "A" : "B");
  SetShieldMode(NONE);
  VerifyAndFixShieldMode(NONE);
}

int
RpcPinWrite(const char *param, char *value)
{
  char *pin_str;
#ifndef PURE_RPI_V2
  char *param_cpy;
  char *port_str;
  int pin_num;
  int pin_map_num;
  int value_num;
  int res = 0;
  if (NULL == param || '\0' == param[0] || 'g' == param[0]) {
    LOG(LOG_ERROR, "Invalid parameter name: %s", param);
    return -1;
  }

  asprintf(&param_cpy, "%s", param);
  value_num = atoi(value);

  if (0 != value_num && 1 != value_num) {
    LOG(LOG_ERROR, "Invalid value, should be 0 or 1: %d", value_num);
    free(param_cpy);
    return -1;
  }

  if (NULL != strtok(param_cpy, ".")) {
    port_str = strtok(NULL, ".");
    port_str = port_str + strlen("cn");
    pin_str = strtok(NULL, ".");
    pin_num = atoi(pin_str);
  } else {
    LOG(LOG_ERROR, "Invalid parameter name: %s", param_cpy);
    free(param_cpy);
    return -1;
  }

  switch (port_str[1]) {
    case '1': {
      if (2 == pin_num) pin_map_num = CN11_2;
      if (4 == pin_num) pin_map_num = CN11_4;
      break;
    }
    case '2': {
      if (2 == pin_num) pin_map_num = CN12_2;
      if (4 == pin_num) pin_map_num = CN12_4;
      break;
    }
    case '3': {
      if (2 == pin_num) pin_map_num = CN13_2;
      if (4 == pin_num) pin_map_num = CN13_4;
      break;
    }
    case '4': {
      if (2 == pin_num) pin_map_num = CN14_2;
      if (4 == pin_num) pin_map_num = CN14_4;
      break;
    }
    case '5': {
      if (2 == pin_num) pin_map_num = CN15_2;
      if (4 == pin_num) pin_map_num = CN15_4;
      break;
    }
    case '6': {
      if (2 == pin_num) pin_map_num = CN16_2;
      if (4 == pin_num) pin_map_num = CN16_4;
      break;
    }
    case '7': {
      if (2 == pin_num) pin_map_num = CN17_2;
      if (4 == pin_num) pin_map_num = CN17_4;
      break;
    }
    case '8': {
      if (2 == pin_num) pin_map_num = CN18_2;
      if (4 == pin_num) pin_map_num = CN18_4;
      break;
    }
    case '9': {
      if (2 == pin_num) pin_map_num = CN19_2;
      if (4 == pin_num) pin_map_num = CN19_4;
      break;
    }
    case '0': {
      if (2 == pin_num) pin_map_num = CN20_2;
      if (4 == pin_num) pin_map_num = CN20_4;
      break;
    }
    default: {
      free(param_cpy);
      return -1;
    }
  }

  PinMode(atoi(port_str) - 10, DIO);
  res = DigitalWrite(pin_map_num, value_num);
  //LOG(LOG_ERROR,"%s %s --> %c %d", param_cpy, value, port_str[1], pin_num);
  free(param_cpy);
  return res;
#else // Using GPIO without I/O shield
  // Ex. get "10" from parameter "gpio.10", "3" from "gpio.3"
  asprintf(&pin_str, "%.3s", param + 5);
  DigitalWrite(atoi(pin_str), atoi(value));
  free(pin_str);
  return 0;
#endif // PURE_RPI_V2
}

void
SetShieldMode(int mode)
{
  /* Set all pin to output mode */
  bcm2835_gpio_fsel(GPIO_GEN0, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(GPIO_GEN1, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(GPIO_GEN2, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(GPIO_GEN3, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(GPIO_GEN4, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(GPIO_GEN5, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(GPIO_GEN6, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_write(GPIO_GEN6, HIGH);
  bcm2835_gpio_write(GPIO_GEN5, HIGH);
  bcm2835_gpio_write(GPIO_GEN4, HIGH);
  bcm2835_gpio_write(GPIO_GEN3, HIGH);
  usleep(gpio_delay_usecs);

  switch (mode) {
    case INIT_ALL_LOW: {  //GPIO_GEN6..0 = 000 0000
      bcm2835_gpio_write(GPIO_GEN6, LOW);
      bcm2835_gpio_write(GPIO_GEN5, LOW);
      bcm2835_gpio_write(GPIO_GEN4, LOW);
      bcm2835_gpio_write(GPIO_GEN3, LOW);
      bcm2835_gpio_write(GPIO_GEN2, LOW);
      bcm2835_gpio_write(GPIO_GEN1, LOW);
      bcm2835_gpio_write(GPIO_GEN0, LOW);
      break;
    }
    case NONE: {  //GPIO_GEN6..0 = 111 0000
      bcm2835_gpio_write(GPIO_GEN3, LOW);
      bcm2835_gpio_write(GPIO_GEN2, LOW);
      bcm2835_gpio_write(GPIO_GEN1, LOW);
      bcm2835_gpio_write(GPIO_GEN0, LOW);
      break;
    }
    case DAC: {  //GPIO_GEN6..0 = 111 1110
      bcm2835_gpio_write(GPIO_GEN2, HIGH);
      bcm2835_gpio_write(GPIO_GEN1, HIGH);
      bcm2835_gpio_write(GPIO_GEN0, LOW);
      break;
    }
    case RTC: {  //GPIO_GEN6..0 = 111 1101
      bcm2835_gpio_write(GPIO_GEN2, HIGH);
      bcm2835_gpio_write(GPIO_GEN1, LOW);
      bcm2835_gpio_write(GPIO_GEN0, HIGH);
      break;
    }
    case INTERNAL_REG: {  //GPIO_GEN6..0 = 111 1000
      bcm2835_gpio_write(GPIO_GEN2, LOW);
      bcm2835_gpio_write(GPIO_GEN1, LOW);
      bcm2835_gpio_write(GPIO_GEN0, LOW);
      break;
    }
    case OUTPUT_REG_1: {  //GPIO_GEN6..0 = 111 1001
      bcm2835_gpio_write(GPIO_GEN2, LOW);
      bcm2835_gpio_write(GPIO_GEN1, LOW);
      bcm2835_gpio_write(GPIO_GEN0, HIGH);
      break;
    }
    case OUTPUT_REG_2: {  //GPIO_GEN6..0 = 111 1010
      bcm2835_gpio_write(GPIO_GEN2, LOW);
      bcm2835_gpio_write(GPIO_GEN1, HIGH);
      bcm2835_gpio_write(GPIO_GEN0, LOW);
      break;
    }
    case INPUT_REG: {  //GPIO_GEN6..0 = 111 1011
      bcm2835_gpio_write(GPIO_GEN2, LOW);
      bcm2835_gpio_write(GPIO_GEN1, HIGH);
      bcm2835_gpio_write(GPIO_GEN0, HIGH);
      break;
    }
    default:
    case ADC: {  //GPIO_GEN6..0 = 111 1111
      bcm2835_gpio_write(GPIO_GEN2, HIGH);
      bcm2835_gpio_write(GPIO_GEN1, HIGH);
      bcm2835_gpio_write(GPIO_GEN0, HIGH);
      break;
    }
  }
  usleep(gpio_delay_usecs);
}

int
GetShieldMode()
{
  int mode = (bcm2835_gpio_lev(GPIO_GEN6) << 6) + 
             (bcm2835_gpio_lev(GPIO_GEN5) << 5) +
             (bcm2835_gpio_lev(GPIO_GEN4) << 4) + 
             (bcm2835_gpio_lev(GPIO_GEN3) << 3) +
             (bcm2835_gpio_lev(GPIO_GEN2) << 2) + 
             (bcm2835_gpio_lev(GPIO_GEN1) << 1) +
              bcm2835_gpio_lev(GPIO_GEN0);
  return mode;
}

void
VerifyAndFixShieldMode(const int mode)
{
  int reboot_count = 0;
  int sampling_count = 1;

  while (GetShieldMode() != mode) {
    if (sampling_count > MAX_SAMPLING_RETRY_TIME) {
      reboot_count = GetParamInt("shield.rebootcount");

      if (reboot_count > MAX_REBOOT_RETRY_TIME) {
        LOG(LOG_ERROR, "Cannot fix [GPIO_GEN6..0] by reboot %d times!", 
            MAX_REBOOT_RETRY_TIME);
		SetParam("shield.rebootcount", 0, INT);
        ShieldLibClose();
        exit(FALSE);
      } else {
        SetParam("shield.rebootcount", ++reboot_count, INT);
        LOG(LOG_ERROR, "Failed to switch shield mode [%d]: REBOOT!", mode);
        system("sudo reboot");
        //LOG(LOG_ERROR,"reboot!");
        exit(FALSE);
      }
    }

    LOG(LOG_ERROR, "sampling_count: %d", sampling_count);
    //SetShieldMode(mode);
    sampling_count ++;
  }
}

int
ShieldLibInit()
{
  if (bcm2835_init() == FALSE) {
    LOG(LOG_ERROR, "Failed to initialize bcm2835 library: %s", strerror(errno));

    if (errno == 13) {
      LOG(LOG_WARNING, "Please restart client with \"sudo\" command");
    }
    return FALSE;
  }

  spi_speed = LoadSpiSpeed();
  spi_delay_usecs = LoadSpiDelay();
#ifndef PURE_RPI_V2
  bcm2835_spi_begin();
  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);  // MSB First
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);  // CPOL = CPHA = 0
  bcm2835_spi_setClockDivider(spi_speed);
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
  
  // GPIO functionality check set from LOW to HIGH
  SetShieldMode(INIT_ALL_LOW);
  SetShieldMode(ADC);
  VerifyAndFixShieldMode(ADC);
  SetShieldMode(NONE);
#endif // PURE_RPI_V2
  return TRUE;
}

void ShieldLibClose()
{
#ifndef PURE_RPI_V2
  bcm2835_spi_end();
#endif // PURE_RPI_V2
  bcm2835_close();
}