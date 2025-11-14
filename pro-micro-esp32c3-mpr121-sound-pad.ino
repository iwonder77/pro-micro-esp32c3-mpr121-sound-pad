#include <Wire.h>
#include "MPR121.h"

const uint8_t NUM_ELECTRODES = 3;
const uint8_t MPR121_I2C_ADDR = 0x5A;

MPR121 mpr121 = MPR121();

void setup() {
  ire.begin();
    Serial.begin(9600);
  while (!Serial);
  delay(3000);

  if (!mpr121.begin(MPR121_I2C_ADDR, &Wire)) {
    Serial.print("MPR121 not found, check wiring");
    // hold indefinitely
    while (1);
  }

  Serial.println("Initial CDC and CDT Values:");
  mpr121.dumpCDCandCDTRegisters();
  delay(1000);


  Serial.println("New CDC and CDT Values:");
  mpr121.dumpCDCandCDTRegisters();
  delay(1000);

  Serial.println("=== ELECTRODE READINGS ===");
  Serial.println("Electrode, Filtered, Baseline, Delta");
}

void loop() {
  for (uint8_t i = 0; i < NUM_ELECTRODES; i++) {
    uint8_t lsb = mpr121.readRegister8(0x04 + 2 * i);         // will return 8 bits
    uint8_t msb = mpr121.readRegister8(0x05 + 2 * i);         // will return 2 bits
    uint16_t filtered = ((uint16_t)(msb & 0x03) << 8) | lsb;  // combined into 10-bit value

    // read 8-bit baseline REGISTERS (these 8-bits acrually represent the upper 8 bits of a 10-bit system)
    uint8_t baseline_raw = mpr121.readRegister8(0x1E + i);
    uint16_t baseline = baseline_raw << 2;  // left shift to match filtered data scale (10-bits)

    // calculate delta (filtered - baseline), cast to int16_t because delta can be negative
    int16_t delta = (int16_t)filtered - (int16_t)baseline;

    // print results
    Serial.print(i);
    Serial.print(",");
    Serial.print(filtered);
    Serial.print(",");
    Serial.print(baseline);
    Serial.print(",");
    Serial.print(delta);
    Serial.println();
  }

  Serial.println();
  delay(500);
}
