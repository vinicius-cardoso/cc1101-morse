# Design decisions

A record of what was chosen, what was rejected, and why. Written so the reasoning
survives — if a decision gets revisited, the tradeoff is here rather than being
re-derived from scratch.

## 1. No display

**Decision:** the device has no screen and no indicator panel. Output is the
buzzer and the serial monitor.

**Why:** a display is the single most expensive feature a board like this can
carry, and the cost is not the display itself — it is everything the display
drags in. A multiplexed LED panel needs a driver IC; the common drivers are 5 V
parts, so a 3.3 V MCU needs level shifting; 5 V from a battery needs a boost
converter; and the driver's addressing scheme rarely matches the physical
arrangement, which forces a second copper layer to resolve the crossings.

None of that exists here. The chain never starts.

**Tradeoff accepted:** you cannot read what you sent without a computer attached.
For a device whose purpose is keying Morse — where the operator is listening, not
looking — that is a smaller loss than it sounds. The buzzer carries the
information in real time, which is how Morse is meant to be received.

**Open, and cheap:** an I²C OLED on GPIO8/GPIO9 gives decoded text with two wires
and no driver IC, because it has its own controller. The firmware already
supports one (`display.cpp`) — see §11. The *board* still has no footprint for
it, which keeps §2 and §3 intact.

## 2. Through-hole only

**Decision:** every part on the board is through-hole. No SMD, not even passives.

**Why:**

- **Soldering.** On a single-layer board, through-hole parts are soldered from
  the face *without* components. There is nothing to work around, no tweezers,
  no reflow, no solder paste.
- **Laser.** Every footprint sits on a 2.54 mm or wider grid, which no fiber
  laser struggles with. Characterising the minimum trace/space stops being a
  prerequisite for starting.
- **Serviceability.** Parts can be desoldered and replaced with a plain iron.

**Tradeoff accepted:** a larger board, and 44 holes to drill. Both are cheap
here — the board is sized by the Supermini and the CC1101 module, not by the
passives, so going SMD would not shrink it much.

## 3. Single layer, no jumpers

**Decision:** route on one face, no jumper wires.

**Why it is possible:** there is very little to route. Twelve nets, seven parts:
six parallel traces between two headers, two signal traces to a switch and a
buzzer, and power. Nothing fights for the same space once the parts are placed
sensibly.

**What makes it work in practice:** ground is a **filled zone**, not a traced
net (see §10). That removes the net that touches everything from the routing
problem entirely, which is what lets the rest fit on one face.

**Rejected:** double-sided "because it is easier to route." It is, but it costs
two-sided laser registration and alignment holes for a board that does not need
it. Single layer is the point of this design; if the routing ever genuinely
requires a second layer, that is a signal the scope crept.

## 4. USB-C power, no battery

**Decision:** powered from the ESP32-C3 Supermini's own USB-C port. No battery,
and no header for one.

**Why:** the CC1101 and the ESP32-C3 are both 3.3 V natively, so there is no
second rail to generate. The Supermini's onboard regulator is the entire power
design — zero power components on the board.

**Why no battery now:** the board's first job is bench work — talking to a second
unit, testing into a dummy load, settling the ANATEL question. All of that
happens next to a computer, where USB is already connected for the serial monitor
anyway.

**Rejected:** fitting the battery now. It is a cell, a charge module, a switch,
and a sense divider — four parts and their traces — for a capability nothing
currently needs. Adding it later means soldering to the Supermini's `5V`/`GND`
pins, which needs no board change: nothing here wants more than 3.3 V, so no
boost converter is involved.

## 5. Transceiver, not just a transmitter

**Decision:** the device receives as well as transmits. Incoming carrier is
decoded to letters and played on the buzzer at a different pitch.

**Why:** it is nearly free. The CC1101 does OOK in both directions with the same
async configuration, and the decoder that times the key is *the same decoder*
that times the received signal — a mark is a mark whether a finger or a carrier
produced it. `decoder.cpp` does not know which source it is handling.

What it buys is that two boards talk to each other, which makes the device
testable without a second radio of any other kind, and useful as a pair.

**Tradeoff accepted:** in async mode there is no addressing and no error
correction, so anything on 433 MHz keys the receiver — garage doors, weather
stations, car fobs. Expect garbage between real letters. The decoder filters
implausibly short marks; it cannot tell a doorbell from a dash.

## 6. OOK async, not the packet engine

**Decision:** configure the CC1101 as a bare carrier switch (async serial mode,
`PKTCTRL0 = 0x32`), bypassing its packet engine entirely.

**Why:** Morse on a radio *is* OOK — carrier on for a mark, off for a space, with
the timing carrying the message. There is nothing to packetise. Using the packet
engine would mean buffering the operator's keying, transmitting it as a block,
and replaying it at the far end, which destroys the property that makes Morse
Morse: it is live. You hear the other operator's hand, their rhythm, their
hesitation.

**Consequence:** no CRC, no addressing, no retries. Accepted — see §5.

**Rejected:** packet mode with a Morse-shaped payload. More robust on paper, but
it turns a radio into a messaging protocol and the latency makes real-time
keying impossible.

## 7. GPIO8, GPIO9 and GPIO21 are kept free

**Decision:** the key sits on GPIO10, the buzzer on GPIO20, and the CC1101 on
GPIO0–7. GPIO3, GPIO4, GPIO8, GPIO9 and GPIO21 are left unconnected.

**Why GPIO8/GPIO9 specifically:** they are the strapping pins and the I²C
defaults, which makes them the least convenient pins to commit and the most
valuable to keep. An I²C OLED showing decoded text is the obvious future
addition (§1), and it needs exactly those two.

This cost something real: the crossing-free pin search in §9 found a solution
41 mm shorter when allowed to use them. The shorter route was not worth spending
the pins.

## 8. The buzzer carries two pitches

**Decision:** 700 Hz for your own keying, 550 Hz for received signal.

**Why:** the buzzer is the only real-time output the device has, and it has to
serve two directions at once. Two pitches make them distinguishable by ear
without looking at the serial monitor — which matters, because the whole point of
keying is not looking at a screen.

700 Hz is the classic CW sidetone pitch. 550 Hz is far enough below it to be
unmistakable and still comfortable.

**Requires a passive buzzer.** An active buzzer contains its own oscillator and
plays one fixed note, ignoring `tone()` entirely — which would collapse both
pitches into one and lose the distinction that justifies having them.

## 9. The SPI pin assignment was chosen by the router, not by hand

**Decision:** MOSI on GPIO6, SCK on GPIO5, MISO on GPIO2 — replacing an earlier
hand-picked GPIO2/3/4.

**Why:** the first autoroute attempt failed. Not marginally — it left a net
unroutable and stalled. The cause was not the router: the straight-line paths
between the connector and the MCU **crossed each other five times among seven
nets**, because the connector's left-hand pins were assigned to the MCU's
right-hand pins and vice versa.

On a two-layer board a crossing is a via. On a single-layer board it is a trace
that **cannot be routed at all**, so five crossings meant the assignment was
unroutable no matter how long the router ran.

**What makes the fix legal:** every SPI line here is bit-banged
(`radio.cpp`, `transfer_()`), not driven by a hardware SPI peripheral. There is
no fixed pin mapping to respect — any GPIO can carry any of these signals. So the
mapping is free, and the right way to pick it is to search for the permutation
with no crossings rather than to guess.

**The general lesson, worth keeping:** when an autorouter fails on a trivial
board, the placement or the assignment is wrong, not the router's settings. Count
the crossings before touching anything else — on one layer that number is a hard
bound on what is routable.

## 10. Ground is a filled zone

**Decision:** ground is a copper pour on `B.Cu`, not a routed net.

**Why:** ground is the net that touches everything — every part has a ground pin,
and they sit at opposite corners of the board. Routing it on a single layer means
threading one trace past every other trace, and it was the last thing the
autorouter failed on.

A pour makes the problem disappear: each ground pad drops into the fill wherever
it happens to sit, with nothing to route.

**Why the fragmentation worry did not apply:** the usual objection to a
single-layer pour is that signal traces cut it into islands. This board is
40 × 60 mm with about 15 thin traces concentrated in one corridor — there is far
more uninterrupted copper than there is trace, and the ground pads sit at the
edges where the fill is continuous. Dense boards have this problem; this one does
not.

**It also suits the process.** A fiber laser works by *removing* copper, so
leaving a large filled area is less etching than clearing it away. The pour is
faster to make than the alternative, not slower.

**Verification:** after filling, no GND pad should show a ratsnest line, and DRC
must be clean. A zone that has not been refilled since the traces were laid will
show clearance violations against every net — refill (`B`) before believing any
DRC result.

## 11. The OLED is supported in firmware but absent from the board

**Decision:** `display.cpp` drives an SSD1306 on GPIO8/GPIO9, but the PCB carries
no footprint, no connector and no I²C pull-ups for it.

**Why both at once:** §1 rejected a display because of what a display *drags in*
— a driver IC, a second voltage, a second copper layer. An I²C OLED brings none
of that: it has its own controller, runs at 3.3 V, and costs two wires. The
objection in §1 was never to pixels; it was to the support chain.

But putting it on the board would still cost a connector, four more holes, and a
cutout in a case that does not exist yet — for a part that is genuinely optional.

So the firmware supports it and the board does not require it. On a breadboard
you get decoded text; on the bare PCB you get the buzzer and the serial monitor,
which is what §1 signed up for.

**How absence is handled — runtime, not compile time.** `Display::begin()` probes
0x3C once. Nothing there, and every later call returns immediately. No retry, no
timeout, no blocking.

That choice matters more than it looks. A `#define` would have meant two
binaries, and the wrong one on the wrong board is a confusing failure. A runtime
probe means **one firmware image runs on both**, and plugging a screen into a
finished board works without recompiling anything.

**Tradeoff accepted:** a few hundred bytes of flash on boards that will never
have a display, and a dependency on two Adafruit libraries. `DISPLAY_ENABLE 0`
removes both if that ever matters.

## 12. The socket follows the module's pinout, and the firmware follows the socket

**Decision:** `J1`'s pads carry GND, VCC, GDO0, CSN, SCK, MOSI, MISO, GDO2 in
that order — the CC1101 module's own numbering — and `radio.h` names whichever
GPIO reaches each pad accordingly.

**Why this needed deciding at all:** the board was first drawn with a different
pin order, guessed rather than read off the module. Two things were wrong. The
supply pair was reversed, so plugging a module into the board would have applied
+3V3 to its GND pin. And all six signals were offset, so nothing would have
talked even if the module survived.

It was caught on the breadboard, where `status` reported
`PARTNUM 0xFF VERSION 0xFF` — MISO stuck high, the signature of a module that is
not driving the bus.

**Why the fix was cheap:** the SPI is bit-banged (`radio.cpp`, `transfer_()`), so
no pin is special. Once the socket matches the module, the firmware is six
`#define` lines. Nothing was re-routed for the signals.

The power pair was different — `+3V3` and `GND` are physical rails and cannot be
remapped in software, so that pair did need fixing on the board.

**The rule worth keeping:** read the pinout off the module in hand, not off a
datasheet found elsewhere, and get it into the schematic before routing. This
cost a full re-check of the board late, and would have cost a destroyed module
had it reached copper.

## 13. All copper is on the back, components on the front

**Decision:** traces and the ground pour live on `B.Cu`. Footprints stay on
`F.Cu`, so component bodies sit on the front.

**Why:** the laminate is single-sided — copper on one face, bare substrate on
the other. That is not a layer *choice*, it is what the material is. The
earlier design had copper on `F.Cu`, which describes a board whose copper is on
the component side; no such board was going to be made.

The build is the standard through-hole one: parts on the bare face, leads
through the holes, soldered on the copper face. Nothing about the routing
changed — the same 69 segments and the same pour, on the other side of the
board.

**Cost:** nothing electrically. Through-hole pads are defined on `*.Cu`, so they
already existed on both layers and needed no edit.

**What it does add is a way to ruin a board.** KiCad draws every layer from the
top, so a `B.Cu` export is a view *through* the laminate. Engraving it as
exported puts a mirrored image on the copper, and every footprint lands
reversed. The artwork has to be flipped before it reaches the laser — see
`docs/fabrication.md`, which carries the check for it.

## Open items

- **ANATEL limits** for 433 MHz short-range transmission in Brazil. Not
  researched. **Blocking** for any transmission outside a dummy load.
- **CC1101 module pin order.** Varies between revisions. Check the silkscreen on
  the module in hand against the table in `docs/hardware.md` before routing;
  crossing VCC and GND destroys it.
- **Received-signal noise floor.** Unknown until it is on the air. The decoder's
  minimum-mark filter is a guess (`RADIO_RX_MIN_MARK_MS`) and wants tuning
  against the real band.
