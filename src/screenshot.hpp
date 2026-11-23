#pragma once

#include <HardwareSerial.h>
#include <TFT_eSPI.h>

namespace screenshot {
void take(TFT_eSPI &tft, HardwareSerial &out);
}
