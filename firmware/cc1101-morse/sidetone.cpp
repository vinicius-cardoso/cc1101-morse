#include "sidetone.h"

Sidetone sidetone;

void Sidetone::begin() {
  pinMode(SIDETONE_PIN, OUTPUT);
  digitalWrite(SIDETONE_PIN, LOW);
  sounding_ = false;
}

// ------------------------------------------------------------
// First source to key the buzzer owns it until it releases.
//
// The alternative — letting the key interrupt an incoming
// signal — was rejected: your own keying would chop up what
// you are copying, which is exactly backwards. On a real band
// you hear both at once; here the earlier one simply wins.
// ------------------------------------------------------------

void Sidetone::on(MorseSource src) {
  if (!enabled_) return;

  if (sounding_) {
    if (owner_ == src) return;   // already ours, nothing to do
    return;                      // someone else has it
  }

  owner_ = src;
  tone(SIDETONE_PIN, freqFor_(src));
  sounding_ = true;
}

void Sidetone::off(MorseSource src) {
  if (!sounding_) return;
  if (owner_ != src) return;     // not ours to stop
  silence();
}

void Sidetone::silence() {
  if (!sounding_) return;
  noTone(SIDETONE_PIN);
  digitalWrite(SIDETONE_PIN, LOW);
  sounding_ = false;
}

void Sidetone::setEnabled(bool on) {
  enabled_ = on;
  if (!enabled_) silence();
}

// ------------------------------------------------------------
// Sweep 400 -> 1200 Hz so the ear can settle the passive/active
// question in two seconds. See the comment in sidetone.h.
// ------------------------------------------------------------

void Sidetone::sweepTest() {
  bool wasEnabled = enabled_;
  enabled_ = true;
  silence();

  Serial.println();
  Serial.println(F("[BUZZER] sweeping 400 -> 1200 Hz..."));
  Serial.println(F("  pitch climbs    -> PASSIVE (correct part)"));
  Serial.println(F("  one steady note -> ACTIVE  (wrong part)"));
  Serial.println(F("  silence         -> not wired"));

  for (uint16_t f = 400; f <= 1200; f += 20) {
    tone(SIDETONE_PIN, f);
    delay(25);
  }
  noTone(SIDETONE_PIN);
  digitalWrite(SIDETONE_PIN, LOW);
  sounding_ = false;

  delay(300);

  // Then the two real pitches back to back — on a passive
  // buzzer these are obviously different notes, which is the
  // property the firmware actually depends on.
  Serial.println(F("[BUZZER] now the two sidetone pitches:"));
  Serial.println(F("  700 Hz (your keying), then 550 Hz (received)"));

  tone(SIDETONE_PIN, SIDETONE_TX_HZ);
  delay(500);
  noTone(SIDETONE_PIN);
  delay(200);
  tone(SIDETONE_PIN, SIDETONE_RX_HZ);
  delay(500);
  noTone(SIDETONE_PIN);
  digitalWrite(SIDETONE_PIN, LOW);
  sounding_ = false;

  Serial.println(F("[BUZZER] if those sounded identical, it is ACTIVE."));
  Serial.println();

  enabled_ = wasEnabled;
}

// ------------------------------------------------------------

void Sidetone::blip(uint16_t freqHz, uint16_t ms) {
  if (!enabled_) return;

  bool wasSounding = sounding_;
  MorseSource wasOwner = owner_;
  silence();

  tone(SIDETONE_PIN, freqHz, ms);
  delay(ms);
  noTone(SIDETONE_PIN);
  digitalWrite(SIDETONE_PIN, LOW);

  sounding_ = false;
  if (wasSounding) on(wasOwner);
}
