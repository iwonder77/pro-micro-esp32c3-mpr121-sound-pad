#include <Wire.h>
#include <Adafruit_MPR121.h>

const uint8_t NUM_ELECTRODES = 3;
const uint8_t MPR121_I2C_ADDR = 0x5A;
Adafruit_MPR121 mpr121 = Adafruit_MPR121();

void dumpCDCandCDTRegisters() {
  delay(1000);
  Serial.println("ELECTRODE: 00 01 02 03 04 05 06 07 08 09 10 11");
  Serial.println("           -- -- -- -- -- -- -- -- -- -- -- --");
  // CDC - Charge Discharge Current
  Serial.print("CDC:       ");
  for (int i = 0; i < 12; i++) {
    uint8_t reg = mpr121.readRegister8(0x5F + i);
    if (reg < 10) Serial.print(" ");
    Serial.print(reg);
    Serial.print(" ");
  }
  Serial.println();
  // CDT
  Serial.print("CDT:       ");
  for (int i = 0; i < 6; i++) {
    // read CDT registers (6 total)
    uint8_t reg = mpr121.readRegister8(0x6C + i);
    uint8_t cdt_even = reg & 0b111;        // even numbered electrode CDT -> bits[2:0]
    uint8_t cdt_odd = (reg >> 4) & 0b111;  // odd numbered electrode CDT -> bits[6:4]
    if (cdt_even < 10) Serial.print(" ");
    Serial.print(cdt_even);
    Serial.print(" ");
    if (cdt_odd < 10) Serial.print(" ");
    Serial.print(cdt_odd);
    Serial.print(" ");
  }
  Serial.println();
  Serial.println("----------------------------------------");
  Serial.println();
}

// --- BASELINE TRACKING ---
// see page 12 (sec 5.5) of datasheet AND application note AN3891 for baseline tracking and filtering info
// with the 0.2" acrylic, the delta between filtered and baseline is quite small (around -30, before changes to CDC)
// with the default baseline FALLING (filtered < baseline) parameters, the MPR121 is led to believe these changes are noise
// and promptly drops the baseline, completely killing our touch detection
// setting new values for MHDF, NHDF, NCLF, and FDLF should alleviate these issues, more explained below
void setupBaselineTracking() {
  Serial.println("\n=== CONFIGURING BASELINE TRACKING ===");

  // ============================================
  // FALLING BASELINE (filtered < baseline)
  // ============================================
  // THIS IS WHAT MATTERS FOR TOUCH!
  // when you touch, filtered drops, triggering falling mode
  // we want to prevent baseline from immediately tracking your finger

  // MHD - Max Half Delta
  mpr121.writeRegister(MPR121_MHDF, 0x01);  // MHD Falling = 1
  // keep same as library setiting - i.e. small changes (±2 counts) are tracked immediately
  // this is okay because tiny environmental changes should be followed

  // NHD - Noise Half Delta
  mpr121.writeRegister(MPR121_NHDF, 0x01);  // NHD Falling = 1
  // when baseline does adjust, move by small increment (1 count)
  // this is conservative - was 5 in library, we're being more careful

  // NCL - Noise Count Limit
  mpr121.writeRegister(MPR121_NCLF, 0x64);  // NCL Falling = 100
  // need 100 consecutive samples beyond ±2 before adjustin
  // at 100 samples/sec = 1 second of sustained change
  // a brief touch won't accumulate this, but real drift will

  // FDL - Filter Delay Limit
  mpr121.writeRegister(MPR121_FDLF, 0x02);  // FDL Falling = 2
  // add slight filter delay to slow the system even more
}

// Set CDC for electrodes 0–2 (charge current in µA: 0..63)
void setCDC(uint8_t cdc) {
  if (cdc > 63) cdc = 63;

  // Electrodes 0,1,2 are at registers 0x5F, 0x60, 0x61
  for (uint8_t ele = 0; ele < 3; ele++) {
    mpr121.writeRegister(0x5F + ele, cdc);
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial);
  delay(3000);
  Wire.begin();

  if (!mpr121.begin(MPR121_I2C_ADDR, &Wire)) {
    Serial.print("MPR121 not found, check wiring");
    // hold indefinitely
    while (1);
  }

  Serial.println("Initial CDC and CDT Values:");
  dumpCDCandCDTRegisters();
  delay(1000);

  setupBaselineTracking();
  setCDC(48);
  // mpr121.setAutoconfig(true);

  Serial.println("New CDC and CDT Values:");
  dumpCDCandCDTRegisters();
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
    Serial.println(delta);
  }

  Serial.println();
  delay(500);
}
