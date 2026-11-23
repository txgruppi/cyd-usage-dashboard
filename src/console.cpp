#include "console.hpp"

namespace {
// Drop VT100-ish escapes (arrows, etc.): ESC [ … final or ESC O … final or ESC
// + final.
bool serialEscapeEat(char c) {
  static uint8_t esc = 0; // 0 normal, 1 saw ESC, 2 saw ESC [ or ESC O
  if (esc) {
    if (esc == 1) {
      esc = (c == '[' || c == 'O') ? 2 : 0;
    } else if (c >= 0x40 && c <= 0x7E) {
      esc = 0;
    }
    return true;
  }
  if (c == '\x1b') {
    esc = 1;
    return true;
  }
  return false;
}

String serialIn;

struct ReserveSerialIn {
  ReserveSerialIn() { serialIn.reserve(1024); }
};

ReserveSerialIn reserveSerialIn;
} // namespace

bool console::readLine(String &line) {
  if (!Serial.available()) {
    return false;
  }
  char c = Serial.read();
  if (serialEscapeEat(c)) {
    return false;
  }
  switch (c) {
  case '\n':
    Serial.write('\n');
    line = serialIn;
    serialIn = "";
    return true;
  case '\r':
    break;
  case '\b':
  case 0x7f:
    if (serialIn.length() > 0) {
      serialIn.remove(serialIn.length() - 1);
      Serial.print("\b \b");
    }
    break;
  default:
    serialIn += c;
    Serial.write(c);
    break;
  }
  return false;
}
