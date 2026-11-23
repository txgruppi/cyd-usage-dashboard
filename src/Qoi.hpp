#pragma once

#include <cstdint>
#include <functional>

class QoiEncoder {
public:
  using Sink = std::function<void(const uint8_t *buf, size_t len)>;
  QoiEncoder(uint32_t width, uint32_t height, Sink sink);
  bool ok() const { return !_failed; }
  bool encodeRow(const uint8_t *rgb);
  bool finish();

private:
  void emit(const uint8_t *buf, size_t len) { _sink(buf, len); }
  void fail() { _failed = true; }
  bool sendHeader();

  Sink _sink;
  uint32_t _width;
  uint32_t _height;
  uint32_t _rows = 0;
  uint8_t _prev[3] = {};
  uint8_t _index[64 * 3] = {};
  uint8_t _run = 0;
  bool _headerSent = false;
  bool _failed = false;
};