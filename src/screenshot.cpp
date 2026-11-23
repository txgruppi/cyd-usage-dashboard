#include "screenshot.hpp"

#include <Arduino.h>
#include <stdio.h>

#include "Qoi.hpp"

namespace {
void serialWriteHex(HardwareSerial &out, const uint8_t *data, size_t len) {
  char pair[3] = "00";
  for (size_t i = 0; i < len; i++) {
    snprintf(pair, sizeof pair, "%02x", (unsigned)data[i]);
    while (out.availableForWrite() < 2) {
      yield();
    }
    out.write(pair, 2);
    if ((i & 0xff) == 0) {
      yield();
    }
  }
}

void _take(TFT_eSPI &tft, HardwareSerial &out, uint32_t width, uint32_t height,
           uint16_t *row565, uint8_t *row) {
  QoiEncoder encoder(width, height);
  for (int y = 0; y < height; y++) {
    tft.readRect(0, y, width, 1, row565);
    for (int x = 0; x < width; x++) {
      // readRect returns byte-swapped 565 (pushRect order).
      uint16_t c = __builtin_bswap16(row565[x]);
      uint8_t r = (c >> 11) & 0x1f;
      uint8_t g = (c >> 5) & 0x3f;
      uint8_t b = c & 0x1f;
      row[x * 3] = (uint8_t)((r << 3) | (r >> 2));
      row[x * 3 + 1] = (uint8_t)((g << 2) | (g >> 4));
      row[x * 3 + 2] = (uint8_t)((b << 3) | (b >> 2));
    }
    if (!encoder.encodeRow(row)) {
      out.printf("screenshot: encode failed at y=%d\n", y);
      return;
    }
    if ((y & 15) == 0) {
      yield();
    }
  }
  std::vector<uint8_t> data = encoder.finish();
  if (data.empty()) {
    out.println("screenshot: empty");
    return;
  }
  serialWriteHex(out, data.data(), data.size());
  out.println();
}
} // namespace

void screenshot::take(TFT_eSPI &tft, HardwareSerial &out) {
  auto width = tft.width();
  auto height = tft.height();
  uint16_t *row565 = (uint16_t *)malloc(width * sizeof(uint16_t));
  if (row565 == nullptr) {
    out.println("screenshot: out of memory");
    return;
  }
  uint8_t *row = (uint8_t *)malloc(width * 3 * sizeof(uint8_t));
  if (row == nullptr) {
    out.println("screenshot: out of memory");
    free(row565);
    return;
  }
  _take(tft, out, width, height, row565, row);
  free(row565);
  free(row);
}
