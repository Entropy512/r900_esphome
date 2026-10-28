// Neptune R900 OOK decoder core. Portable C++ (no Arduino/ESP deps).
// Protocol (verified on a real capture, see README):
//   OOK, 32768 chips/s.  Frame = 64-chip preamble + 168-chip payload (232 chips, ~7.08 ms)
//   Preamble : 32 Manchester bits 00000000000000001110010101100100  (bit1="10", bit0="01")
//   Payload  : 42 digits x 4 chips. digit patterns 0=0011 1=0101 2=0110 3=1100 4=1010 5=1001
//              digit pair d1*6+d2 (<32) = one 5-bit symbol -> 21 symbols = 16 data + 5 RS parity
//   FEC      : RS over GF(32) poly 0x25 gen 2, shortened (10 zero symbols between data and parity),
//              5 syndromes at roots a^29..a^33 must be zero.
#pragma once
#include <stdint.h>
#include <stddef.h>

struct R900Msg {
  uint32_t id;
  uint8_t  unkn1;       // 8 bits, meter type? (0xA3 on test meter)
  uint8_t  nouse;       // 6 bits, day bins of no use
  uint8_t  backflow;    // 2 bits
  uint32_t consumption; // 24 bits, cumulative (units depend on meter)
  uint8_t  unkn3;       // 2 bits
  uint8_t  leak;        // 4 bits, day bins of leak
  uint8_t  leakNow;     // 2 bits, leak past 24h
  uint8_t  symbols[21];
  int      preambleErrors; // chip mismatches in the 64-chip preamble
  int      chipOffset;     // where preamble started in the chip buffer
};

struct R900Run { uint16_t us; uint8_t level; };  // level 1 = carrier present

enum { R900_MAX_CHIPS = 320, R900_FRAME_CHIPS = 232 };

// Expand carrier on/off runs into a chip array (1 chip = 30.5176 us).
// 8 leading idle chips and trailing idle are padded with 0. Returns chip count (= R900_MAX_CHIPS).
size_t r900_runs_to_chips(const R900Run* runs, size_t n, uint8_t* chips, bool invert);

// Find preamble + decode payload from a chip array. Accepts only if RS syndromes are all zero.
bool r900_decode_chips(const uint8_t* chips, size_t nChips, R900Msg& out);

// Convenience: runs -> chips -> message.
bool r900_decode(const R900Run* runs, size_t n, R900Msg& out);
