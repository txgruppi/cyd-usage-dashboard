#include "UsageScreen.hpp"

#include <stdio.h>
#include <string.h>

namespace usage {

namespace {
struct State {
  uint32_t width;
  uint32_t height;
  uint32_t x;
  uint32_t y;
  uint32_t row_height;
};

uint16_t s_palette[16] = {
    0x0000, 0x7BEF, 0xBDF7, 0xFFFF, 0x7800, 0xF800, 0x03E0, 0x07E0,
    0x7BE0, 0xFFE0, 0x000F, 0x001F, 0x780F, 0xF81F, 0x03EF, 0x07FF,
};

constexpr uint16_t C_BG = 0;
constexpr uint16_t C_TRACK = 1;
constexpr uint16_t C_DIV = 2;
constexpr uint16_t C_BLUE = 10;
constexpr uint16_t C_MUTED = 2;
constexpr uint16_t C_TEXT = 3;
constexpr uint16_t C_GOOD = 15;
constexpr uint16_t C_WARN = 9;
constexpr uint16_t C_DANGER = 5;

constexpr int MARGIN_X = 12, RIGHT = 308, TEXT_GAP = 8;
constexpr int NAME_SIZE = 2, DATE_SIZE = 1, PCT_SIZE = 2;
constexpr int NAME_Y_OFF = 14, DATE_Y_OFF = 18, PCT_Y_OFF = 33;
constexpr int BAR_X = 12, BAR_Y_OFF = 36, BAR_W = 240, BAR_H = 10;
constexpr float WARN_AT = 75.f, DANGER_AT = 90.f;

int inkWidth(const char *s, int size) {
  int n = s ? (int)strlen(s) : 0;
  return n ? (6 * size * n - size) : 0;
}

int rightAlignX(const char *s, int size) {
  int w = inkWidth(s, size);
  return w ? (RIGHT - (w - 1)) : RIGHT;
}

uint16_t fillColor(float p) {
  if (p >= DANGER_AT)
    return C_DANGER;
  if (p >= WARN_AT)
    return C_WARN;
  return C_BLUE;
}

uint16_t resetColor(int days) {
  if (days <= 2)
    return C_GOOD;
  if (days <= 7)
    return C_BLUE;
  return C_MUTED;
}

bool formatReset(char *buf, size_t len, int days) {
  if (days < 0)
    return false;
  if (days == 0)
    snprintf(buf, len, "today");
  else
    snprintf(buf, len, "in %dd", days > 999 ? 999 : days);
  return true;
}

void fitName(char *buf, size_t len, const char *name, int avail) {
  if (!name)
    name = "";
  if (inkWidth(name, NAME_SIZE) <= avail) {
    snprintf(buf, len, "%s", name);
    return;
  }
  int maxChars = (avail + NAME_SIZE) / (6 * NAME_SIZE);
  if (maxChars < 2) {
    buf[0] = 0;
    return;
  }
  snprintf(buf, len, "%.*s..", maxChars - 2, name);
}

void text(TFT_eSPI &g, const char *s, int x, int y, uint16_t c, int size) {
  uint16_t bg = C_BG;
  g.setTextFont(1);
  g.setTextSize(size);
  g.setTextDatum(TL_DATUM);
  g.setTextColor(c, bg);
  g.drawString(s, x, y);
}

void drawUsageRow(TFT_eSPI &g, State &state, const Usage &u) {
  g.fillRect(state.x, state.y, state.width, state.row_height, C_BG);

  char dateBuf[12];
  const char *date =
      formatReset(dateBuf, sizeof dateBuf, u.resetDays) ? dateBuf : nullptr;
  int avail = date ? (rightAlignX(date, DATE_SIZE) - TEXT_GAP - MARGIN_X)
                   : (RIGHT - MARGIN_X + 1);

  char name[32];
  fitName(name, sizeof name, u.name, avail);
  text(g, name, MARGIN_X, state.y + NAME_Y_OFF, C_TEXT, NAME_SIZE);
  if (date)
    text(g, date, rightAlignX(date, DATE_SIZE), state.y + DATE_Y_OFF,
         resetColor(u.resetDays), DATE_SIZE);

  g.fillRect(BAR_X, state.y + BAR_Y_OFF, BAR_W, BAR_H, C_TRACK);
  int fw = (int)(BAR_W * (u.pct / 100.f) + 0.5f);
  if (fw < 0)
    fw = 0;
  if (fw > BAR_W)
    fw = BAR_W;
  if (fw > 0)
    g.fillRect(BAR_X, state.y + BAR_Y_OFF, fw, BAR_H, fillColor(u.pct));

  char pct[12];
  int ip = (int)(u.pct + 0.5f);
  if (ip < 0)
    ip = 0;
  snprintf(pct, sizeof pct, "%3d%%", ip);
  text(g, pct, rightAlignX(pct, PCT_SIZE), state.y + PCT_Y_OFF, C_TEXT,
       PCT_SIZE);

  if (state.y + state.row_height < state.height)
    g.drawFastHLine(0, state.y + state.row_height - 1, state.width, C_DIV);
}
} // namespace

const uint16_t *palette() { return s_palette; }

uint16_t rgb888To565(uint32_t rgb888) {
  const uint8_t r = (rgb888 >> 16) & 0xFF;
  const uint8_t g = (rgb888 >> 8) & 0xFF;
  const uint8_t b = rgb888 & 0xFF;
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) |
                               (b >> 3));
}

bool setPaletteEntry(size_t index, uint16_t rgb565) {
  if (index >= paletteLen) {
    return false;
  }
  s_palette[index] = rgb565;
  return true;
}

bool setPaletteEntryRgb888(size_t index, uint32_t rgb888) {
  if (rgb888 > 0xFFFFFF) {
    return false;
  }
  return setPaletteEntry(index, rgb888To565(rgb888));
}

void applyPalette(TFT_eSprite &sprite) {
  for (size_t i = 0; i < paletteLen; i++) {
    sprite.setPaletteColor(static_cast<uint8_t>(i), s_palette[i]);
  }
}

void drawUsageScreen(TFT_eSPI &g, std::vector<Usage> &rows) {
  const int w = g.width();
  const int h = g.height();
  g.fillRect(0, 0, w, h, C_BG);
  const size_t n = rows.size();
  if (n == 0) {
    return;
  }
  const int rowH = h / static_cast<int>(n);
  if (rowH <= 0) {
    return;
  }
  State state = {
      .width = static_cast<uint32_t>(w),
      .height = static_cast<uint32_t>(h),
      .x = 0,
      .y = 0,
      .row_height = static_cast<uint32_t>(rowH),
  };
  for (size_t i = 0; i < n; i++) {
    state.y = static_cast<uint32_t>(i) * state.row_height;
    drawUsageRow(g, state, rows[i]);
  }
  const int used = rowH * static_cast<int>(n);
  if (used < h) {
    g.fillRect(0, used, w, h - used, C_BG);
  }
}
} // namespace usage