#ifndef MPR121_H
#define MPR121_H
#define I2CADDR_DEFAULT 0x5A

#include "Arduino.h"
#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CDevice.h>

enum {
  MPR121_TOUCHSTATUS_L = 0x00,
  MPR121_TOUCHSTATUS_H = 0x01,
  MPR121_FILTDATA_0L = 0x04,
  MPR121_FILTDATA_0H = 0x05,
  MPR121_BASELINE_0 = 0x1E,
  MPR121_MHDR = 0x2B,
  MPR121_NHDR = 0x2C,
  MPR121_NCLR = 0x2D,
  MPR121_FDLR = 0x2E,
  MPR121_MHDF = 0x2F,
  MPR121_NHDF = 0x30,
  MPR121_NCLF = 0x31,
  MPR121_FDLF = 0x32,
  MPR121_NHDT = 0x33,
  MPR121_NCLT = 0x34,
  MPR121_FDLT = 0x35,
  MPR121_TOUCHTH_0 = 0x41,
  MPR121_RELEASETH_0 = 0x42,
  MPR121_DEBOUNCE = 0x5B,
  MPR121_CONFIG1 = 0x5C,
  MPR121_CONFIG2 = 0x5D,
  MPR121_ECR = 0x5E,
  MPR121_AUTOCONFIG0 = 0x7B,
  MPR121_AUTOCONFIG1 = 0x7C,
  MPR121_UPLIMIT = 0x7D,
  MPR121_LOWLIMIT = 0x7E,
  MPR121_TARGETLIMIT = 0x7F,
  MPR121_SOFTRESET = 0x80,
};

class MPR121 {
public:
  MPR121();
  ~MPR121();                       // destructor
  MPR121(const MPR121 &) = delete; // no copy
  MPR121 &operator=(const MPR121 &) = delete;
  bool begin(uint8_t i2c_addr = I2CADDR_DEFAULT, TwoWire *the_wire = &Wire);
  uint8_t readRegister8(uint8_t reg);
  void writeRegister(uint8_t reg, uint8_t value);
  void setThresholds(uint8_t touch, uint8_t release);
  void dumpCDCandCDTRegisters();

private:
  Adafruit_I2CDevice *i2c_dev = NULL;
  const uint8_t MPR121_TOUCH_THRESHOLD_DEFAULT =
      12; // default touch threshold value
  const uint8_t MPR121_RELEASE_THRESHOLD_DEFAULT =
      6; // default relese threshold value
};

// Unused functions of original Adafruit_MPR121 library:
/*    uint16_t readRegister16(uint8_t reg);
      uint16_t touched(void);
      uint16_t filteredData(uint8_t t);
      uint16_t baselineData(uint8_t t);
*/

#endif
