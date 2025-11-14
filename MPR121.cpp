#include "MPR121.h"

MPR121::MPR121() {}

MPR121::~MPR121() {
  if (i2c_dev) {
    delete i2c_dev;
    i2c_dev = nullptr;
  }
}

bool MPR121::begin(uint8_t i2c_addr, TwoWire *the_wire) {
  if (i2c_dev) {
    delete i2c_dev;
  }
  i2c_dev = new Adafruit_I2CDevice(i2c_addr, the_wire);
  if (!i2c_dev->begin()) {
    Serial.print(i2c_addr, HEX);
    Serial.println(": I2C Fail");
    return false;
  }

  // ---------- Soft Reset ----------
  // reset all regsiters to 0x00 except 0x5C=0x10 & 0x5D=0x24
  writeRegister(MPR121_SOFTRESET, 0x63);

  // ---------- STOP Mode ----------
  // Electrode Configuration Register: 0x5E (ECR)
  writeRegister(MPR121_ECR, 0x0);

  delay(10);

  uint8_t c = readRegister8(MPR121_CONFIG2);
  if (c != 0x24) {
    Serial.print(i2c_addr, HEX);
    Serial.println(": Reset Fail");
    return false;
  }

  // --- SET THRESHOLD VALUES ---
  setThresholds(MPR121_TOUCH_THRESHOLD_DEFAULT,
                MPR121_RELEASE_THRESHOLD_DEFAULT);

  // ---------- SET BASELINE TRACKING PARAMETERS ----------
  // see page 12 (sec 5.5) of datasheet AND application
  // note AN3891 for baseline tracking and filtering info
  //
  // with the 0.2" PLA plate on the copper touch pads, the delta between
  // filtered data and baseline was quite small (around -30, BEFORE any changes
  // were made to signal strength, i.e. charge/discharge current and time)
  //
  // the MPR121's default baseline tracking system parameters for this FALLING
  // case led the baseline to immediately track the filtered data (believing it
  // was noise/changes to environment instead of an actual touch event) this
  // rendered the pads useless in detecting touches
  //
  // we need to slow down this baseline tracking system by
  // setting new values for MHDF, NHDF, NCLF, and FDLF

  // ========== FALLING (filtered < baseline) ==========
  //  * MHDF = 1, small changes (±2 counts) are tracked immediately
  //    this is okay because tiny environmental changes should be followed
  writeRegister(MPR121_MHDF, 0x01);
  //  * NHDF = 1, when baseline does adjust, move by small increment (1 count)
  writeRegister(MPR121_NHDF, 0x01);
  //  * NCLF = 100, need 100 consecutive samples beyond small ±2 counts (MHDF)
  //    before adjusting baseline, at 100 samples/sec, every 1 second of
  //    sustained change will accumulate and change baseline (real drift), but
  //    brief touches won't
  writeRegister(MPR121_NCLF, 0x64);
  //  * FDLF = 2, add slight filter delay to slow the system even more
  writeRegister(MPR121_FDLF, 0x02);

  // ========== RISING (filtered > baseline) ==========
  writeRegister(MPR121_MHDR, 0x01);
  writeRegister(MPR121_NHDR, 0x01);
  writeRegister(MPR121_NCLR, 0x0E);
  writeRegister(MPR121_FDLR, 0x00);

  // ========== TOUCH ==========
  writeRegister(MPR121_NHDT, 0x00);
  writeRegister(MPR121_NCLT, 0x00);
  writeRegister(MPR121_FDLT, 0x00);
  // ----------------------------------------------------------------------------

  // --- DEBOUNCE ---
  writeRegister(MPR121_DEBOUNCE, 0);

  // ---------- CONFIG1 & CONFIG 2 ----------
  // Filter/Global CDC Configuration Register: 0x5C (CONFIG1)
  // set to 0x10 = 0b00010000
  //  * FFI (First Filter Iterations) - bits [7:6]
  //    ** FFI = 00, sets samples taken to 6 (default)
  //  * CDC (global Charge/Discharge Current) - bits [5:0]
  //    ** CDC = 010000, sets current to 16μA (default)
  // Filter/Global CDT Configuration Register: 0x5D (CONFIG2)
  // set to 0x41 = 0b01000001
  //  * CDT (global Charge/Discharge Time) - bits [7:5]
  //    ** CDT = 010, sets charge time to 1μS
  //  * SFI (Second Filter Iterations) - bits [4:3]
  //    ** CDT = 00, number of samples for 2nd level filter set to 4 (default)
  //  * ESI (Electrode Sample Interval) - bits [2:0]
  //    ** ESI = 001, electrode sampling period set to 2 ms
  // (see pg 14 of datasheet for description and encoding values)
  writeRegister(MPR121_CONFIG1, 0x10);
  writeRegister(MPR121_CONFIG2, 0x40);

  // ---------- AUTOCONFIG ----------
  // Auto-Configuration Control Register 0: 0x7B (AUTOCONFIG0)
  //  * FFI (First Filter Iterations) - bits [7:6] (same as 0x5C)
  //  * RETRY - bits [5:4]
  //  * BVA - bits [3:2], fill with same bits as CL bits in ECR (0x5E) register
  //  * ARE (Auto-Reconfiguration Enable) - bit 1
  //  * ACE (Auto-Configuration Enable) - bit 0
  // Auto-Configuration Control Register 1: 0x7C (AUTOCONFIG1)
  //  * SCTS (Skip Charge Time Search) - bit 7
  //  * bits [6:3] unused
  //  * OORIE (Out-of-range interrupt enable) - bit 2
  //  * ARFIE (Auto-reconfiguration fail interrupt enable) - bit 1
  //  * ACFIE (Auto-configuration fail interrupt enable) - bit 0
  // (see pg 17 of datasheet for description and encoding values)
  // important: disable autoconfig so it doesn't overwrite our CDC/CDT values
  writeRegister(MPR121_AUTOCONFIG0, 0b00001000);

  // ---------- RUN Mode ----------
  // Set ECR to enable all 12 electrodes
  //  * CL (Calibration Lock) - bits [7:6] = 0b10 (baseline tracking enabled
  //  with 5 bits)
  //  * ELEPROX_EN (Proximity Enable) - bits [5:4] = 0b00 (proximity detection
  //  disabled)
  //  * ELE_EN (Electrode Enable) - bits [3:0] = 0b1100 (run mode with electrode
  //  0 - 11 detection enabled)
  writeRegister(MPR121_ECR, 0x8C);

  return true;
}

uint8_t MPR121::readRegister8(uint8_t reg) {
  Adafruit_BusIO_Register thereg = Adafruit_BusIO_Register(i2c_dev, reg, 1);
  return (thereg.read());
}

void MPR121::writeRegister(uint8_t reg, uint8_t value) {
  bool stop_required =
      true; // MPR121 must be put in Stop Mode to write to most registers

  Adafruit_BusIO_Register ecr_reg =
      Adafruit_BusIO_Register(i2c_dev, MPR121_ECR, 1);
  uint8_t ecr_backup =
      ecr_reg.read(); // get current value of MPR121_ECR register

  if ((reg == MPR121_ECR) || ((0x73 <= reg) && (reg <= 0x7A)))
    stop_required = false;

  if (stop_required)
    ecr_reg.write(0x00); // set to zero to set board in stop mode

  Adafruit_BusIO_Register the_reg = Adafruit_BusIO_Register(i2c_dev, reg, 1);
  the_reg.write(value); // write value in passed register

  if (stop_required)
    ecr_reg.write(ecr_backup); // write back the previous set ECR settings
}

void MPR121::setThresholds(uint8_t touch, uint8_t release) {
  for (uint8_t i = 0; i < 12; i++) { // set all thresholds to same value
    writeRegister(MPR121_TOUCHTH_0 + 2 * i, touch);
    writeRegister(MPR121_RELEASETH_0 + 2 * i, release);
  }
}

void MPR121::dumpCDCandCDTRegisters() {
  Serial.println("ELECTRODE: 00 01 02 03 04 05 06 07 08 09 10 11");
  Serial.println("           -- -- -- -- -- -- -- -- -- -- -- --");

  // CDC - Charge Discharge Current
  Serial.print("CDC:       ");
  for (int i = 0; i < 12; i++) {
    uint8_t reg = readRegister8(0x5F + i);
    if (reg < 10)
      Serial.print(" ");
    Serial.print(reg);
    Serial.print(" ");
  }
  Serial.println();

  // CDT - Charge Discharge Time
  Serial.print("CDT:       ");
  for (int i = 0; i < 6; i++) {
    // read CDT registers (6 total)
    uint8_t reg = readRegister8(0x6C + i);
    uint8_t cdt_even = reg & 0b111; // even numbered electrode CDT -> bits[2:0]
    uint8_t cdt_odd =
        (reg >> 4) & 0b111; // odd numbered electrode CDT -> bits[6:4]
    if (cdt_even < 10)
      Serial.print(" ");
    Serial.print(cdt_even);
    Serial.print(" ");
    if (cdt_odd < 10)
      Serial.print(" ");
    Serial.print(cdt_odd);
    Serial.print(" ");
  }
  Serial.println();
  Serial.println("----------------------------------------");
  Serial.println();
}
