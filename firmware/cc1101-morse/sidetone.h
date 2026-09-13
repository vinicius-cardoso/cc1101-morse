// ============================================================
// sidetone.h — the buzzer
// ============================================================
//
// You cannot learn Morse silently. The rhythm is the code, and
// the ear learns it long before the eye does — so the buzzer
// follows the key directly, on while it is down, off when it
// is up.
//
// Here it does double duty. With no LED panel this is the only
// real-time output the device has, and it has to carry both
// directions at once: what you are sending and what is coming
// in. Two pitches keep them apart, which matters because the
// point of keying is not looking at a screen.
//
//   700 Hz   your own keying     (classic CW sidetone)
//   550 Hz   received signal     (low enough to never blur)
//
// The buzzer must be PASSIVE. An active one contains its own
// oscillator, plays one fixed note, and ignores tone() — which
// would collapse both pitches into one and lose the
// distinction entirely. See bom/README.md.
//
// ============================================================

#ifndef SIDETONE_H
#define SIDETONE_H

#include <Arduino.h>
#include "morse_event.h"

#define SIDETONE_PIN       20
#define SIDETONE_TX_HZ    700   // classic CW pitch, easy on the ear
#define SIDETONE_RX_HZ    550   // received signal — distinctly lower

class Sidetone {
 public:
  void begin();

  // Sound the tone for a given direction. Calling on() for one
  // source while the other is sounding does nothing: whoever
  // started first keeps the buzzer until they stop. A single
  // piezo cannot play two notes, and alternating between them
  // would sound like a fault rather than like two stations.
  void on(MorseSource src = MORSE_FROM_KEY);
  void off(MorseSource src = MORSE_FROM_KEY);

  // Stop unconditionally, whoever owns it.
  void silence();

  bool sounding() const { return sounding_; }

  void setEnabled(bool on);
  bool enabled() const { return enabled_; }

  // A short blip, used for feedback that is not a Morse symbol
  // (letter committed, mode changed).
  void blip(uint16_t freqHz, uint16_t ms);

 private:
  bool        enabled_  = true;
  bool        sounding_ = false;
  MorseSource owner_    = MORSE_FROM_KEY;   // who started the tone

  uint16_t freqFor_(MorseSource src) const {
    return (src == MORSE_FROM_RADIO) ? SIDETONE_RX_HZ : SIDETONE_TX_HZ;
  }
};

extern Sidetone sidetone;

#endif  // SIDETONE_H
