#pragma once
// Minimal stub so ArduinoJson compiles on desktop when someone forces ARDUINO

using boolean = bool;
using byte    = unsigned char;

// Some macros ArduinoJson might check
#ifndef PROGMEM
  #define PROGMEM
#endif
