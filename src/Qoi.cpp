#include "Qoi.hpp"
#include <esp_heap_caps.h>

namespace {
constexpr size_t allocSlack = 16;

constexpr uint8_t opDiff = 0x40;
constexpr uint8_t opLuma = 0x80;
constexpr uint8_t opRun = 0xc0;
constexpr uint8_t opRGB = 0xfe;
constexpr uint8_t end[8] = {0, 0, 0, 0, 0, 0, 0, 1};

int slot(uint8_t r, uint8_t g, uint8_t b) {
  return (r * 3 + g * 5 + b * 7 + 255 * 11) & 63;
}

void storeBe32(uint8_t *dst, uint32_t v) {
  dst[0] = static_cast<uint8_t>(v >> 24);
  dst[1] = static_cast<uint8_t>(v >> 16);
  dst[2] = static_cast<uint8_t>(v >> 8);
  dst[3] = static_cast<uint8_t>(v);
}
} // namespace

bool QoiEncoder::fits(size_t bytes) const {
  // bad_alloc allocates from the heap that just failed. The emergency pool is
  // 0.
  return heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) >=
         bytes + allocSlack;
}

bool QoiEncoder::ensure(size_t n) {
  if (_failed) {
    return false;
  }
  if (n > _cap || _data.size() > _cap - n) {
    fail();
    return false;
  }
  if (_data.size() + n <= _data.capacity()) {
    return true;
  }
  size_t next = _data.capacity() < 2 ? _half : _data.capacity() * 2;
  if (next < _data.size() + n) {
    next = _data.size() + n;
  }
  if (next > _cap) {
    next = _cap;
  }
  if (!fits(next)) {
    fail();
    return false;
  }
  _data.reserve(next);
  return true;
}

bool QoiEncoder::append(const uint8_t *bytes, size_t n) {
  if (!ensure(n)) {
    return false;
  }
  _data.insert(_data.end(), bytes, bytes + n);
  return true;
}

void QoiEncoder::fail() {
  _failed = true;
  _data.clear();
}

QoiEncoder::QoiEncoder(uint32_t width, uint32_t height)
    : _width(width), _height(height) {
  _cap = width * height * 4 + 22;
  _half = _cap / 2;
  if (!fits(_half)) {
    _failed = true;
    return;
  }
  _data.reserve(_half);
  uint8_t hdr[14];
  hdr[0] = 'q';
  hdr[1] = 'o';
  hdr[2] = 'i';
  hdr[3] = 'f';
  storeBe32(hdr + 4, width);
  storeBe32(hdr + 8, height);
  hdr[12] = 3;
  hdr[13] = 0;
  if (!append(hdr, sizeof hdr)) {
    fail();
  }
}

bool QoiEncoder::encodeRow(const uint8_t *rgb) {
  if (_failed || rgb == nullptr || _rows >= _height) {
    fail();
    return false;
  }
  for (uint32_t x = 0; x < _width; x++) {
    const uint8_t r = rgb[x * 3];
    const uint8_t g = rgb[x * 3 + 1];
    const uint8_t b = rgb[x * 3 + 2];
    if (r == _prev[0] && g == _prev[1] && b == _prev[2]) {
      _run++;
      if (_run == 62) {
        const uint8_t op = static_cast<uint8_t>(opRun | (_run - 1));
        _run = 0;
        if (!append(&op, 1)) {
          return false;
        }
      }
      continue;
    }
    if (_run > 0) {
      const uint8_t op = static_cast<uint8_t>(opRun | (_run - 1));
      _run = 0;
      if (!append(&op, 1)) {
        return false;
      }
    }
    const int index = slot(r, g, b);
    uint8_t *seen = _index + index * 3;
    if (seen[0] == r && seen[1] == g && seen[2] == b) {
      const uint8_t op = static_cast<uint8_t>(index);
      if (!append(&op, 1)) {
        return false;
      }
    } else {
      seen[0] = r;
      seen[1] = g;
      seen[2] = b;
      const int8_t vr = static_cast<int8_t>(static_cast<uint8_t>(r - _prev[0]));
      const int8_t vg = static_cast<int8_t>(static_cast<uint8_t>(g - _prev[1]));
      const int8_t vb = static_cast<int8_t>(static_cast<uint8_t>(b - _prev[2]));
      const int8_t vg_r = static_cast<int8_t>(static_cast<uint8_t>(vr - vg));
      const int8_t vg_b = static_cast<int8_t>(static_cast<uint8_t>(vb - vg));
      if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2) {
        const uint8_t op = static_cast<uint8_t>(opDiff | ((vr + 2) << 4) |
                                                ((vg + 2) << 2) | (vb + 2));
        if (!append(&op, 1)) {
          return false;
        }
      } else if (vg_r > -9 && vg_r < 8 && vg > -33 && vg < 32 && vg_b > -9 &&
                 vg_b < 8) {
        const uint8_t op[2] = {
            static_cast<uint8_t>(opLuma | (vg + 32)),
            static_cast<uint8_t>(((vg_r + 8) << 4) | (vg_b + 8))};
        if (!append(op, 2)) {
          return false;
        }
      } else {
        const uint8_t op[4] = {opRGB, r, g, b};
        if (!append(op, 4)) {
          return false;
        }
      }
    }
    _prev[0] = r;
    _prev[1] = g;
    _prev[2] = b;
  }
  _rows++;
  return true;
}

std::vector<uint8_t> QoiEncoder::finish() {
  if (_failed || _rows != _height) {
    fail();
    return {};
  }
  if (_run > 0) {
    const uint8_t op = static_cast<uint8_t>(opRun | (_run - 1));
    _run = 0;
    if (!append(&op, 1)) {
      return {};
    }
  }
  if (!append(end, sizeof end)) {
    return {};
  }
  if (_data.capacity() > _data.size() && fits(_data.size())) {
    _data.shrink_to_fit();
  }
  return std::move(_data);
}
