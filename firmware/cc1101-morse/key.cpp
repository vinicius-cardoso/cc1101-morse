#include "key.h"

Key key;

// ------------------------------------------------------------

void Key::begin() {
  pinMode(KEY_PIN, INPUT_PULLUP);

  down_         = false;
  raw_          = false;
  lastChange_   = millis();
  releaseStart_ = millis();

  letterClosed_ = true;
  wordClosed_   = true;
  anySymbolYet_ = false;
}

// ------------------------------------------------------------

void Key::setUnitMs(uint16_t ms) {
  if (ms < KEY_UNIT_MIN_MS) ms = KEY_UNIT_MIN_MS;
  if (ms > KEY_UNIT_MAX_MS) ms = KEY_UNIT_MAX_MS;
  unit_ = ms;
}

// ------------------------------------------------------------
// Adaptive timing.
//
// A beginner's dots and dashes drift a lot, so rather than
// demanding you hit a fixed speed, the unit follows what you
// actually send. The pull is deliberately gentle (1/8 of the
// error) — fast enough to settle within a few letters, slow
// enough that one sloppy symbol does not throw it off.
// ------------------------------------------------------------

void Key::learnFromDot_(uint32_t ms) {
  if (!adaptive_) return;
  int32_t target = (int32_t)ms;               // a dot IS one unit
  setUnitMs((uint16_t)((int32_t)unit_ + (target - (int32_t)unit_) / 8));
}

void Key::learnFromDash_(uint32_t ms) {
  if (!adaptive_) return;
  int32_t target = (int32_t)ms / 3;           // a dash is three units
  setUnitMs((uint16_t)((int32_t)unit_ + (target - (int32_t)unit_) / 8));
}

// ------------------------------------------------------------

MorseEvent Key::poll() {
  uint32_t now = millis();

  // --- debounce ------------------------------------------------
  bool reading = (digitalRead(KEY_PIN) == LOW);   // active low

  if (reading != raw_) {
    raw_ = reading;
    lastChange_ = now;
  }

  bool settled = (now - lastChange_) >= KEY_DEBOUNCE_MS;

  // --- edges ---------------------------------------------------
  if (settled && reading != down_) {
    down_ = reading;

    if (down_) {
      pressStart_ = now;
      return MORSE_NONE;               // nothing to report yet
    }

    // Released — classify by how long it was held.
    uint32_t held = now - pressStart_;
    releaseStart_ = now;

    letterClosed_ = false;
    wordClosed_   = false;
    anySymbolYet_ = true;

    // The dot/dash boundary sits at 2 units: halfway between a
    // 1-unit dot and a 3-unit dash, which gives the most room
    // for error on both sides.
    if (held < (uint32_t)unit_ * 2) {
      learnFromDot_(held);
      return MORSE_DOT;
    } else {
      learnFromDash_(held);
      return MORSE_DASH;
    }
  }

  // --- gaps while the key is up --------------------------------
  if (!down_ && anySymbolYet_) {
    uint32_t idle = now - releaseStart_;

    // Letter gap is 3 units; fire slightly early (at 2) so the
    // letter commits promptly rather than feeling laggy.
    if (!letterClosed_ && idle >= (uint32_t)unit_ * 2) {
      letterClosed_ = true;
      return MORSE_LETTER_GAP;
    }

    // Word gap is 7 units; 6 for the same reason.
    if (letterClosed_ && !wordClosed_ && idle >= (uint32_t)unit_ * 6) {
      wordClosed_ = true;
      return MORSE_WORD_GAP;
    }
  }

  return MORSE_NONE;
}
