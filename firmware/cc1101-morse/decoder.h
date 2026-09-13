// ============================================================
// decoder.h — events in, letters out
// ============================================================
//
// Walks the Morse tree as symbols arrive and commits a letter
// when the gap closes. Holds no opinion about where the symbols
// came from: one instance decodes your keying, another decodes
// the radio, and neither knows the difference.
//
// That is the whole reason receive was nearly free — see
// morse_event.h.
//
// ============================================================

#ifndef DECODER_H
#define DECODER_H

#include <Arduino.h>
#include "morse_tree.h"
#include "morse_event.h"

#define DECODER_MAX_MESSAGE 120

class Decoder {
 public:
  // `label` prefixes this decoder's serial output, so a log
  // with both directions in it stays readable.
  void begin(const char* label);

  // Feed one event. Returns the letter committed by this event,
  // or MORSE_NO_LETTER if it did not finish one. A word gap
  // returns ' '.
  char feed(MorseEvent ev);

  // The node currently being walked — MORSE_ROOT between
  // letters. Useful for showing a partial letter.
  uint8_t node() const { return node_; }

  // The symbols sent so far in the letter in progress, e.g.
  // "-." while N is being formed. Empty between letters.
  const char* partial() const { return partial_; }

  const char* message() const { return message_; }
  void clearMessage();

  // Print progress as symbols arrive. On by default for the
  // key (you want to see your own fist); noisy for the radio
  // when the band is busy, so it can be turned off there.
  void setVerbose(bool on) { verbose_ = on; }

 private:
  const char* label_ = "";
  uint8_t     node_  = MORSE_ROOT;
  bool        overrun_ = false;      // more than 4 symbols sent
  bool        verbose_ = true;

  char partial_[8] = "";
  uint8_t partialLen_ = 0;

  char    message_[DECODER_MAX_MESSAGE + 1] = "";
  uint8_t messageLen_ = 0;

  void reset_();
  void append_(char c);
  char commitLetter_();
  char commitWord_();
  void applySymbol_(bool isDash);
};

#endif  // DECODER_H
