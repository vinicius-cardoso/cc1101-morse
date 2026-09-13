// ============================================================
// radio.h — the CC1101 as a carrier switch
// ============================================================
//
// The CC1101 is normally a packet radio: you hand it bytes and
// it adds a preamble, a sync word and a CRC. None of that is
// used here.
//
// Morse on a radio is OOK — on-off keying. The carrier is
// switched on for a mark and off for a space, and the *timing
// of those switches is the message*. There is nothing to
// packetise: wrapping it in a frame would mean buffering the
// operator's keying and replaying it at the far end, which
// destroys the property that makes Morse Morse — that it is
// live. You hear the other operator's hand.
//
// So the chip runs in async transparent mode and GDO0 becomes
// the key line:
//
//   TX   firmware drives GDO0  ->  carrier follows it
//   RX   carrier drives GDO0   ->  firmware times it
//
// Same pin, same mode, opposite direction.
//
// ------------------------------------------------------------
// RUNNING WITHOUT THE MODULE
// ------------------------------------------------------------
//
// RADIO_SIMULATE defaults to 1: SPI is never touched and keying
// is logged to serial instead. The key, the decoder, the
// sidetone and the timing all work identically, so the whole
// firmware can be developed on a bare Supermini. Set it to 0
// once the CC1101 is wired.
//
// ============================================================

#ifndef RADIO_H
#define RADIO_H

#include <Arduino.h>
#include "morse_event.h"

// ------------------------------------------------------------
// Set to 1 to run without a CC1101 (logs keying to serial)
// Set to 0 once the module is wired up
// ------------------------------------------------------------
#ifndef RADIO_SIMULATE
#define RADIO_SIMULATE 1
#endif

// Pins — see docs/hardware.md
//
// These are chosen by the PCB layout, not by the chip: every SPI
// line here is bit-banged, so any GPIO will do and the assignment
// is free to be whatever routes best. It was picked by searching
// for the mapping with no crossing traces — the board is single
// layer, so a crossing means a trace that cannot be routed at all.
// See docs/decisions.md §9.
#define RADIO_PIN_MOSI   6
#define RADIO_PIN_SCK    5
#define RADIO_PIN_MISO   2
#define RADIO_PIN_CS     1
#define RADIO_PIN_GDO0   0

// GDO2 is the second configurable status pin. Nothing in the
// firmware needs it yet — keying and carrier detection both run
// on GDO0 — but it is wired because the pin was free and the
// alternative was discovering a use for it after the board was
// etched. Leave it as an input until something configures it.
//
// The obvious uses, if one is ever wanted:
//
//   0x0E   carrier sense — a hardware squelch, asserted when
//          RSSI crosses a threshold. Would let the receiver
//          reject noise before it reaches the timing code,
//          which is the weakest part of the RX path today
//          (see RADIO_RX_MIN_MARK_MS below).
//   0x06   sync word detected — useless here, no sync word.
//   0x3F   the 26 MHz clock, divided. Occasionally handy for
//          measuring the crystal.
//
// Set RADIO_GDO2_CFG to one of those to have begin() apply it.
#define RADIO_PIN_GDO2   7

// 0x2E = three-state, the safe default: the chip drives nothing
// and the pin floats as an unused input.
#define RADIO_GDO2_CFG   0x2E

// ------------------------------------------------------------
// Receive filtering
// ------------------------------------------------------------
//
// 433 MHz is a busy band — garage doors, weather stations, car
// fobs, all keying the receiver with no way to address or
// reject them. Async mode has no sync word and no CRC, so the
// only filter available is plausibility: a mark shorter than
// any real Morse symbol is noise.
//
// The fastest the key allows is 30 WPM, a 40 ms dot. Anything
// under 20 ms cannot be a deliberate symbol at any speed this
// device sends.
//
// This value is a guess until it has been tuned against the
// real noise floor — see docs/roadmap.md phase 1.
// ------------------------------------------------------------
#define RADIO_RX_MIN_MARK_MS 20

// Squelch: after this long with no activity, treat the channel
// as idle and reset the receiver's decode state, so noise from
// an hour ago is not still half-way through a letter.
#define RADIO_RX_IDLE_RESET_MS 3000

enum RadioMode {
  RADIO_MODE_IDLE,
  RADIO_MODE_TX,
  RADIO_MODE_RX
};

class Radio {
 public:
  // Returns false if the module did not answer (real hardware
  // only — simulation always succeeds). Check this: it is the
  // difference between "wired wrong" and "no signal".
  bool begin();

  bool present() const { return present_; }

  // --- transmitting -------------------------------------------

  // Key the carrier on / off. This is the whole transmit API.
  void keyDown();
  void keyUp();
  bool keyed() const { return keyed_; }

  // --- receiving ----------------------------------------------

  // Listen. Call every loop; returns one timed event at a time,
  // in the same vocabulary the key produces.
  MorseEvent poll();

  // Is a carrier present right now? (For the RX sidetone.)
  bool carrier() const { return rxMark_; }

  // The receiver adapts to the sender's speed the same way the
  // key adapts to yours, since the other operator sets the pace.
  uint16_t unitMs() const { return rxUnit_; }
  uint16_t wpm() const { return 1200 / rxUnit_; }

  // --- mode ---------------------------------------------------

  void setMode(RadioMode m);
  RadioMode mode() const { return mode_; }

  // Transmit power, as a PATABLE value. 0xC0 is +10 dBm;
  // 0x60 is around 0 dBm, which is plenty for bench work.
  void setPower(uint8_t paValue);

  // Read PARTNUM/VERSION and report. The first thing to try
  // when the module is newly wired — it isolates SPI problems
  // from radio problems.
  void dumpStatus();

 private:
  bool      present_ = false;
  bool      keyed_   = false;
  RadioMode mode_    = RADIO_MODE_IDLE;
  uint8_t   power_   = 0xC0;

  // --- RX timing state ----------------------------------------
  bool     rxMark_        = false;
  bool     rxRaw_         = false;
  uint32_t rxEdge_        = 0;   // when the current level began
  uint32_t rxMarkStart_   = 0;
  uint32_t rxSpaceStart_  = 0;

  bool     rxLetterClosed_ = true;
  bool     rxWordClosed_   = true;
  bool     rxAnySymbolYet_ = false;

  uint16_t rxUnit_ = 100;        // adaptive, like the key's

  void learnRx_(uint32_t markMs, bool wasDash);

  // --- SPI ----------------------------------------------------
  void    writeReg_(uint8_t addr, uint8_t value);
  uint8_t readReg_(uint8_t addr);
  uint8_t readStatus_(uint8_t addr);
  void    strobe_(uint8_t cmd);
  void    reset_();
  void    configure_();
  uint8_t transfer_(uint8_t b);

  void select_()   { digitalWrite(RADIO_PIN_CS, LOW);  }
  void deselect_() { digitalWrite(RADIO_PIN_CS, HIGH); }
};

extern Radio radio;

#endif  // RADIO_H
