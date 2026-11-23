#pragma once

#include <cstdint>
#include <vector>

class QoiEncoder {
public:
  QoiEncoder(uint32_t width, uint32_t height);
  bool encodeRow(const uint8_t *rgb);
  std::vector<uint8_t> finish();

private:
  bool fits(size_t bytes) const;
  bool ensure(size_t n);
  bool append(const uint8_t *bytes, size_t n);
  void fail();

  uint32_t _width;
  uint32_t _height;
  uint32_t _rows = 0;
  size_t _cap;
  size_t _half;
  uint8_t _prev[3] = {};
  uint8_t _index[64 * 3] = {};
  uint8_t _run = 0;
  bool _failed = false;
  std::vector<uint8_t> _data;
};
