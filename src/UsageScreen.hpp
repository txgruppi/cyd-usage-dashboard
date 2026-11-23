#pragma once

#include <TFT_eSPI.h>
#include <vector>

#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace usage {
// most rows that fit 240 px at the size-1 compact layout
constexpr int N_MAX = 12;

// 4-bit sprite indexes 0..11. RGB565 in the palette table; bulk update via
// `palette load` / `palette dump` (12 #RRGGBB colours).
//
//  idx  name          default    shown    role
//  0    C_BG          0x10A4  #101421  background
//  1    C_TRACK       0x31EA  #313C52  bar trough
//  2    C_DIV         0x2188  #213042  row divider
//  3    C_BLUE        0x4C5D  #4A8AEF  bar fill < 75%
//  4    C_MUTED       0xA5D9  #A5BACE  general muted text
//  5    C_TEXT        0xF7BF  #F7F7FF  names and percentages
//  6    C_GOOD        0x3E6F  #39CF7B  good accent (stock UI unused)
//  7    C_WARN        0xF5A0  #F7B600  bar fill >= 75%
//  8    C_DANGER      0xE208  #E74142  bar fill >= 90% (and >100%)
//  9    C_RESET_SOON  0x5712  #52E394  reset label <= 2 days
//  10   C_RESET_WEEK  0x661F  #63C3FF  reset label <= 7 days
//  11   C_RESET_FAR   0xA5D9  #A5BACE  reset label  > 7 days
constexpr uint16_t C_BG = 0;
constexpr uint16_t C_TRACK = 1;
constexpr uint16_t C_DIV = 2;
constexpr uint16_t C_BLUE = 3;
constexpr uint16_t C_MUTED = 4;
constexpr uint16_t C_TEXT = 5;
constexpr uint16_t C_GOOD = 6;
constexpr uint16_t C_WARN = 7;
constexpr uint16_t C_DANGER = 8;
constexpr uint16_t C_RESET_SOON = 9;
constexpr uint16_t C_RESET_WEEK = 10;
constexpr uint16_t C_RESET_FAR = 11;

struct Usage {
  char *name = nullptr;
  float pct = 0;
  int resetDays = -1; // days until reset, -1 = no reset date

  Usage() = default;
  ~Usage() { freeName(); }

  Usage(const Usage &) = delete;
  Usage &operator=(const Usage &) = delete;

  Usage(Usage &&other) noexcept
      : name(other.name), pct(other.pct), resetDays(other.resetDays) {
    other.name = nullptr;
  }

  Usage &operator=(Usage &&other) noexcept {
    if (this != &other) {
      freeName();
      name = other.name;
      pct = other.pct;
      resetDays = other.resetDays;
      other.name = nullptr;
    }
    return *this;
  }

  void setName(const char *s) {
    freeName();
    name = (s && s[0]) ? strdup(s) : nullptr;
  }

private:
  void freeName() {
    free(name);
    name = nullptr;
  }
};

constexpr size_t paletteLen = 12;
const uint16_t *palette();

bool setPaletteEntry(size_t index, uint16_t rgb565);
uint16_t rgb888To565(uint32_t rgb888);
void applyPalette(TFT_eSprite &sprite);

void drawUsageScreen(TFT_eSPI &g, std::vector<Usage> &rows);

} // namespace usage
