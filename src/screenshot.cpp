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
  }
}

void rgb888From565(uint16_t c, uint8_t *rgb) {
  const uint8_t r5 = (c >> 11) & 0x1f;
  const uint8_t g6 = (c >> 5) & 0x3f;
  const uint8_t b5 = c & 0x1f;
  rgb[0] = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
  rgb[1] = static_cast<uint8_t>((g6 << 2) | (g6 >> 4));
  rgb[2] = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
}

void takeFromSprite(TFT_eSprite &sprite, HardwareSerial &out) {
  const int width = sprite.width();
  const int height = sprite.height();
  if (width <= 0 || height <= 0) {
    out.println("screenshot: bad size");
    return;
  }

  uint8_t *row = static_cast<uint8_t *>(malloc(static_cast<size_t>(width) * 3));
  if (row == nullptr) {
    out.println("screenshot: out of memory");
    return;
  }

  QoiEncoder encoder(
      static_cast<uint32_t>(width), static_cast<uint32_t>(height),
      [&out](const uint8_t *b, size_t n) { serialWriteHex(out, b, n); });
  if (!encoder.ok()) {
    out.println("screenshot: encoder init failed");
    free(row);
    return;
  }

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      // 0xFFFF is valid (e.g. palette white), not only an OOB sentinel from
      // readPixel.
      const uint16_t c = sprite.readPixel(x, y);
      rgb888From565(c, row + x * 3);
    }
    if (!encoder.encodeRow(row)) {
      out.println();
      out.printf("screenshot: encode failed at y=%d\n", y);
      free(row);
      return;
    }
    if ((y & 15) == 0) {
      yield();
    }
  }

  free(row);
  if (!encoder.finish()) {
    out.println();
    out.println("screenshot: finish failed");
    return;
  }
  out.println();
}
} // namespace

void screenshot::take(TFT_eSprite &sprite, HardwareSerial &out) {
  takeFromSprite(sprite, out);
}