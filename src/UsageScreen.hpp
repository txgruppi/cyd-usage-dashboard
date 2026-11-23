#pragma once

#include <TFT_eSPI.h>
#include <vector>

#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace usage {

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

constexpr size_t paletteLen = 16;
const uint16_t *palette();

bool setPaletteEntry(size_t index, uint16_t rgb565);
bool setPaletteEntryRgb888(size_t index, uint32_t rgb888);
uint16_t rgb888To565(uint32_t rgb888);
void applyPalette(TFT_eSprite &sprite);

void drawUsageScreen(TFT_eSPI &g, std::vector<Usage> &rows);

} // namespace usage