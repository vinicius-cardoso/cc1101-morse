// ============================================================
// morse_tree.h — the Morse tree, and where each letter lives
// ============================================================
//
// Decoding is a walk, not a table lookup. Start at the root;
// a dot steps left, a dash steps right. Wherever you stop is
// the letter.
//
// The same walk serves both directions: symbols arriving from
// the key and symbols arriving from the radio are the same
// thing once they have been timed, so there is one decoder and
// one tree, not two of each.
//
// ============================================================

#ifndef MORSE_TREE_H
#define MORSE_TREE_H

#include <Arduino.h>

// ------------------------------------------------------------
// Tree geometry
// ------------------------------------------------------------
//
// Nodes are numbered breadth-first, root = 0:
//
//            0 (root, no letter)
//           .   -
//         1       2        <- E T
//        . -     . -
//       3   4   5   6      <- I A N M
//       ...
//
// (edges labelled with the symbol that takes you down them)
//
// For node n:  dot child  = 2n + 1
//              dash child = 2n + 2
//
// Four levels of letters need nodes 1..30.
//
// ------------------------------------------------------------

#define MORSE_ROOT        0
#define MORSE_NODE_COUNT  31   // 0..30
#define MORSE_NO_LETTER   '\0'

// Node -> letter. Index is the node number above.
// '\0' marks the four slots that carry no English letter
// (..-- .-.- ---. ----), plus the root itself.
static const char MORSE_NODE_LETTER[MORSE_NODE_COUNT] = {
  MORSE_NO_LETTER,            // 0  root
  'E', 'T',                   // 1..2    level 1
  'I', 'A', 'N', 'M',         // 3..6    level 2
  'S', 'U', 'R', 'W',         // 7..10   level 3
  'D', 'K', 'G', 'O',         // 11..14
  'H', 'V', 'F', MORSE_NO_LETTER,   // 15..18  level 4
  'L', MORSE_NO_LETTER, 'P', 'J',   // 19..22
  'B', 'X', 'C', 'Y',               // 23..26
  'Z', 'Q', MORSE_NO_LETTER, MORSE_NO_LETTER  // 27..30
};

// ------------------------------------------------------------
// Walking the tree
// ------------------------------------------------------------

inline uint8_t morseStepDot(uint8_t node)  { return 2 * node + 1; }
inline uint8_t morseStepDash(uint8_t node) { return 2 * node + 2; }

inline bool morseNodeValid(uint8_t node) {
  return node < MORSE_NODE_COUNT;
}

inline char morseLetterAt(uint8_t node) {
  if (!morseNodeValid(node)) return MORSE_NO_LETTER;
  return MORSE_NODE_LETTER[node];
}

// How deep a node sits — level 1 is E/T, level 4 is the last
// row. Used to print the code back and to size TX timing.
inline uint8_t morseDepth(uint8_t node) {
  uint8_t d = 0;
  while (node != MORSE_ROOT && morseNodeValid(node)) {
    node = (node - 1) / 2;
    d++;
  }
  return d;
}

// ------------------------------------------------------------
// Letter -> code, for transmitting and for the serial display
// ------------------------------------------------------------

// Returns the Morse code for an uppercase letter, or nullptr.
inline const char* morseCodeFor(char c) {
  static const char* const CODES[26] = {
    ".-",   "-...", "-.-.", "-..",  ".",    "..-.",
    "--.",  "....", "..",   ".---", "-.-",  ".-..",
    "--",   "-.",   "---",  ".--.", "--.-", ".-.",
    "...",  "-",    "..-",  "...-", ".--",  "-..-",
    "-.--", "--.."
  };
  if (c < 'A' || c > 'Z') return nullptr;
  return CODES[c - 'A'];
}

// Digits, for sending a call sign or a signal report. Not part
// of the four-level tree — 0-9 are five symbols deep, so they
// decode past the tree's last row and are handled separately.
inline const char* morseCodeForDigit(char c) {
  static const char* const DIGITS[10] = {
    "-----", ".----", "..---", "...--", "....-",
    ".....", "-....", "--...", "---..", "----."
  };
  if (c < '0' || c > '9') return nullptr;
  return DIGITS[c - '0'];
}

// Any transmittable character -> its code, or nullptr.
// Lowercase is folded to uppercase; Morse has no case.
inline const char* morseCodeForChar(char c) {
  if (c >= 'a' && c <= 'z') c -= 32;
  if (c >= 'A' && c <= 'Z') return morseCodeFor(c);
  if (c >= '0' && c <= '9') return morseCodeForDigit(c);
  return nullptr;
}

#endif  // MORSE_TREE_H
