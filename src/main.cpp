#include <Arduino.h>
#include <TFT_eSPI.h>
#include <parseargs.h>
#include <stdlib.h>
#include <vector>

#include "UsageScreen.hpp"
#include "console.hpp"
#include "screenshot.hpp"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite frame(&tft);
std::vector<usage::Usage> rows;

namespace {
bool frameReady = false;

bool initFrame(uint32_t width, uint32_t height) {
  frame.setColorDepth(4);
  if (!frame.createSprite(width, height)) {
    Serial.println("error: frame alloc failed");
    return false;
  }
  frame.createPalette(usage::palette(), usage::paletteLen);
  frameReady = true;
  return true;
}

void render() {
  if (!frameReady) {
    return;
  }
  usage::drawUsageScreen(frame, rows);
  frame.pushSprite(0, 0);
}

bool parsePaletteColor(const char *s, uint16_t *out) {
  if (!s || !s[0] || !out) {
    return false;
  }
  const char *p = s;
  if (p[0] == '#') {
    p++;
  }
  char *end = nullptr;
  const unsigned long v = strtoul(p, &end, 16);
  if (end == p || (end && *end != '\0')) {
    return false;
  }
  if (v <= 0xFFFF) {
    *out = static_cast<uint16_t>(v);
    return true;
  }
  if (v <= 0xFFFFFF) {
    *out = usage::rgb888To565(static_cast<uint32_t>(v));
    return true;
  }
  return false;
}

void paletteApplied() {
  if (frameReady) {
    usage::applyPalette(frame);
    render();
  }
}

void paletteDump() {
  const uint16_t *p = usage::palette();
  for (size_t i = 0; i < usage::paletteLen; i++) {
    Serial.printf("%2u 0x%04X\n", (unsigned)i, p[i]);
  }
}
} // namespace

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  initFrame(tft.width(), tft.height());
  render();
}

void loop() {
  String line;
  if (!console::readLine(line)) {
    return;
  }
  char **argv = nullptr;
  int argc = parseargs_parse(line.c_str(), &argv);
  switch (argc) {
  case PARSEARGS_ERR_INVALID_ARGUMENT:
    Serial.println("error: parseargs: invalid argument");
    return;

  case PARSEARGS_ERR_INVALID_SYNTAX:
    Serial.println("error: parseargs: invalid syntax");
    return;

  case PARSEARGS_ERR_UNEXPECTED_END_OF_INPUT:
    Serial.println("error: parseargs: unexpected end of input");
    return;

  case PARSEARGS_ERR_OUT_OF_MEMORY:
    Serial.println("error: parseargs: too many arguments");
    return;
  }

  switch (argc) {
  case 0:
    Serial.println("error: no command");
    break;

  case 1:
    if (strcmp(argv[0], "screenshot") == 0) {
      screenshot::take(tft, Serial);
    } else if (strcmp(argv[0], "mem") == 0) {
      Serial.printf("free=%lu maxalloc=%lu psram=%d\n",
                    (unsigned long)ESP.getFreeHeap(),
                    (unsigned long)ESP.getMaxAllocHeap(), psramFound());
    } else {
      Serial.println("error: unknown command");
    }
    break;

  case 2:
    if (strcmp(argv[0], "palette") == 0 && strcmp(argv[1], "dump") == 0) {
      paletteDump();
    } else if (strcmp(argv[0], "usage") == 0) {
      if (strcmp(argv[1], "reset") == 0) {
        rows.clear();
      } else if (strcmp(argv[1], "show") == 0) {
        render();
      } else {
        Serial.println("error: unknown command");
        return;
      }
    } else {
      Serial.println("error: unknown command");
    }
    break;

  case 4:
    if (strcmp(argv[0], "palette") == 0 && strcmp(argv[1], "set") == 0) {
      int index = atoi(argv[2]);
      uint16_t rgb565 = 0;
      if (index < 0 || index >= (int)usage::paletteLen ||
          !parsePaletteColor(argv[3], &rgb565) ||
          !usage::setPaletteEntry(static_cast<size_t>(index), rgb565)) {
        Serial.println("error: palette set");
        break;
      }
      paletteApplied();
    } else {
      Serial.println("error: unknown command");
    }
    break;

  case 17:
    if (strcmp(argv[0], "palette") == 0 && strcmp(argv[1], "load") == 0) {
      bool ok = true;
      for (int i = 0; i < (int)usage::paletteLen; i++) {
        uint16_t rgb565 = 0;
        if (!parsePaletteColor(argv[i + 2], &rgb565) ||
            !usage::setPaletteEntry(static_cast<size_t>(i), rgb565)) {
          ok = false;
          break;
        }
      }
      if (!ok) {
        Serial.println("error: palette load");
      } else {
        paletteApplied();
      }
    } else {
      Serial.println("error: unknown command");
    }
    break;

  case 3:
    if (strcmp(argv[0], "usage") == 0 && strcmp(argv[1], "resize") == 0) {
      int newSize = atoi(argv[2]);
      if (newSize < 0) {
        Serial.println("error: new size out of range");
        return;
      }
      rows.resize(static_cast<size_t>(newSize));
    }
    break;

  case 6:
    if (strcmp(argv[0], "usage") == 0 && strcmp(argv[1], "set") == 0) {
      int index = atoi(argv[2]);
      int days = atoi(argv[3]);
      float pct = atof(argv[4]);
      const char *name = argv[5];
      if (index < 0) {
        Serial.println("error: index out of range");
        return;
      }
      if (index >= rows.size()) {
        rows.resize(index + 1);
      }
      rows[index].setName(name);
      rows[index].pct = pct;
      rows[index].resetDays = days;
    }
    break;

  default:
    Serial.println("error: unknown command");
    break;
  }
  parseargs_free(argv);
}