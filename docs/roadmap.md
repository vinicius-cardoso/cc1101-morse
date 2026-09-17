# Roadmap

Ordered so that each step produces something testable, and so the parts that are
still undecided stay off the critical path.

The ANATEL question blocks transmitting on air, so everything up to and including
the PCB is arranged to happen without needing it answered — bench work into a
dummy load is legal and proves the whole design.

## Phase 0 — firmware without hardware

The sketch runs on a bare ESP32-C3 Supermini with nothing else attached.
`radio.h` defaults to `RADIO_SIMULATE 1`, which prints what it would key instead
of driving the chip.

- [x] Key timing: debounce, classify dot vs dash by duration.
- [x] Tree walk and letter decode → serial.
- [x] Adaptive timing, so the dot/dash threshold follows the user's speed.
- [x] Sidetone keyed with the input.
- [ ] Flash it, key a button on GPIO10, confirm letters come out right.
- [ ] Check the adaptive unit settles sensibly for a beginner's keying. The
      1/8 pull rate is a guess and has never been tested against a real hand.

## Phase 1 — the radio on a breadboard

No PCB needed. Supermini + CC1101 module on jumper wires.

- [ ] Wire the CC1101 per the pin table in `docs/hardware.md`. **Check the
      module's silkscreen against it first** — pin order varies by revision and
      crossing VCC/GND destroys the module.
- [ ] Set `RADIO_SIMULATE 0`. Confirm SPI talks: read the `PARTNUM` and `VERSION`
      status registers and check they are sane (`0x00` / `0x14`). This is the
      first thing to do and it isolates wiring problems from everything else.
- [ ] **Fit a 50 Ω dummy load**, not the whip. Everything below happens into the
      dummy load until ANATEL is settled.
- [ ] Transmit: key the radio and confirm the carrier appears. An SDR dongle or
      a second CC1101 is enough to see it.
- [ ] Receive: put the module in RX, confirm GDO0 follows an incoming carrier.
- [ ] Tune `RADIO_RX_MIN_MARK_MS` against the real noise floor. 433 MHz is busy —
      garage doors, weather stations, car fobs — and the current value is a
      guess.

## Phase 2 — two units talking

This is the milestone that makes the device a device rather than a demo.

- [ ] Build a second breadboard unit.
- [ ] Key one, decode on the other. Both into dummy loads, sitting next to each
      other — at a few centimetres the leakage is plenty of signal.
- [ ] Confirm the RX sidetone pitch (550 Hz) is distinguishable from your own
      keying (700 Hz) by ear, which is the point of having two pitches.
- [ ] Full duplex-ish behaviour: what happens if both key at once? Decide whether
      that needs handling or is just an operator problem, the way it is on a real
      band.

## Phase 3 — PCB

Single layer, through-hole only. See `docs/hardware.md` § Single-layer routing.

- [x] Schematic in KiCad. ERC clean, netlist verified against the pin table.
- [x] Placement: CC1101 socket at the top edge, SMA pointing off the board;
      Supermini centred beneath it; key below that; buzzer top-right.
- [x] Route on one face. **No vias, no jumper wires.** Took a pin reassignment
      to get there — see `docs/decisions.md` §9.
- [x] Ground as a filled zone rather than a routed net — `docs/decisions.md` §10.
- [x] Four M2 mounting holes, one per corner. Board is 40 × 60 mm.
- [ ] **Refill the zone (`B`) and re-run DRC until clean.** A zone that has not
      been refilled since the traces were laid reports a clearance violation
      against every net it touches — DRC is meaningless until this is done.
- [ ] Check no trace runs through a mounting hole. The autorouter could not read
      the holes' keepout areas, so it routed without knowing they exist.
- [ ] Export DXF for the laser — `docs/fabrication.md`. **Watch the polarity:**
      the copper layer is what must *remain*, and a laser removes what it marks.
- [ ] Engrave, drill (44 holes, five sizes), populate, test.

## Phase 4 — on the air

- [ ] **Research ANATEL limits for 433 MHz short-range in Brazil. Blocking.**
      Nothing in this phase happens before this is answered.
- [ ] Swap the dummy load for the whip. Range test between two units.
- [ ] Characterise: how far, and how badly does the band's existing traffic
      corrupt reception in practice?

## Phase 5 — case

- [ ] Onshape model around the finished board.
- [ ] Opening for the SMA whip, or a panel-mount pigtail so the module's
      placement is not dictated by the antenna.
- [ ] Design for the whip being unscrewed for storage — it is detachable and the
      device is meant to be pocketable.
- [ ] Export STEP/STL to `mechanical/`.

## Ideas, unscheduled

- **I²C OLED** on GPIO8/9 showing decoded text, so the device is usable without a
  computer attached. The pin budget has five spare, so this costs nothing but the
  part — it is the most obvious next feature.
- **Beacon mode:** transmit a stored message on a timer. Useful for range
  testing alone.
- **Practice mode without the radio:** the device sends a letter to the buzzer,
  you copy it back on the key. Reverse training, no RF involved, so no
  regulatory question.
- **Store what you sent** and replay it, for reviewing your own fist.
- **Adjustable TX power** via `PATABLE`, exposed as a serial command — useful for
  bench work and for staying inside whatever limit ANATEL turns out to set.
