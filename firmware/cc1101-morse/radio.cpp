#include "radio.h"

Radio radio;

// ------------------------------------------------------------
// CC1101 registers — only the ones this design touches.
//
// The full map is large and mostly concerns the packet engine,
// which is switched off here. See the TI datasheet (SWRS061)
// if something below needs changing.
// ------------------------------------------------------------

#define CC1101_IOCFG2     0x00
#define CC1101_IOCFG0     0x02
#define CC1101_FIFOTHR    0x03
#define CC1101_PKTLEN     0x06
#define CC1101_PKTCTRL1   0x07
#define CC1101_PKTCTRL0   0x08
#define CC1101_FSCTRL1    0x0B
#define CC1101_FREQ2      0x0D
#define CC1101_FREQ1      0x0E
#define CC1101_FREQ0      0x0F
#define CC1101_MDMCFG4    0x10
#define CC1101_MDMCFG3    0x11
#define CC1101_MDMCFG2    0x12
#define CC1101_MDMCFG1    0x13
#define CC1101_MDMCFG0    0x14
#define CC1101_MCSM0      0x18
#define CC1101_FOCCFG     0x19
#define CC1101_AGCCTRL2   0x1B
#define CC1101_AGCCTRL1   0x1C
#define CC1101_AGCCTRL0   0x1D
#define CC1101_FREND0     0x22
#define CC1101_FSCAL3     0x23
#define CC1101_FSCAL2     0x24
#define CC1101_FSCAL1     0x25
#define CC1101_FSCAL0     0x26
#define CC1101_TEST2      0x2C
#define CC1101_TEST1      0x2D
#define CC1101_TEST0      0x2E

// Status registers (read with the burst bit set)
#define CC1101_PARTNUM    0x30
#define CC1101_VERSION    0x31
#define CC1101_RSSI       0x34
#define CC1101_MARCSTATE  0x35

// Command strobes
#define CC1101_SRES       0x30
#define CC1101_SCAL       0x33
#define CC1101_SRX        0x34
#define CC1101_STX        0x35
#define CC1101_SIDLE      0x36
#define CC1101_SFRX       0x3A
#define CC1101_SFTX       0x3B

#define CC1101_PATABLE    0x3E

#define CC1101_WRITE_BURST 0x40
#define CC1101_READ_SINGLE 0x80
#define CC1101_READ_BURST  0xC0

// GDO0 configurations.
//   0x0D — asynchronous serial data output (RX: pin follows carrier)
//   0x2E — three-state (TX: pin is an input, we drive it)
#define GDO0_CFG_ASYNC_DATA 0x0D
#define GDO0_CFG_HIGH_Z     0x2E

// ------------------------------------------------------------

bool Radio::begin() {
#if RADIO_SIMULATE
  present_ = true;
  Serial.println(F("[RADIO] SIMULATED — no CC1101 required"));
  Serial.println(F("        set RADIO_SIMULATE 0 in radio.h"));
  Serial.println(F("        once the module is wired"));
  return true;
#else
  pinMode(RADIO_PIN_CS,   OUTPUT);
  pinMode(RADIO_PIN_SCK,  OUTPUT);
  pinMode(RADIO_PIN_MOSI, OUTPUT);
  pinMode(RADIO_PIN_MISO, INPUT);

  // GDO2 is wired but unused. Held as an input so the ESP32-C3
  // never drives a pin the CC1101 might also drive.
  pinMode(RADIO_PIN_GDO2, INPUT);

  deselect_();
  digitalWrite(RADIO_PIN_SCK,  LOW);
  digitalWrite(RADIO_PIN_MOSI, LOW);

  delay(10);
  reset_();

  // A CC1101 answers 0x00 / 0x14 here. Anything else means the
  // wiring is wrong — almost always MISO/MOSI swapped, or the
  // module's pin order differing from the assumed one.
  uint8_t part = readStatus_(CC1101_PARTNUM);
  uint8_t ver  = readStatus_(CC1101_VERSION);
  present_ = (ver != 0x00 && ver != 0xFF);

  if (!present_) {
    Serial.print(F("[RADIO] no response (PARTNUM 0x"));
    Serial.print(part, HEX);
    Serial.print(F(" VERSION 0x"));
    Serial.print(ver, HEX);
    Serial.println(F(") — check wiring"));
    return false;
  }

  configure_();
  setPower(power_);
  setMode(RADIO_MODE_RX);

  Serial.print(F("[RADIO] CC1101 ready (VERSION 0x"));
  Serial.print(ver, HEX);
  Serial.println(')');
  return true;
#endif
}

// ------------------------------------------------------------
// Register table.
//
// Everything here serves one goal: turn the packet radio into a
// plain carrier switch. The comments say what each line is for,
// because a bare table of hex is unreadable six months later.
// ------------------------------------------------------------

void Radio::configure_() {
#if !RADIO_SIMULATE
  // --- carrier frequency: 433.92 MHz ---------------------------
  // f = (FREQ / 2^16) * 26 MHz, with a 26 MHz crystal.
  writeReg_(CC1101_FREQ2, 0x10);
  writeReg_(CC1101_FREQ1, 0xB0);
  writeReg_(CC1101_FREQ0, 0x71);

  // --- modulation ---------------------------------------------
  // MDMCFG2 = 0x30: OOK/ASK, no Manchester, NO sync word.
  // The sync word is the packet engine's way of knowing a
  // transmission started; we have no packets, so it must be off
  // or the receiver would wait forever for a frame that never
  // comes.
  writeReg_(CC1101_MDMCFG2, 0x30);

  // Channel filter bandwidth and data rate. In async mode the
  // data rate does not gate our timing — the firmware does —
  // but it still sets the receiver's filtering, so it wants to
  // be comfortably faster than the fastest keying.
  writeReg_(CC1101_MDMCFG4, 0xF5);
  writeReg_(CC1101_MDMCFG3, 0x83);
  writeReg_(CC1101_MDMCFG1, 0x22);
  writeReg_(CC1101_MDMCFG0, 0xF8);

  // --- packet engine: off -------------------------------------
  // PKTCTRL0 = 0x32: asynchronous serial mode, infinite packet
  // length, no CRC, no whitening. This is the line that turns
  // the chip into a carrier switch.
  writeReg_(CC1101_PKTCTRL0, 0x32);
  writeReg_(CC1101_PKTCTRL1, 0x00);   // no address check, no status bytes
  writeReg_(CC1101_PKTLEN,   0xFF);

  // --- GDO pins -----------------------------------------------
  // GDO2 is wired to a GPIO but unused by default (three-state).
  // See RADIO_GDO2_CFG in radio.h for what it could carry.
  writeReg_(CC1101_IOCFG2, RADIO_GDO2_CFG);
  writeReg_(CC1101_IOCFG0, GDO0_CFG_ASYNC_DATA);

  // --- AGC ----------------------------------------------------
  // OOK needs the AGC told what "off" looks like, since an
  // absent carrier is the signal. These are TI's recommended
  // OOK values: without them the receiver chases the noise
  // floor during spaces and reports a carrier that is not there.
  writeReg_(CC1101_AGCCTRL2, 0x07);
  writeReg_(CC1101_AGCCTRL1, 0x00);
  writeReg_(CC1101_AGCCTRL0, 0x91);

  writeReg_(CC1101_FSCTRL1, 0x06);
  writeReg_(CC1101_FOCCFG,  0x16);
  writeReg_(CC1101_FREND0,  0x11);

  // --- calibration --------------------------------------------
  writeReg_(CC1101_MCSM0,  0x18);   // auto-calibrate when leaving IDLE
  writeReg_(CC1101_FSCAL3, 0xE9);
  writeReg_(CC1101_FSCAL2, 0x2A);
  writeReg_(CC1101_FSCAL1, 0x00);
  writeReg_(CC1101_FSCAL0, 0x1F);

  writeReg_(CC1101_TEST2, 0x81);
  writeReg_(CC1101_TEST1, 0x35);
  writeReg_(CC1101_TEST0, 0x09);

  strobe_(CC1101_SCAL);
  delay(2);
#endif
}

// ------------------------------------------------------------

void Radio::setPower(uint8_t paValue) {
  power_ = paValue;
#if !RADIO_SIMULATE
  if (!present_) return;
  // In OOK, PATABLE[0] is the "off" level and PATABLE[1] the
  // "on" level — so the table is literally the key's two states.
  select_();
  transfer_(CC1101_PATABLE | CC1101_WRITE_BURST);
  transfer_(0x00);          // carrier off
  transfer_(power_);        // carrier on
  deselect_();
#endif
}

// ------------------------------------------------------------
// Mode switching.
//
// GDO0 changes direction with the mode, which is the one thing
// to be careful about: in TX it is an output we drive, in RX it
// is an input driven by the chip. Setting the pinMode before
// the chip has been reconfigured would briefly fight it.
// ------------------------------------------------------------

void Radio::setMode(RadioMode m) {
  if (m == mode_) return;

  // Always drop the carrier before changing anything.
  if (keyed_) keyUp();

#if RADIO_SIMULATE
  mode_ = m;
  return;
#else
  if (!present_) { mode_ = m; return; }

  strobe_(CC1101_SIDLE);
  delayMicroseconds(100);

  switch (m) {
    case RADIO_MODE_TX:
      writeReg_(CC1101_IOCFG0, GDO0_CFG_HIGH_Z);
      pinMode(RADIO_PIN_GDO0, OUTPUT);
      digitalWrite(RADIO_PIN_GDO0, LOW);     // carrier off
      strobe_(CC1101_SFTX);
      strobe_(CC1101_STX);
      break;

    case RADIO_MODE_RX:
      pinMode(RADIO_PIN_GDO0, INPUT);
      writeReg_(CC1101_IOCFG0, GDO0_CFG_ASYNC_DATA);
      strobe_(CC1101_SFRX);
      strobe_(CC1101_SRX);
      break;

    case RADIO_MODE_IDLE:
      pinMode(RADIO_PIN_GDO0, INPUT);
      break;
  }

  mode_ = m;
  delayMicroseconds(200);
#endif
}

// ------------------------------------------------------------
// Keying.
//
// Switching into TX costs a calibration (a few hundred
// microseconds), so it happens on the first keyDown rather than
// per symbol — the radio stays in TX until something explicitly
// puts it back to RX. Doing it per dot would put that latency
// inside every symbol and smear the timing.
// ------------------------------------------------------------

void Radio::keyDown() {
  if (keyed_) return;

  if (mode_ != RADIO_MODE_TX) setMode(RADIO_MODE_TX);
  keyed_ = true;

#if RADIO_SIMULATE
  Serial.print(F("<tx on>"));
#else
  if (present_) digitalWrite(RADIO_PIN_GDO0, HIGH);
#endif
}

void Radio::keyUp() {
  if (!keyed_) return;
  keyed_ = false;

#if RADIO_SIMULATE
  Serial.print(F("<tx off>"));
#else
  if (present_) digitalWrite(RADIO_PIN_GDO0, LOW);
#endif
}

// ------------------------------------------------------------
// Receiving.
//
// This is key.cpp's timing logic pointed at a pin the chip
// drives instead of a pin a finger drives. The differences are
// only the two that matter physically:
//
//   - no debounce; a radio edge is not a bouncing contact
//   - a minimum mark length, because the band is full of other
//     people's transmitters and a 3 ms blip is not a dot
//
// Everything downstream is identical, which is the point.
// ------------------------------------------------------------

MorseEvent Radio::poll() {
  if (mode_ != RADIO_MODE_RX) return MORSE_NONE;

  uint32_t now = millis();

#if RADIO_SIMULATE
  bool reading = false;              // a simulated band is quiet
#else
  bool reading = present_ && (digitalRead(RADIO_PIN_GDO0) == HIGH);
#endif

  // --- edges ---------------------------------------------------
  if (reading != rxRaw_) {
    rxRaw_  = reading;
    rxEdge_ = now;

    if (reading) {
      // Carrier came up. Provisional — a mark this short may
      // still turn out to be noise when it ends.
      rxMark_      = true;
      rxMarkStart_ = now;
    } else {
      // Carrier dropped: classify what we just heard.
      uint32_t held = now - rxMarkStart_;
      rxMark_       = false;
      rxSpaceStart_ = now;

      if (held < RADIO_RX_MIN_MARK_MS) {
        return MORSE_NONE;           // noise, not a symbol
      }

      rxLetterClosed_ = false;
      rxWordClosed_   = false;
      rxAnySymbolYet_ = true;

      bool isDash = (held >= (uint32_t)rxUnit_ * 2);
      learnRx_(held, isDash);
      return isDash ? MORSE_DASH : MORSE_DOT;
    }
  }

  // --- gaps while the channel is quiet -------------------------
  if (!rxMark_ && rxAnySymbolYet_) {
    uint32_t idle = now - rxSpaceStart_;

    if (!rxLetterClosed_ && idle >= (uint32_t)rxUnit_ * 2) {
      rxLetterClosed_ = true;
      return MORSE_LETTER_GAP;
    }

    if (rxLetterClosed_ && !rxWordClosed_ && idle >= (uint32_t)rxUnit_ * 6) {
      rxWordClosed_ = true;
      return MORSE_WORD_GAP;
    }

    // Squelch. Nothing for a long while means the other station
    // has stopped, so forget the speed we learned from them —
    // the next one to come up may send at a different pace.
    if (idle >= RADIO_RX_IDLE_RESET_MS) {
      rxAnySymbolYet_ = false;
      rxUnit_         = 100;
    }
  }

  return MORSE_NONE;
}

// ------------------------------------------------------------
// Adapt to the sender's speed, exactly as the key adapts to
// yours. The other operator sets the pace and we follow it —
// same gentle 1/8 pull, same reasoning.
// ------------------------------------------------------------

void Radio::learnRx_(uint32_t markMs, bool wasDash) {
  int32_t target = wasDash ? (int32_t)markMs / 3 : (int32_t)markMs;
  int32_t next   = (int32_t)rxUnit_ + (target - (int32_t)rxUnit_) / 8;

  if (next < 30)  next = 30;      // faster than any hand sends
  if (next > 400) next = 400;
  rxUnit_ = (uint16_t)next;
}

// ------------------------------------------------------------

void Radio::dumpStatus() {
#if RADIO_SIMULATE
  Serial.println(F("[RADIO] simulated — no status to read"));
#else
  if (!present_) {
    Serial.println(F("[RADIO] not present"));
    return;
  }

  Serial.print(F("[RADIO] PARTNUM 0x"));
  Serial.print(readStatus_(CC1101_PARTNUM), HEX);
  Serial.print(F("  VERSION 0x"));
  Serial.print(readStatus_(CC1101_VERSION), HEX);
  Serial.print(F("  MARCSTATE 0x"));
  Serial.print(readStatus_(CC1101_MARCSTATE) & 0x1F, HEX);

  // RSSI is a two's-complement value in half-dB steps with a
  // 74 dB offset (datasheet section 17.3).
  int8_t  raw = (int8_t)readStatus_(CC1101_RSSI);
  int16_t dbm = (int16_t)raw / 2 - 74;
  Serial.print(F("  RSSI "));
  Serial.print(dbm);
  Serial.println(F(" dBm"));
#endif
}

// ------------------------------------------------------------
// Bit-banged SPI.
//
// The CC1101 is mode 0 (clock idle low, sample on rising edge)
// and MSB first. Bit-banging rather than using the SPI
// peripheral keeps the pin choice free for the PCB layout,
// and at Morse rates the speed is irrelevant — even a slow
// register write finishes long before the next symbol.
// ------------------------------------------------------------

uint8_t Radio::transfer_(uint8_t b) {
#if RADIO_SIMULATE
  (void)b;
  return 0;
#else
  uint8_t in = 0;
  for (int8_t i = 7; i >= 0; i--) {
    digitalWrite(RADIO_PIN_MOSI, (b >> i) & 1);
    digitalWrite(RADIO_PIN_SCK, HIGH);
    in = (in << 1) | (digitalRead(RADIO_PIN_MISO) ? 1 : 0);
    digitalWrite(RADIO_PIN_SCK, LOW);
  }
  return in;
#endif
}

void Radio::writeReg_(uint8_t addr, uint8_t value) {
#if !RADIO_SIMULATE
  select_();
  transfer_(addr);
  transfer_(value);
  deselect_();
#else
  (void)addr; (void)value;
#endif
}

uint8_t Radio::readReg_(uint8_t addr) {
#if !RADIO_SIMULATE
  select_();
  transfer_(addr | CC1101_READ_SINGLE);
  uint8_t v = transfer_(0x00);
  deselect_();
  return v;
#else
  (void)addr;
  return 0;
#endif
}

// Status registers collide with the command strobes in the
// address space — 0x30 is both SRES and PARTNUM. The burst bit
// is what distinguishes a status read from a strobe, so these
// must always use READ_BURST, never READ_SINGLE.
uint8_t Radio::readStatus_(uint8_t addr) {
#if !RADIO_SIMULATE
  select_();
  transfer_(addr | CC1101_READ_BURST);
  uint8_t v = transfer_(0x00);
  deselect_();
  return v;
#else
  (void)addr;
  return 0;
#endif
}

void Radio::strobe_(uint8_t cmd) {
#if !RADIO_SIMULATE
  select_();
  transfer_(cmd);
  deselect_();
#else
  (void)cmd;
#endif
}

void Radio::reset_() {
#if !RADIO_SIMULATE
  // The manual power-on reset sequence from the datasheet:
  // toggle CS, wait, then strobe SRES and wait for MISO to go
  // low (the chip pulls it low when the reset completes).
  deselect_();
  delayMicroseconds(5);
  select_();
  delayMicroseconds(10);
  deselect_();
  delayMicroseconds(45);

  select_();
  uint32_t start = millis();
  while (digitalRead(RADIO_PIN_MISO) == HIGH) {
    if (millis() - start > 100) break;   // no module attached
  }
  transfer_(CC1101_SRES);

  start = millis();
  while (digitalRead(RADIO_PIN_MISO) == HIGH) {
    if (millis() - start > 100) break;
  }
  deselect_();
  delay(1);
#endif
}
