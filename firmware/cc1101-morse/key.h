// ============================================================
// key.h — reading the Morse key
// ============================================================
//
// Turns button presses into dots, dashes and gaps.
//
// Morse timing is all relative to one unit, the dot length:
//
//   dot           1 unit
//   dash          3 units
//   symbol gap    1 unit   (between symbols in a letter)
//   letter gap    3 units  (between letters)
//   word gap      7 units  (between words)
//
// So the only real question is "how long is your dot?", and
// everything else follows. The unit is measured as you send
// rather than making you match a fixed speed — see below.
//
// The radio receiver emits the same event vocabulary (see
// morse_event.h), so a mark from the key and a mark from the air
// are handled by exactly the same code downstream.
//
// ============================================================

#ifndef KEY_H
#define KEY_H

#include <Arduino.h>
#include "morse_event.h"

#define KEY_PIN 10

// Starting dot length in ms (~12 WPM). Adapts from here.
#define KEY_UNIT_DEFAULT_MS 100

// Bounds on the adaptive unit, so a stuck key or a very long
// deliberate pause cannot drag the timing somewhere useless.
#define KEY_UNIT_MIN_MS  40    // ~30 WPM
#define KEY_UNIT_MAX_MS 400    // ~3 WPM

#define KEY_DEBOUNCE_MS 15

class Key {
 public:
  void begin();

  // Call every loop. Returns one event at a time.
  MorseEvent poll();

  bool isDown() const { return down_; }

  // Current dot length in milliseconds.
  uint16_t unitMs() const { return unit_; }

  // Words per minute, derived from the unit length.
  // PARIS is the standard 50-unit reference word.
  uint16_t wpm() const { return 1200 / unit_; }

  // Fixed timing instead of adaptive, if you want to practise
  // against a set speed rather than your own.
  void setAdaptive(bool on) { adaptive_ = on; }
  bool adaptive() const { return adaptive_; }
  void setUnitMs(uint16_t ms);

 private:
  bool     down_          = false;
  bool     raw_           = false;
  uint32_t lastChange_    = 0;   // debounce timer
  uint32_t pressStart_    = 0;
  uint32_t releaseStart_  = 0;

  bool     letterClosed_  = true;  // letter gap already reported?
  bool     wordClosed_    = true;  // word gap already reported?
  bool     anySymbolYet_  = false; // suppress gaps before the first symbol

  uint16_t unit_          = KEY_UNIT_DEFAULT_MS;
  bool     adaptive_      = true;

  void learnFromDot_(uint32_t ms);
  void learnFromDash_(uint32_t ms);
};

extern Key key;

#endif  // KEY_H
