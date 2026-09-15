// ============================================================
// CC1101 Morse — a handheld Morse transceiver
// ============================================================
//
// Key it and the characters go out over 433 MHz as OOK. Leave
// it idle and it decodes what another unit sends back.
//
// Hardware: ESP32-C3 Supermini + CC1101 module + key + buzzer.
// See docs/hardware.md for wiring.
//
// ------------------------------------------------------------
// RUNNING WITHOUT HARDWARE
// ------------------------------------------------------------
//
// The CC1101 is not required. radio.h defaults to
// RADIO_SIMULATE 1, which logs what it would key instead of
// driving the chip. Everything else — key timing, decoding,
// adaptive speed, sidetone — works identically.
//
// You can also drive it entirely from the serial monitor with
// no key wired: send '.' and '-' for symbols, space for a
// letter gap. Type `help` for the rest.
//
// Set RADIO_SIMULATE to 0 once the module is on the board.
//
// ------------------------------------------------------------
// BEFORE TRANSMITTING
// ------------------------------------------------------------
//
// Never key the radio with the antenna unscrewed — power
// reflects back into the PA and can damage it. Use a 50 Ohm
// dummy load for bench work.
//
// The ANATEL position on 433 MHz short-range transmission in
// Brazil has not been researched. Until it has, transmit into
// a dummy load only. See docs/roadmap.md phase 4.
//
// ============================================================

#include "morse_event.h"
#include "morse_tree.h"
#include "key.h"
#include "sidetone.h"
#include "radio.h"
#include "decoder.h"
#include "display.h"

// ------------------------------------------------------------
// Two decoders, one per direction. Same class, same code —
// they differ only in what feeds them and how their output is
// labelled. See morse_event.h.
// ------------------------------------------------------------

static Decoder txDecoder;   // what you are sending
static Decoder rxDecoder;   // what is coming in

// ------------------------------------------------------------
// The OLED, when one is attached.
//
// It mirrors what already goes to serial — nothing depends on
// it, and on the bare PCB (which has no OLED footprint) every
// call below is a no-op. See display.h.
// ------------------------------------------------------------

static void refreshStatus() {
  if (!oled.present()) return;
  char buf[24];
  snprintf(buf, sizeof(buf), "%2u wpm  %s  %s",
           key.wpm(),
           radio.mode() == RADIO_MODE_TX ? "TX" : "RX",
           radio.present() ? "RF" : "--");
  oled.setStatus(buf);
}

// TX holds the radio while you are keying and for a moment
// after, so the mode does not flap between every symbol. Once
// it expires the radio goes back to listening.
#define TX_HANG_MS 1200
static uint32_t lastKeyActivity = 0;
static bool     txHolding       = false;

// ------------------------------------------------------------
// Sending a stored message, for beacon and test use.
// ------------------------------------------------------------

static void sendString(const char* s) {
  uint16_t unit = key.unitMs();

  Serial.print(F("TX: "));
  Serial.println(s);

  radio.setMode(RADIO_MODE_TX);

  for (const char* p = s; *p; p++) {
    if (*p == ' ') {
      delay(unit * 7);          // word gap
      continue;
    }

    const char* code = morseCodeForChar(*p);
    if (!code) continue;        // silently skip punctuation

    for (const char* c = code; *c; c++) {
      radio.keyDown();
      sidetone.on(MORSE_FROM_KEY);
      delay(*c == '-' ? unit * 3 : unit);

      radio.keyUp();
      sidetone.off(MORSE_FROM_KEY);
      delay(unit);              // symbol gap
    }

    delay(unit * 2);            // letter gap (1 already spent above)
  }

  radio.setMode(RADIO_MODE_RX);
  Serial.println(F("TX: done"));
}

// ------------------------------------------------------------
// Serial commands — lets the whole thing be driven without a
// key or a radio attached.
// ------------------------------------------------------------

static void printHelp() {
  Serial.println();
  Serial.println(F("commands:"));
  Serial.println(F("  .         send a dot"));
  Serial.println(F("  -         send a dash"));
  Serial.println(F("  <space>   letter gap (commit the letter)"));
  Serial.println(F("  /         word gap"));
  Serial.println(F("  send TEXT transmit TEXT as Morse"));
  Serial.println(F("  clear     clear both messages"));
  Serial.println(F("  wpm N     set speed, fixed (disables adaptive)"));
  Serial.println(F("  auto      re-enable adaptive speed"));
  Serial.println(F("  tone      toggle the buzzer"));
  Serial.println(F("  buzzer    sweep test: passive or active?"));
  Serial.println(F("  rx        listen (default)"));
  Serial.println(F("  quiet     stop logging received symbols"));
  Serial.println(F("  loud      log received symbols"));
  Serial.println(F("  power N   TX power: 0=low 1=mid 2=full"));
  Serial.println(F("  status    radio registers and RSSI"));
  Serial.println(F("  help      this list"));
  Serial.println();
}

static void handleLine(String& line) {
  if (line == "help") {
    printHelp();

  } else if (line == "clear") {
    txDecoder.clearMessage();
    rxDecoder.clearMessage();
    oled.setMessage("", "");
    Serial.println(F("messages cleared"));

  } else if (line == "auto") {
    key.setAdaptive(true);
    Serial.println(F("adaptive speed on"));

  } else if (line == "tone") {
    sidetone.setEnabled(!sidetone.enabled());
    Serial.print(F("buzzer "));
    Serial.println(sidetone.enabled() ? F("on") : F("off"));

  } else if (line == "buzzer") {
    sidetone.sweepTest();

  } else if (line == "rx") {
    radio.setMode(RADIO_MODE_RX);
    txHolding = false;
    refreshStatus();
    Serial.println(F("listening"));

  } else if (line == "quiet") {
    rxDecoder.setVerbose(false);
    Serial.println(F("rx symbol logging off"));

  } else if (line == "loud") {
    rxDecoder.setVerbose(true);
    Serial.println(F("rx symbol logging on"));

  } else if (line == "status") {
    radio.dumpStatus();
    Serial.print(F("mode: "));
    switch (radio.mode()) {
      case RADIO_MODE_TX:   Serial.println(F("TX"));   break;
      case RADIO_MODE_RX:   Serial.println(F("RX"));   break;
      case RADIO_MODE_IDLE: Serial.println(F("idle")); break;
    }
    Serial.print(F("tx speed: "));
    Serial.print(key.wpm());
    Serial.print(F(" wpm (unit "));
    Serial.print(key.unitMs());
    Serial.print(F(" ms), "));
    Serial.println(key.adaptive() ? F("adaptive") : F("fixed"));
    Serial.print(F("rx speed: "));
    Serial.print(radio.wpm());
    Serial.println(F(" wpm (learned from the sender)"));

  } else if (line.startsWith("send ")) {
    String text = line.substring(5);
    text.trim();
    if (text.length() > 0) sendString(text.c_str());

  } else if (line.startsWith("wpm ")) {
    int w = line.substring(4).toInt();
    if (w >= 3 && w <= 30) {
      key.setAdaptive(false);
      key.setUnitMs(1200 / w);
      refreshStatus();
      Serial.print(F("fixed at "));
      Serial.print(w);
      Serial.println(F(" wpm"));
    } else {
      Serial.println(F("wpm must be 3..30"));
    }

  } else if (line.startsWith("power ")) {
    int p = line.substring(6).toInt();
    // Three useful points rather than raw PATABLE values: low
    // for bench work into a dummy load, full for range tests.
    static const uint8_t LEVELS[3] = {0x60, 0x84, 0xC0};
    if (p >= 0 && p <= 2) {
      radio.setPower(LEVELS[p]);
      Serial.print(F("power set to level "));
      Serial.println(p);
    } else {
      Serial.println(F("power must be 0..2"));
    }

  } else {
    Serial.print(F("unknown: "));
    Serial.println(line);
  }
}

static void handleSerial() {
  static String line = "";

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      line.trim();
      if (line.length() > 0) handleLine(line);
      line = "";
      continue;
    }

    // Single-character symbol input, acted on immediately —
    // but only when it is not part of a command word, so
    // "send SOS" is not eaten letter by letter.
    if (line.length() == 0) {
      if (c == '.') { txDecoder.feed(MORSE_DOT);  continue; }
      if (c == '-') { txDecoder.feed(MORSE_DASH); continue; }
      if (c == ' ') { txDecoder.feed(MORSE_LETTER_GAP); continue; }
      if (c == '/') {
        txDecoder.feed(MORSE_LETTER_GAP);
        txDecoder.feed(MORSE_WORD_GAP);
        continue;
      }
    }

    line += c;
  }
}

// ------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("================================"));
  Serial.println(F(" CC1101 Morse — transceiver"));
  Serial.println(F("================================"));

  key.begin();
  sidetone.begin();
  oled.begin();
  oled.splash("CC1101 Morse", "starting...");
  radio.begin();

  txDecoder.begin("tx | ");
  rxDecoder.begin("RX | ");

  radio.setMode(RADIO_MODE_RX);
  refreshStatus();
  oled.setMessage("", "");

  Serial.print(F("speed: "));
  Serial.print(key.wpm());
  Serial.print(F(" wpm (unit "));
  Serial.print(key.unitMs());
  Serial.println(F(" ms), adaptive"));

  if (!oled.present()) {
    Serial.println(F("display: none (buzzer + serial only)"));
  }

  printHelp();
  Serial.println(F("ready — press the key, or type . and -"));
  Serial.println(F("listening on 433.92 MHz"));
  Serial.println();
}

// ------------------------------------------------------------

void loop() {
  handleSerial();

  // --- transmit side -------------------------------------------

  MorseEvent kev = key.poll();

  if (key.isDown()) {
    bool wasHolding = txHolding;
    radio.keyDown();
    sidetone.on(MORSE_FROM_KEY);
    lastKeyActivity = millis();
    txHolding = true;
    if (!wasHolding) refreshStatus();   // RX -> TX
  } else {
    radio.keyUp();
    sidetone.off(MORSE_FROM_KEY);
  }

  if (kev != MORSE_NONE) {
    char done = txDecoder.feed(kev);
    if (done != MORSE_NO_LETTER) {
      oled.showLetter(done, MORSE_FROM_KEY);
    } else {
      oled.showPartial(txDecoder.partial(), MORSE_FROM_KEY);
    }
    lastKeyActivity = millis();
  }

  // Hand the radio back to receive once keying has stopped for
  // a moment. Without the hang time the mode would switch on
  // every symbol gap, and each switch costs a calibration.
  if (txHolding && !key.isDown() &&
      (millis() - lastKeyActivity) > TX_HANG_MS) {
    radio.setMode(RADIO_MODE_RX);
    txHolding = false;
    refreshStatus();
  }

  // --- receive side --------------------------------------------

  MorseEvent rev = radio.poll();

  // The RX sidetone only sounds when you are not keying, so
  // your own transmission is never interrupted by an incoming
  // one. sidetone.on() enforces this too; checking here keeps
  // it explicit.
  if (!key.isDown()) {
    if (radio.carrier()) {
      sidetone.on(MORSE_FROM_RADIO);
    } else {
      sidetone.off(MORSE_FROM_RADIO);
    }
  }

  if (rev != MORSE_NONE) {
    char done = rxDecoder.feed(rev);
    if (done != MORSE_NO_LETTER) {
      oled.showLetter(done, MORSE_FROM_RADIO);
    } else {
      oled.showPartial(rxDecoder.partial(), MORSE_FROM_RADIO);
    }
  }
}
