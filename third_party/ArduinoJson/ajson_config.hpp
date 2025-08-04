// Disable Arduino-specific extensions when not on an Arduino toolchain
#ifndef ARDUINO
  #define ARDUINOJSON_ENABLE_ARDUINO_STRING  0
  #define ARDUINOJSON_ENABLE_ARDUINO_STREAM  0
  #define ARDUINOJSON_ENABLE_ARDUINO_PRINT   0
  #define ARDUINOJSON_ENABLE_PROGMEM         0
#endif
