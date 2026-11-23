#include "UsageScreen.hpp"

#include <stdio.h>
#include <string.h>

namespace usage {

namespace {
struct Layout {
  int n = 0;
  int rowH = 0;
  uint8_t nameFont = 1, nameSize = 2;
  uint8_t dateFont = 1, dateSize = 1;
  uint8_t pctFont = 1, pctSize = 2;
  int nameY = 0, dateY = 0, pctY = 0;
  int barX = 12, barY = 0, barW = 240, barH = 10;
  int marginX = 12, right = 308, textGap = 8;
  bool pctBelow = false;
  int nameRight = 308;
};

// RGB565 defaults. Shown (bit-expanded) hex is listed in UsageScreen.hpp.
uint16_t s_palette[12] = {
    0x10A4, //  0 C_BG
    0x31EA, //  1 C_TRACK
    0x2188, //  2 C_DIV
    0x4C5D, //  3 C_BLUE        bar < 75%
    0xA5D9, //  4 C_MUTED
    0xF7BF, //  5 C_TEXT
    0x3E6F, //  6 C_GOOD
    0xF5A0, //  7 C_WARN        bar >= 75%
    0xE208, //  8 C_DANGER      bar >= 90%
    0x5712, //  9 C_RESET_SOON  reset <= 2 days
    0x661F, // 10 C_RESET_WEEK  reset <= 7 days
    0xA5D9, // 11 C_RESET_FAR   reset  > 7 days
};

constexpr int MARGIN_X = 12, RIGHT = 308, TEXT_GAP = 8;
constexpr int SCREEN_W = 320, SCREEN_H = 240;
constexpr int MIN_ROW_H = 20; // compact size-1 floor
constexpr float WARN_AT = 75.f, DANGER_AT = 90.f;

int glyphH(uint8_t font, int size) {
  if (font == 1)
    return 8 * size;
  if (font == 2)
    return 16 * size;
  if (font == 4)
    return 26 * size;
  return 8 * size;
}

int inkWidth(const char *s, int size) {
  int n = s ? (int)strlen(s) : 0;
  return n ? (6 * size * n - size) : 0;
}

int textW(TFT_eSPI &g, const char *s, uint8_t font, int size) {
  if (!s || !s[0])
    return 0;
  if (font == 1)
    return inkWidth(s, size);
  g.setTextFont(font);
  g.setTextSize(size);
  return g.textWidth(s);
}

int rightAlignX(TFT_eSPI &g, const char *s, uint8_t font, int size, int right) {
  int w = textW(g, s, font, size);
  return w ? (right - (w - 1)) : right;
}

int barWFor(TFT_eSPI &g, uint8_t pctFont, int pctSize) {
  const int pctW = textW(g, "112%", pctFont, pctSize);
  return (RIGHT - (pctW - 1)) - TEXT_GAP - MARGIN_X;
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
    return C_RESET_SOON;
  if (days <= 7)
    return C_RESET_WEEK;
  return C_RESET_FAR;
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

void fitName(char *buf, size_t len, const char *name, int avail, int size) {
  if (!name)
    name = "";
  if (inkWidth(name, size) <= avail) {
    snprintf(buf, len, "%s", name);
    return;
  }
  int maxChars = (avail + size) / (6 * size);
  if (maxChars < 2) {
    if (inkWidth(".", size) <= avail)
      snprintf(buf, len, ".");
    else
      buf[0] = 0;
    return;
  }
  snprintf(buf, len, "%.*s..", maxChars - 2, name);
}

void text(TFT_eSPI &g, const char *s, int x, int y, uint16_t c, uint8_t font,
          int size) {
  g.setTextFont(font);
  g.setTextSize(size);
  g.setTextDatum(TL_DATUM);
  g.setTextColor(c, C_BG);
  g.drawString(s, x, y);
}

int rowHeight(int n, int i, int h) {
  const int base = h / n;
  const int extra = h % n;
  return base + (i < extra ? 1 : 0);
}

int rowY(int n, int i, int h) {
  const int base = h / n;
  const int extra = h % n;
  return i * base + (i < extra ? i : extra);
}

// Drop name size 3 -> 2 if any label would truncate at the given right edge
// (used when the date is not on the name line).
int nameSizeAgainstRight(const std::vector<Usage> &rows, int n, int defSize,
                         int nameRight) {
  const int avail = nameRight - MARGIN_X + 1;
  for (int i = 0; i < n; i++) {
    if (inkWidth(rows[i].name, defSize) > avail)
      return defSize - 1;
  }
  return defSize;
}

// Drop name size 3 -> 2 if any label would overlap its date on the same line.
int nameSizeBesideDate(const std::vector<Usage> &rows, int n, int defSize,
                       int dateSize) {
  for (int i = 0; i < n; i++) {
    char dateBuf[12];
    const char *date = formatReset(dateBuf, sizeof dateBuf, rows[i].resetDays)
                           ? dateBuf
                           : nullptr;
    int avail;
    if (date) {
      const int dw = inkWidth(date, dateSize);
      const int dx = dw ? (RIGHT - (dw - 1)) : RIGHT;
      avail = dx - TEXT_GAP - MARGIN_X;
    } else {
      avail = RIGHT - MARGIN_X + 1;
    }
    if (inkWidth(rows[i].name, defSize) > avail)
      return defSize - 1;
  }
  return defSize;
}

Layout makeLayout(TFT_eSPI &g, int n, int rowH, const std::vector<Usage> &rows,
                  int shown) {
  Layout L;
  L.n = n;
  L.rowH = rowH;
  L.marginX = MARGIN_X;
  L.right = RIGHT;
  L.textGap = TEXT_GAP;
  L.nameRight = RIGHT;

  if (n == 1) {
    // Hero: size-3 name on its own line, Font4 % with the reset under a wide
    // bar.
    L.nameFont = 1;
    L.nameSize = 3;
    L.dateFont = 1;
    L.dateSize = 2;
    L.pctFont = 4;
    L.pctSize = 1;
    L.nameRight = SCREEN_W - 4; // 316, 4 px margin
    L.nameSize = nameSizeAgainstRight(rows, shown, L.nameSize, L.nameRight);
    L.pctBelow = true;
    L.barH = 20;
    L.barX = MARGIN_X;
    L.barW = RIGHT - L.barX + 1;
    {
      const int nameH = glyphH(L.nameFont, L.nameSize);
      const int dateH = glyphH(L.dateFont, L.dateSize);
      const int pctH = glyphH(L.pctFont, L.pctSize);
      const int gap1 = 28, gap2 = 24;
      const int block = nameH + gap1 + L.barH + gap2 + pctH;
      const int top = (rowH - block) / 2;
      L.nameY = top;
      L.barY = L.nameY + nameH + gap1;
      L.pctY = L.barY + L.barH + gap2;
      L.dateY = L.pctY + (pctH - dateH) / 2;
    }
    return L;
  }

  if (n == 2) {
    // Two stacked cards, same language as the hero.
    L.nameFont = 1;
    L.nameSize = 3;
    L.dateFont = 1;
    L.dateSize = 2;
    L.pctFont = 4;
    L.pctSize = 1;
    L.nameRight = SCREEN_W - 4;
    L.nameSize = nameSizeAgainstRight(rows, shown, L.nameSize, L.nameRight);
    L.pctBelow = true;
    L.barH = 16;
    L.barX = MARGIN_X;
    L.barW = RIGHT - L.barX + 1;
    {
      const int nameH = glyphH(L.nameFont, L.nameSize);
      const int dateH = glyphH(L.dateFont, L.dateSize);
      const int pctH = glyphH(L.pctFont, L.pctSize);
      const int gap1 = 18, gap2 = 14;
      const int block = nameH + gap1 + L.barH + gap2 + pctH;
      const int top = (rowH - block) / 2;
      L.nameY = top;
      L.barY = L.nameY + nameH + gap1;
      L.pctY = L.barY + L.barH + gap2;
      L.dateY = L.pctY + (pctH - dateH) / 2;
    }
    return L;
  }

  if (n == 3) {
    L.nameFont = 1;
    L.nameSize = nameSizeBesideDate(rows, shown, 3, 2);
    L.dateFont = 1;
    L.dateSize = 2;
    L.pctFont = 1;
    L.pctSize = 3;
    L.barH = 12;
    L.barX = MARGIN_X;
    L.barW = barWFor(g, L.pctFont, L.pctSize);
    {
      const int nameH = glyphH(L.nameFont, L.nameSize);
      const int dateH = glyphH(L.dateFont, L.dateSize);
      const int pctH = glyphH(L.pctFont, L.pctSize);
      const int gapy = 10;
      const int block = nameH + gapy + pctH;
      const int top = (rowH - block) / 2;
      L.nameY = top;
      L.dateY = L.nameY + (nameH - dateH) / 2;
      L.pctY = L.nameY + nameH + gapy;
      L.barY = L.pctY + (pctH - L.barH) / 2;
    }
    return L;
  }

  if (n <= 6) {
    // Confirmed typography: GLCD size 2 name/%, size 1 date, 10 px bar.
    // n=4 at 60 px keeps the exact on-device offsets; bar width stays 240.
    L.nameFont = 1;
    L.nameSize = 2;
    L.dateFont = 1;
    L.dateSize = 1;
    L.pctFont = 1;
    L.pctSize = 2;
    L.barH = 10;
    L.barX = MARGIN_X;
    L.barW = 240;
    if (n == 4 && rowH == 60) {
      L.nameY = 14;
      L.dateY = 18;
      L.pctY = 33;
      L.barY = 36;
    } else {
      const int nameH = glyphH(L.nameFont, L.nameSize);
      const int dateH = glyphH(L.dateFont, L.dateSize);
      const int pctH = glyphH(L.pctFont, L.pctSize);
      const int gapy = 4;
      const int block = nameH + gapy + pctH;
      const int top = (rowH - block) / 2;
      L.nameY = top;
      L.dateY = L.nameY + (nameH - dateH) / 2;
      L.pctY = L.nameY + nameH + gapy;
      L.barY = L.pctY + (pctH - L.barH) / 2;
    }
    return L;
  }

  // Compact: everything GLCD size 1, two tight lines.
  L.nameFont = 1;
  L.nameSize = 1;
  L.dateFont = 1;
  L.dateSize = 1;
  L.pctFont = 1;
  L.pctSize = 1;
  L.barH = (n <= 8) ? 6 : 4;
  L.barX = MARGIN_X;
  L.barW = barWFor(g, L.pctFont, L.pctSize);
  {
    const int nameH = 8, dateH = 8, pctH = 8;
    const int leftover = rowH - nameH - pctH;
    int gapy = leftover / 3;
    if (gapy < 2)
      gapy = 2;
    if (gapy > 6)
      gapy = 6;
    const int block = nameH + gapy + pctH;
    const int top = (rowH - block) / 2;
    L.nameY = top;
    L.dateY = L.nameY + (nameH - dateH) / 2;
    L.pctY = L.nameY + nameH + gapy;
    L.barY = L.pctY + (pctH - L.barH) / 2;
  }
  return L;
}

void drawUsageRow(TFT_eSPI &g, int y, const Layout &L, const Usage &u,
                  bool last) {
  g.fillRect(0, y, g.width(), L.rowH, C_BG);

  char dateBuf[12];
  const char *date =
      formatReset(dateBuf, sizeof dateBuf, u.resetDays) ? dateBuf : nullptr;

  char name[32];
  if (L.pctBelow) {
    const int avail = L.nameRight - L.marginX + 1;
    fitName(name, sizeof name, u.name, avail, L.nameSize);
    text(g, name, L.marginX, y + L.nameY, C_TEXT, L.nameFont, L.nameSize);
    if (date) {
      text(g, date, L.marginX, y + L.dateY, resetColor(u.resetDays), L.dateFont,
           L.dateSize);
    }
  } else {
    int avail;
    int dx = 0;
    if (date) {
      dx = rightAlignX(g, date, L.dateFont, L.dateSize, L.right);
      avail = dx - L.textGap - L.marginX;
    } else {
      avail = L.right - L.marginX + 1;
    }
    fitName(name, sizeof name, u.name, avail, L.nameSize);
    text(g, name, L.marginX, y + L.nameY, C_TEXT, L.nameFont, L.nameSize);
    if (date) {
      text(g, date, dx, y + L.dateY, resetColor(u.resetDays), L.dateFont,
           L.dateSize);
    }
  }

  g.fillRect(L.barX, y + L.barY, L.barW, L.barH, C_TRACK);
  int fw = (int)(L.barW * (u.pct / 100.f) + 0.5f);
  if (fw < 0)
    fw = 0;
  if (fw > L.barW)
    fw = L.barW;
  if (fw > 0)
    g.fillRect(L.barX, y + L.barY, fw, L.barH, fillColor(u.pct));

  char pct[12];
  int ip = (int)(u.pct + (u.pct >= 0.f ? 0.5f : -0.5f));
  if (ip < 0)
    ip = 0;
  snprintf(pct, sizeof pct, "%3d%%", ip);
  text(g, pct, rightAlignX(g, pct, L.pctFont, L.pctSize, L.right), y + L.pctY,
       C_TEXT, L.pctFont, L.pctSize);

  if (!last)
    g.drawFastHLine(0, y + L.rowH - 1, g.width(), C_DIV);
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

void applyPalette(TFT_eSprite &sprite) {
  for (size_t i = 0; i < paletteLen; i++) {
    sprite.setPaletteColor(static_cast<uint8_t>(i), s_palette[i]);
  }
}

void drawUsageScreen(TFT_eSPI &g, std::vector<Usage> &rows) {
  const int w = g.width();
  const int h = g.height();
  g.fillRect(0, 0, w, h, C_BG);
  int n = (int)rows.size();
  if (n <= 0)
    return;
  int cap = h / MIN_ROW_H;
  if (cap > N_MAX)
    cap = N_MAX;
  if (cap < 1)
    cap = 1;
  if (n > cap)
    n = cap;
  for (int i = 0; i < n; i++) {
    const int y = rowY(n, i, h);
    const int rh = rowHeight(n, i, h);
    if (rh <= 0)
      return;
    Layout L = makeLayout(g, n, rh, rows, n);
    const bool last = (i == n - 1) || (y + rh >= h);
    drawUsageRow(g, y, L, rows[static_cast<size_t>(i)], last);
  }
}
} // namespace usage
