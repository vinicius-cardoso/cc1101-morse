#include "decoder.h"

// ------------------------------------------------------------

void Decoder::begin(const char* label) {
  label_ = label;
  reset_();
  clearMessage();
}

void Decoder::reset_() {
  node_       = MORSE_ROOT;
  overrun_    = false;
  partial_[0] = '\0';
  partialLen_ = 0;
}

void Decoder::clearMessage() {
  message_[0] = '\0';
  messageLen_ = 0;
}

// ------------------------------------------------------------
// The message buffer is fixed-size rather than a String: this
// runs for hours between resets and heap fragmentation on a
// microcontroller is a slow, confusing failure. When it fills,
// the oldest half is dropped — losing old text is better than
// refusing new text.
// ------------------------------------------------------------

void Decoder::append_(char c) {
  if (messageLen_ >= DECODER_MAX_MESSAGE) {
    uint8_t keep = DECODER_MAX_MESSAGE / 2;
    memmove(message_, message_ + (messageLen_ - keep), keep);
    messageLen_ = keep;
  }
  message_[messageLen_++] = c;
  message_[messageLen_]   = '\0';
}

// ------------------------------------------------------------
// A symbol arrived: step down the tree.
// ------------------------------------------------------------

void Decoder::applySymbol_(bool isDash) {
  if (partialLen_ < sizeof(partial_) - 1) {
    partial_[partialLen_++] = isDash ? '-' : '.';
    partial_[partialLen_]   = '\0';
  }

  uint8_t next = isDash ? morseStepDash(node_) : morseStepDot(node_);

  if (!morseNodeValid(next)) {
    // Past the fourth level — no English letter lives here.
    // Keep accepting symbols so the letter gap still commits,
    // but remember that the result is invalid.
    overrun_ = true;
    return;
  }

  node_ = next;

  if (verbose_) {
    Serial.print(label_);
    Serial.print(F("  "));
    Serial.print(partial_);

    char c = morseLetterAt(node_);
    if (c != MORSE_NO_LETTER) {
      Serial.print(F("  ("));
      Serial.print(c);
      Serial.println(')');
    } else {
      Serial.println(F("  (no letter here)"));
    }
  }
}

// ------------------------------------------------------------
// The letter gap closed: commit whatever we landed on.
// ------------------------------------------------------------

char Decoder::commitLetter_() {
  if (node_ == MORSE_ROOT && !overrun_) return MORSE_NO_LETTER;

  char c = overrun_ ? MORSE_NO_LETTER : morseLetterAt(node_);

  if (c != MORSE_NO_LETTER) {
    append_(c);

    Serial.print(label_);
    Serial.print(F("LETTER: "));
    Serial.print(c);
    Serial.print(F("   ["));
    Serial.print(morseCodeFor(c));
    Serial.println(']');
  } else {
    Serial.print(label_);
    Serial.print(F("LETTER: ?   ["));
    Serial.print(partial_);
    Serial.println(F("]  not a valid Morse letter"));
    append_('?');
  }

  Serial.print(label_);
  Serial.print(F("MESSAGE: "));
  Serial.println(message_);

  reset_();
  return c;
}

// ------------------------------------------------------------

char Decoder::commitWord_() {
  if (messageLen_ == 0) return MORSE_NO_LETTER;
  if (message_[messageLen_ - 1] == ' ') return MORSE_NO_LETTER;

  append_(' ');

  Serial.print(label_);
  Serial.print(F("--- word gap ---   "));
  Serial.println(message_);

  return ' ';
}

// ------------------------------------------------------------

char Decoder::feed(MorseEvent ev) {
  switch (ev) {
    case MORSE_DOT:        applySymbol_(false); break;
    case MORSE_DASH:       applySymbol_(true);  break;
    case MORSE_LETTER_GAP: return commitLetter_();
    case MORSE_WORD_GAP:   return commitWord_();
    case MORSE_NONE:       break;
  }
  return MORSE_NO_LETTER;
}
