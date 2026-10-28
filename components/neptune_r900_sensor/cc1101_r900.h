// Minimal CC1101 driver, hard-wired for Neptune R900: OOK, 32.768 kBd async serial out on GDO0,
// 116 kHz RX filter, 26 MHz crystal. No library needed.
#pragma once
#include <Arduino.h>
#include <SPI.h>

// Pins are supplied by the ESPHome YAML configuration at runtime.
static int cs_pin = 27;
static int gdo0_pin = 4;
static int sck_pin = 18;
static int mosi_pin = 23;
static int miso_pin = 19;

inline void configurePins(int cs, int gdo0, int sck, int mosi, int miso) {
  cs_pin = cs; gdo0_pin = gdo0; sck_pin = sck; mosi_pin = mosi; miso_pin = miso;
}

namespace cc1101 {

// ---- registers / strobes ----
enum : uint8_t {
  IOCFG2 = 0x00, IOCFG0 = 0x02, FIFOTHR = 0x03, PKTCTRL1 = 0x07, PKTCTRL0 = 0x08, CHANNR = 0x0A,
  FSCTRL1 = 0x0B, FSCTRL0 = 0x0C, FREQ2 = 0x0D, FREQ1 = 0x0E, FREQ0 = 0x0F,
  MDMCFG4 = 0x10, MDMCFG3 = 0x11, MDMCFG2 = 0x12, MDMCFG1 = 0x13, MDMCFG0 = 0x14, DEVIATN = 0x15,
  MCSM2 = 0x16, MCSM1 = 0x17, MCSM0 = 0x18, FOCCFG = 0x19, BSCFG = 0x1A,
  AGCCTRL2 = 0x1B, AGCCTRL1 = 0x1C, AGCCTRL0 = 0x1D, FREND1 = 0x21, FREND0 = 0x22,
  FSCAL3 = 0x23, FSCAL2 = 0x24, FSCAL1 = 0x25, FSCAL0 = 0x26, TEST2 = 0x2C, TEST1 = 0x2D, TEST0 = 0x2E,
  SRES = 0x30, SCAL = 0x33, SRX = 0x34, SIDLE = 0x36, SFRX = 0x3A,
  ST_PARTNUM = 0x30, ST_VERSION = 0x31, ST_RSSI = 0x34, ST_MARCSTATE = 0x35,
};

// ---- R900 settings ----
static const double XTAL_HZ = 26000000.0;
// Channel (center of the 131.072 kHz grid): ch n (group 1, n=0..16) = 911.08797 + n*0.131072 MHz.
// Measured on the capture: ch10 = 912.3987 MHz.
inline uint32_t freq_word(double hz) { return (uint32_t)(hz / (XTAL_HZ / 65536.0) + 0.5); }
inline double   channel_hz(int n)   { return (911.08797 + n * 0.131072) * 1e6; }   // n 0..16; group 2 (914.857 + n*0.131072) not handled

static inline void sel(bool on) { digitalWrite(cs_pin, on ? LOW : HIGH); }

inline uint8_t readReg(uint8_t a)    { sel(1); SPI.transfer(a | 0xC0); uint8_t v = SPI.transfer(0); sel(0); return v; } // status regs & config
inline void    writeReg(uint8_t a, uint8_t v) { sel(1); SPI.transfer(a); SPI.transfer(v); sel(0); }
inline void    strobe(uint8_t s)     { sel(1); SPI.transfer(s); sel(0); }

inline void setFrequency(double hz) {
  uint32_t w = freq_word(hz);
  strobe(SIDLE);
  writeReg(FREQ2, (w >> 16) & 0xFF); writeReg(FREQ1, (w >> 8) & 0xFF); writeReg(FREQ0, w & 0xFF);
  strobe(SRX);   // MCSM0 autocal on IDLE->RX
}

inline int rssiDbm() { int r = readReg(ST_RSSI); return (r >= 128 ? (r - 256) / 2 : r / 2) - 74; }

// Returns true if chip answered (PARTNUM 0, VERSION 0x04 or 0x14).
inline bool begin(double carrierHz) {
  pinMode(cs_pin, OUTPUT); sel(0);
  pinMode(gdo0_pin, INPUT);
  SPI.begin(sck_pin, miso_pin, mosi_pin, cs_pin);
  SPI.setFrequency(4000000);
  SPI.setDataMode(SPI_MODE0);
  // reset
  sel(0); delayMicroseconds(5); sel(1); delayMicroseconds(10); sel(0); delayMicroseconds(45);
  sel(1); while (digitalRead(miso_pin)) { }       // wait for crystal
  SPI.transfer(SRES); while (digitalRead(miso_pin)) { } sel(0);
  delay(5);
  uint8_t ver = readReg(ST_VERSION);
  if (ver == 0x00 || ver == 0xFF) return false;

  writeReg(IOCFG2, 0x2E);      // GDO2 high-Z (unused)
  writeReg(IOCFG0, 0x0D);      // GDO0 = serial data output (async, raw demodulated OOK)
  writeReg(FIFOTHR, 0x47);
  writeReg(PKTCTRL1, 0x00);
  writeReg(PKTCTRL0, 0x32);    // asynchronous serial mode, infinite packet, no CRC/whitening
  writeReg(CHANNR, 0x00);
  writeReg(FSCTRL1, 0x06);     // IF = 26e6/2^10*6 = 152 kHz
  writeReg(FSCTRL0, 0x00);     // freq trim (use to correct crystal error, 1 LSB = 396.7 Hz*... see README)
  uint32_t w = freq_word(carrierHz);
  writeReg(FREQ2, (w >> 16) & 0xFF); writeReg(FREQ1, (w >> 8) & 0xFF); writeReg(FREQ0, w & 0xFF);
  writeReg(MDMCFG4, 0x0A);     // 0x0A is RX filter 812 kHz (E=2,M=3) and DRATE_E=10 supports 32.73kbd (the Exponent)
  writeReg(MDMCFG3, 0x4A);     // DRATE_M=74 -> 32.73 kBd (the Mantissa)
  writeReg(MDMCFG2, 0x30);     // OOK/ASK, no Manchester, no sync, DC filter on
  writeReg(MDMCFG1, 0x22);
  writeReg(MDMCFG0, 0xF8);
  writeReg(DEVIATN, 0x00);
  writeReg(MCSM2, 0x07);
  writeReg(MCSM1, 0x0C);       // stay in RX after RX (no packets in async mode)
  writeReg(MCSM0, 0x18);       // autocal when going IDLE -> RX
  writeReg(FOCCFG, 0x00);      // frequency offset compensation off (unreliable for OOK)
  writeReg(BSCFG, 0x6C);
  writeReg(AGCCTRL2, 0x04);    // per TI OOK example; try 0x03..0x07 if sensitivity is poor
  writeReg(AGCCTRL1, 0x00);
  writeReg(AGCCTRL0, 0x92);
  writeReg(FREND1, 0xB6);
  writeReg(FREND0, 0x11);      // OOK
  writeReg(FSCAL3, 0xE9); writeReg(FSCAL2, 0x2A); writeReg(FSCAL1, 0x00); writeReg(FSCAL0, 0x1F);
  writeReg(TEST2, 0x81); writeReg(TEST1, 0x35); writeReg(TEST0, 0x09);
  strobe(SIDLE); strobe(SFRX); strobe(SCAL); delay(2);
  strobe(SRX);
  return true;
}

#if defined(RF_MODULE_INIT_STATUS)
inline void dump() {
  Serial.printf("CC1101 PARTNUM=0x%02X VERSION=0x%02X MARCSTATE=0x%02X RSSI=%d dBm\n",
                readReg(ST_PARTNUM), readReg(ST_VERSION), readReg(ST_MARCSTATE) & 0x1F, rssiDbm());
  for (uint8_t a = 0; a <= 0x2E; a++) { Serial.printf("%02X=%02X ", a, readReg(a)); if ((a & 7) == 7) Serial.println(); }
  Serial.println();
}
#endif
}  // namespace cc1101
