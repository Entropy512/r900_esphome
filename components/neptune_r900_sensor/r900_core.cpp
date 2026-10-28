#include "r900_core.h"
#include <string.h>
#include <math.h>

static const int      LEAD_CHIPS   = 8;
static const uint32_t GLITCH_US    = 12;   // runs shorter than this are noise (chip = 30.5 us)
static const int      MAX_PRE_ERR  = 6;    // preamble chip mismatches tolerated (of 64)
static const int      MAX_RUNS     = 600;

static uint8_t gfExp[62], gfLog[32];
static uint8_t preChips[64];
static bool    inited = false;

static void init_tables() {
  if (inited) return;
  int x = 1;
  for (int i = 0; i < 31; i++) { gfExp[i] = gfExp[i + 31] = (uint8_t)x; gfLog[x] = (uint8_t)i; x <<= 1; if (x & 32) x ^= 37; }
  const char* pre = "00000000000000001110010101100100";
  for (int i = 0; i < 32; i++) { bool b = pre[i] == '1'; preChips[2 * i] = b; preChips[2 * i + 1] = !b; }
  inited = true;
}
static inline uint8_t gmul(uint8_t a, uint8_t b) { return (a == 0 || b == 0) ? 0 : gfExp[gfLog[a] + gfLog[b]]; }

static bool rs_ok(const uint8_t sym[21]) {
  uint8_t msg[31];
  memset(msg, 0, sizeof msg);
  memcpy(msg, sym, 16);
  memcpy(msg + 26, sym + 16, 5);
  for (int i = 0; i < 5; i++) {
    uint8_t s = msg[0], root = gfExp[(29 + i) % 31];
    for (int j = 1; j < 31; j++) s = gmul(s, root) ^ msg[j];
    if (s) return false;
  }
  return true;
}

static inline int digit_of(uint8_t nib) {
  switch (nib) { case 0x3: return 0; case 0x5: return 1; case 0x6: return 2;
                 case 0xC: return 3; case 0xA: return 4; case 0x9: return 5; default: return -1; }
}

size_t r900_runs_to_chips(const R900Run* runs, size_t n, uint8_t* chips, bool invert) {
  init_tables();
  memset(chips, 0, R900_MAX_CHIPS);
  if (n > MAX_RUNS) n = MAX_RUNS;
  // merge glitches: (L, short !L, L) -> one L run
  static R900Run m[MAX_RUNS];
  size_t k = 0;
  for (size_t i = 0; i < n; i++) {
    if (runs[i].us < GLITCH_US && k > 0 && i + 1 < n) {
      m[k - 1].us = (uint16_t)((uint32_t)m[k - 1].us + runs[i].us + runs[i + 1].us > 65535 ? 65535 : m[k - 1].us + runs[i].us + runs[i + 1].us);
      i++;  // next run (same level as m[k-1]) absorbed
    } else m[k++] = runs[i];
  }
  // Chip clock recovery. T = time since the first carrier-on edge. Rising edges are used to
  // estimate the chip grid phase (falling edges are skewed by slicer threshold / AGC), then each
  // chip is read at its centre. Tolerates ~+-0.4 chip of edge skew.
  size_t first = 0;
  while (first < k && !m[first].level) first++;      // frame starts at first carrier-on run
  if (first >= k) return R900_MAX_CHIPS;
  const float Tc = 1000000.0f / 32768.0f;
  static float edgeT[MAX_RUNS + 1];                  // end time of each run (from frame start)
  // Rising and falling edges are skewed in opposite directions by the slicer, so estimate the
  // grid phase of each edge type separately (circular mean) and correct edge times before sampling.
  float t = 0, rx = 0, ry = 0, fx = 0, fy = 0; int nr = 0, nf = 0;
  for (size_t i = first; i < k; i++) {
    t += m[i].us;
    float ph = 6.2831853f * (t / Tc);
    if (m[i].level) { fx += cosf(ph); fy += sinf(ph); nf++; }   // run end = falling edge
    else            { rx += cosf(ph); ry += sinf(ph); nr++; }   // run end = rising edge
  }
  float rphase = nr ? atan2f(ry, rx) / 6.2831853f * Tc : 0.0f;   // us
  float fphase = nf ? atan2f(fy, fx) / 6.2831853f * Tc : 0.0f;
  t = 0;
  for (size_t i = first; i < k; i++) {
    t += m[i].us;
    edgeT[i - first] = t - (m[i].level ? fphase : rphase);
  }
  float mphase = 0.0f;
  size_t i = first; float runEnd = edgeT[0];
  for (size_t c = LEAD_CHIPS; c < R900_MAX_CHIPS; c++) {
    float ts = ((float)(c - LEAD_CHIPS) + mphase + 0.5f) * Tc;
    while (i < k && ts >= runEnd) { i++; if (i < k) runEnd = edgeT[i - first]; }
    uint8_t v = (i < k) ? (m[i].level != 0) : 0;
    chips[c] = v ^ (invert ? 1 : 0);
  }
  return R900_MAX_CHIPS;
}

bool r900_decode_chips(const uint8_t* chips, size_t nChips, R900Msg& out) {
  init_tables();
  if (nChips < R900_FRAME_CHIPS) return false;
  for (size_t s = 0; s + R900_FRAME_CHIPS <= nChips; s++) {
    int mm = 0;
    for (int i = 0; i < 64 && mm <= MAX_PRE_ERR; i++) mm += (chips[s + i] != preChips[i]);
    if (mm > MAX_PRE_ERR) continue;
    const uint8_t* p = chips + s + 64;
    int dig[42]; bool bad = false;
    for (int i = 0; i < 42; i++) {
      uint8_t nib = (p[4*i] << 3) | (p[4*i+1] << 2) | (p[4*i+2] << 1) | p[4*i+3];
      dig[i] = digit_of(nib);
      if (dig[i] < 0) { bad = true; break; }
    }
    if (bad) continue;
    uint8_t sym[21];
    for (int i = 0; i < 21; i++) { int v = dig[2*i] * 6 + dig[2*i+1]; if (v > 31) { bad = true; break; } sym[i] = (uint8_t)v; }
    if (bad || !rs_ok(sym)) continue;
    // unpack 16 data symbols = 80 bits
    uint8_t bits[80];
    for (int i = 0; i < 16; i++) for (int b = 0; b < 5; b++) bits[5*i + b] = (sym[i] >> (4 - b)) & 1;
    auto get_bits = [&](int a, int n) { uint32_t v = 0; for (int i = 0; i < n; i++) v = (v << 1) | bits[a + i]; return v; };
    out.id = get_bits(0, 32); out.unkn1 = get_bits(32, 8); out.nouse = get_bits(40, 6); out.backflow = get_bits(46, 2);
    out.consumption = get_bits(48, 24); out.unkn3 = get_bits(72, 2); out.leak = get_bits(74, 4); out.leakNow = get_bits(78, 2);
    memcpy(out.symbols, sym, 21);
    out.preambleErrors = mm; out.chipOffset = (int)s;
    return true;
  }
  return false;
}

bool r900_decode(const R900Run* runs, size_t n, R900Msg& out) {
  uint8_t chips[R900_MAX_CHIPS];
  r900_runs_to_chips(runs, n, chips, false);
  return r900_decode_chips(chips, R900_MAX_CHIPS, out);
}
