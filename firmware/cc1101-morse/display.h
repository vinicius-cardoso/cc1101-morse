// ============================================================
// display.h — optional SSD1306 OLED
// ============================================================
//
// A 128x64 I2C OLED on GPIO8/GPIO9, showing what you are
// sending and what is coming in. It is the difference between
// needing a laptop attached and not.
//
// ------------------------------------------------------------
// IT IS NOT ON THE PCB
// ------------------------------------------------------------
//
// The board has no OLED footprint — docs/decisions.md §1 chose
// no display, and §7 kept GPIO8/GPIO9 free precisely so one
// could be wired later. This code serves that: on a breadboard
// the OLED works, and on the bare PCB it is simply absent.
//
// Absence is handled at runtime, not by a #define. begin()
// probes the I2C bus; if nothing answers at 0x3C, every other
// call becomes a no-op and the firmware carries on with the
// buzzer and the serial monitor exactly as before. Nothing
// blocks, nothing waits, nothing needs recompiling.
//
// So one binary runs on both, and plugging a display into a
// finished board is a thing you can just do.
//
// ------------------------------------------------------------
// COMPILING WITHOUT THE LIBRARIES
// ------------------------------------------------------------
//
// Needs Adafruit_SSD1306 and Adafruit_GFX. If they are not
// installed, set DISPLAY_ENABLE to 0 and the whole module
// compiles away to nothing — the sketch still builds and runs.
//
// ============================================================

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "morse_event.h"

// ------------------------------------------------------------
// Set to 0 to build without the Adafruit libraries
// ------------------------------------------------------------
#ifndef DISPLAY_ENABLE
#define DISPLAY_ENABLE 1
#endif

// I2C pins — the ESP32-C3 defaults, kept free for this
#define DISPLAY_SDA_PIN  8
#define DISPLAY_SCL_PIN  9

#define DISPLAY_ADDR     0x3C
#define DISPLAY_WIDTH    128
#define DISPLAY_HEIGHT   64

// How much of the message tail to keep on screen
#define DISPLAY_LINE_CHARS 21   // 128 px / 6 px per char

// ------------------------------------------------------------
// Minimum gap between physical redraws.
//
// render() pushes the whole 1 KB framebuffer over I2C, which at
// 400 kHz takes roughly 20 ms. Calling it per event is fine when
// events are dots and dashes from a hand; it is fatal when they
// arrive from a floating GDO0 pin picking up noise, because the
// redraws then starve the loop and the key is never polled.
//
// 50 ms is faster than an eye notices and slow enough that even a
// pathological event rate cannot monopolise the CPU.
// ------------------------------------------------------------
#define DISPLAY_MIN_REDRAW_MS 50

class Display {
 public:
  // Probes the bus. Returns false if no OLED answered — which
  // is not an error, just the PCB's normal state.
  bool begin();

  bool present() const { return present_; }

  // The symbols of the letter being formed right now (".-.."),
  // plus which source is sending them.
  void showPartial(const char* symbols, MorseSource src);

  // A letter was committed. Appends to the running message.
  void showLetter(char c, MorseSource src);

  // The full message so far, for redrawing after a mode change.
  void setMessage(const char* tx, const char* rx);

  // Status line: speed, TX/RX mode, radio presence.
  void setStatus(const char* text);

  // Splash at startup.
  void splash(const char* line1, const char* line2);

  // Push the buffer to the panel, at most every
  // DISPLAY_MIN_REDRAW_MS. Call it freely.
  void render();

  // Push right now, ignoring the rate limit. For the splash and
  // for anything that must be on screen before a blocking
  // operation starts.
  void renderNow();

  // Call every loop: draws a frame that the rate limiter
  // deferred. Without this a final update can be dropped and the
  // screen left one event stale.
  void tick();

 private:
  bool present_ = false;

  char txLine_[DISPLAY_LINE_CHARS + 1] = "";
  char rxLine_[DISPLAY_LINE_CHARS + 1] = "";
  char partial_[10] = "";
  char status_[DISPLAY_LINE_CHARS + 1] = "";
  MorseSource partialSrc_ = MORSE_FROM_KEY;

  uint32_t lastRender_ = 0;
  bool     dirty_      = false;

  // Keep only the last N chars, so a long message scrolls
  // rather than overflowing.
  void appendTail_(char* line, char c);
};

extern Display oled;

#endif  // DISPLAY_H
