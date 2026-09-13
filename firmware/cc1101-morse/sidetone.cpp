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
