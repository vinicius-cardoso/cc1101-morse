#include "display.h"

Display oled;

#if DISPLAY_ENABLE

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 panel(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1);

// ------------------------------------------------------------
// Probe before trusting.
//
// Adafruit_SSD1306::begin() returns false when nothing answers,
// but it is worth checking the bus directly first: on a board
// with no OLED the I2C pins float, and a bare begin() can sit
// there retrying. A single-address probe answers in
// microseconds either way.
// ------------------------------------------------------------

bool Display::begin() {
  Wire.begin(DISPLAY_SDA_PIN, DISPLAY_SCL_PIN);

  Wire.beginTransmission(DISPLAY_ADDR);
  if (Wire.endTransmission() != 0) {
    present_ = false;
    Serial.println(F("[OLED] none found — running headless"));
    return false;
  }

  if (!panel.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDR)) {
    present_ = false;
    Serial.println(F("[OLED] found on the bus but would not start"));
    return false;
  }

  present_ = true;
  panel.clearDisplay();
  panel.setTextColor(SSD1306_WHITE);
  panel.display();

  Serial.println(F("[OLED] ready"));
  return true;
}

// ------------------------------------------------------------

void Display::appendTail_(char* line, char c) {
  size_t n = strlen(line);
  if (n >= DISPLAY_LINE_CHARS) {
    // Drop the oldest character and shuffle left. The line is
    // short enough that this costs nothing, and it keeps the
    // most recent text visible, which is the part you want.
    memmove(line, line + 1, n - 1);
    n--;
  }
  line[n] = c;
  line[n + 1] = '\0';
}

// ------------------------------------------------------------

void Display::showPartial(const char* symbols, MorseSource src) {
  if (!present_) return;
  strncpy(partial_, symbols ? symbols : "", sizeof(partial_) - 1);
  partial_[sizeof(partial_) - 1] = '\0';
  partialSrc_ = src;
  render();
}

void Display::showLetter(char c, MorseSource src) {
  if (!present_) return;
  appendTail_(src == MORSE_FROM_RADIO ? rxLine_ : txLine_, c);
  partial_[0] = '\0';
  render();
}

void Display::setMessage(const char* tx, const char* rx) {
  if (!present_) return;
  if (tx) {
    size_t n = strlen(tx);
    const char* tail = (n > DISPLAY_LINE_CHARS) ? tx + n - DISPLAY_LINE_CHARS : tx;
    strncpy(txLine_, tail, DISPLAY_LINE_CHARS);
    txLine_[DISPLAY_LINE_CHARS] = '\0';
  }
  if (rx) {
    size_t n = strlen(rx);
    const char* tail = (n > DISPLAY_LINE_CHARS) ? rx + n - DISPLAY_LINE_CHARS : rx;
    strncpy(rxLine_, tail, DISPLAY_LINE_CHARS);
    rxLine_[DISPLAY_LINE_CHARS] = '\0';
  }
  render();
}

void Display::setStatus(const char* text) {
  if (!present_) return;
  strncpy(status_, text ? text : "", DISPLAY_LINE_CHARS);
  status_[DISPLAY_LINE_CHARS] = '\0';
  render();
}

// ------------------------------------------------------------

void Display::splash(const char* line1, const char* line2) {
  if (!present_) return;
  panel.clearDisplay();
  panel.setTextSize(1);
  panel.setCursor(0, 8);
  panel.println(line1);
  panel.setCursor(0, 24);
  panel.println(line2);
  panel.display();
}

// ------------------------------------------------------------
// Layout
//
//   status          (small, top)
//   ---------------------------
//   TX: the letters you sent
//   symbols being formed        (large — this is the live one)
//   ---------------------------
//   RX: what came in
//
// The symbol line is drawn at size 2 because it is the thing
// changing while you key, and the one you glance at mid-letter.
// ------------------------------------------------------------

void Display::render() {
  if (!present_) return;

  panel.clearDisplay();
  panel.setTextSize(1);

  // status
  panel.setCursor(0, 0);
  panel.print(status_);
  panel.drawFastHLine(0, 10, DISPLAY_WIDTH, SSD1306_WHITE);

  // what you have sent
  panel.setCursor(0, 14);
  panel.print(F("TX "));
  panel.print(txLine_);

  // the letter in progress, large
  panel.setTextSize(2);
  panel.setCursor(0, 26);
  if (partial_[0]) {
    panel.print(partialSrc_ == MORSE_FROM_RADIO ? '<' : '>');
    panel.print(partial_);
  }

  // what came in
  panel.setTextSize(1);
  panel.drawFastHLine(0, 46, DISPLAY_WIDTH, SSD1306_WHITE);
  panel.setCursor(0, 52);
  panel.print(F("RX "));
  panel.print(rxLine_);

  panel.display();
}

#else  // DISPLAY_ENABLE == 0

// Built without the Adafruit libraries. Every call is a no-op
// and present() is always false, so the rest of the firmware
// needs no #ifdefs of its own.

bool Display::begin() {
  present_ = false;
  Serial.println(F("[OLED] compiled out (DISPLAY_ENABLE 0)"));
  return false;
}
void Display::showPartial(const char*, MorseSource) {}
void Display::showLetter(char, MorseSource) {}
void Display::setMessage(const char*, const char*) {}
void Display::setStatus(const char*) {}
void Display::splash(const char*, const char*) {}
void Display::render() {}
void Display::appendTail_(char*, char) {}

#endif  // DISPLAY_ENABLE
