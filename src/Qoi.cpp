#include "Qoi.hpp"

namespace {
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

QoiEncoder::QoiEncoder(uint32_t width, uint32_t height, Sink sink)
    : _sink(std::move(sink)), _width(width), _height(height) {
  if (!_sink) {
    _failed = true;
  }
}

bool QoiEncoder::sendHeader() {
  if (_headerSent) {
    return true;
  }
  uint8_t hdr[14];
  hdr[0] = 'q';
  hdr[1] = 'o';
  hdr[2] = 'i';
  hdr[3] = 'f';
  storeBe32(hdr + 4, _width);
  storeBe32(hdr + 8, _height);
  hdr[12] = 3;
  hdr[13] = 0;
  emit(hdr, sizeof hdr);
  _headerSent = true;
  return true;
}

bool QoiEncoder::encodeRow(const uint8_t *rgb) {
  if (_failed || rgb == nullptr || _rows >= _height) {
    fail();
    return false;
  }
  if (!sendHeader()) {
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
        emit(&op, 1);
      }
      continue;
    }
    if (_run > 0) {
      const uint8_t op = static_cast<uint8_t>(opRun | (_run - 1));
      _run = 0;
      emit(&op, 1);
    }
    const int index = slot(r, g, b);
    uint8_t *seen = _index + index * 3;
    if (seen[0] == r && seen[1] == g && seen[2] == b) {
      const uint8_t op = static_cast<uint8_t>(index);
      emit(&op, 1);
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
        emit(&op, 1);
      } else if (vg_r > -9 && vg_r < 8 && vg > -33 && vg < 32 && vg_b > -9 &&
                 vg_b < 8) {
        const uint8_t op[2] = {
            static_cast<uint8_t>(opLuma | (vg + 32)),
            static_cast<uint8_t>(((vg_r + 8) << 4) | (vg_b + 8))};
        emit(op, 2);
      } else {
        const uint8_t op[4] = {opRGB, r, g, b};
        emit(op, 4);
      }
    }
    _prev[0] = r;
    _prev[1] = g;
    _prev[2] = b;
  }
  _rows++;
  return true;
}

bool QoiEncoder::finish() {
  if (_failed || _rows != _height) {
    fail();
    return false;
  }
  if (_run > 0) {
    const uint8_t op = static_cast<uint8_t>(opRun | (_run - 1));
    _run = 0;
    emit(&op, 1);
  }
  emit(end, sizeof end);
  return true;
}