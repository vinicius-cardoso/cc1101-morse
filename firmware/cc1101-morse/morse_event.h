// ============================================================
// morse_event.h — what a Morse source emits
// ============================================================
//
// A dot is a dot whether a finger made it or a carrier did.
//
// This is the whole reason the transceiver costs so little
// extra over a transmitter: the key and the radio receiver
// both produce this same event stream, so `decoder.cpp` takes
// either one and never asks which. Adding receive meant adding
// a second producer, not a second decoder.
//
// ============================================================

#ifndef MORSE_EVENT_H
#define MORSE_EVENT_H

enum MorseEvent {
  MORSE_NONE,
  MORSE_DOT,
  MORSE_DASH,
  MORSE_LETTER_GAP,   // long enough to end the letter
  MORSE_WORD_GAP      // longer still — end of a word
};

// Where an event came from. The decoder ignores this; it exists
// so the serial log and the sidetone pitch can tell you whether
// you are hearing yourself or someone else.
enum MorseSource {
  MORSE_FROM_KEY,
  MORSE_FROM_RADIO
};

#endif  // MORSE_EVENT_H
