# Design decisions

A record of what was chosen, what was rejected, and why. Written so the reasoning
survives — if a decision gets revisited, the tradeoff is here rather than being
re-derived from scratch.

This board is a deliberate subtraction from [Sparks](../../sparks), so several
entries below are of the form "Sparks decided X; here is why that decision does
not apply." Where a decision carried over unchanged, it says so rather than
restating the argument.

## 1. No LED panel — this is the decision everything else follows from

**Decision:** drop the 26-LED Morse tree panel and the MAX7219 that drives it.
Output is the buzzer and the serial monitor.

**Why:** the panel is the *whole* reason Sparks is a complex board. Removing it
is not a small saving, and the cascade is worth laying out because each step
removes something that only existed to support the step before it:

| Removed | Because |
|---|---|
| 26 SMD LEDs | no panel |
| MAX7219 (DIP-24, 24 holes) | nothing to drive |
| `RSET` resistor, 2 decoupling caps | no MAX7219 |
| Level shifter (conditional) | no 5 V logic to shift |
| Boost converter (HW-357) | nothing needs 5 V any more |
| LiPo cell, charge circuit | no boost, so USB is enough |
| Battery-sense divider (2 R) | no battery |
| The second copper layer | the matrix was what needed it |
| Two-sided laser registration | one face only |

35 parts become 8; 29 SMD parts become 0.

**Tradeoff accepted:** the panel was Sparks' teaching mechanism — seeing *where a
letter lives in the tree* rather than being told what it was. That is genuinely
lost, and this board does not replace it. It is a different device: Sparks is for
learning Morse, this is for sending it.

**Not a regression to fix later.** Adding the panel back means the MAX7219, which
means 5 V, which means the boost, which means two layers. The simplicity is a
package deal, and [Sparks](../../sparks) already exists for people who want the
panel.

## 2. Through-hole only

**Decision:** every part on the board is through-hole. No SMD, not even passives.

**Why:** Sparks used 0805 parts throughout, but that was driven by the LEDs — 26
of them had to sit flush against an engraved panel, which only SMD achieves, and
once the LEDs were SMD the passives followed for consistency. Neither reason
survives the panel's removal.

Through-hole then wins on the things that actually matter here:

- **Soldering.** On a single-layer board, through-hole parts are soldered from
  the face *without* components. There is nothing to work around, no tweezers,
  no reflow, no solder paste.
- **Laser.** Every footprint sits on a 2.54 mm or wider grid, which no fiber
  laser struggles with. Characterising the minimum trace/space stops being a
  prerequisite.
- **Serviceability.** Parts can be desoldered and replaced with a plain iron.

**Tradeoff accepted:** a larger board, and 34 holes to drill. Both are cheap
here — the board is already sized by the Supermini and the CC1101 module, not by
the passives, so SMD would not shrink it much.

## 3. Single layer, no jumpers

**Decision:** route on one face, no jumper wires.

**Why it is possible:** Sparks' §8 found the 26-LED matrix needs ~56 crossings on
one layer, because the matrix is organised by tree *level* while the panel is
organised by tree *geometry* — two different organisations of the same 26 parts
that do not reconcile on one face. That finding is specific to the matrix. With
it gone, what remains is six parallel traces between two headers, two signal
traces to a switch and a buzzer, and power — which has no crossings at all.

**Ground is a perimeter trace, not a pour.** On a single-layer board a pour would
have to route around every signal trace and would end up split. Instead every
ground-connected part is placed at the board edge and ground runs around the
perimeter with short stubs inward. This is a **placement constraint**, not just a
routing one — see the sketch in `docs/hardware.md`.

**Rejected:** double-sided "because it is easier to route." It is, but it costs
two-sided laser registration and alignment holes for a board that does not need
it. Single-layer is the point of this design; if the routing ever genuinely
requires a second layer, that is a signal the scope crept.

## 4. USB-C power, with battery pads

**Decision:** powered from the ESP32-C3 Supermini's own USB-C port. A 2-pin
header (`J3`) brings `VIN`/`GND` to the edge, unpopulated.

**Why:** Sparks needed a battery because a handheld trainer is used away from a
desk, and it needed 5 V because of the MAX7219 — which from a 3.7 V cell meant a
boost converter. That chain produced the HW-357 module, a trimmer that ships
uncalibrated and destroys the board if it is not set to 5.0 V before connecting,
and a ~35% efficiency tax on runtime.

Here the CC1101 and the ESP32-C3 are both 3.3 V natively. There is no 5 V rail to
produce, so even if a battery is added later it feeds `VIN` directly with no
boost. The dangerous part of Sparks' power design was a consequence of the
MAX7219, and it is gone.

**Why pads rather than a battery now:** the board's first job is bench work —
talking to a second unit, testing into a dummy load, settling the ANATEL
question. All of that happens next to a computer, where USB is already connected
for the serial monitor anyway. Two pads keep the option open at the cost of one
header.

**Rejected:** fitting the battery now. It is a cell, a charge module, a switch,
and a sense divider — four parts and their traces — for a capability nothing
currently needs.

## 5. Transceiver, not just a transmitter

**Decision:** the device receives as well as transmits. Incoming carrier is
decoded to letters and played on the buzzer at a different pitch.

**Why:** it is nearly free. The CC1101 does OOK in both directions with the same
async configuration, and the decoder that times the key is *the same decoder*
that times the received signal — a mark is a mark whether a finger or a carrier
produced it. `decoder.cpp` does not know which source it is handling.

What it buys is that two boards talk to each other, which makes the device
testable without a second radio of any other kind, and makes it useful as a pair.

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

## 7. Pin assignment matches Sparks exactly

**Decision:** the key stays on GPIO10, the buzzer on GPIO20, and the CC1101 on
GPIO0–4 — identical to Sparks.

**Why:** the two projects share `key.cpp`, `sidetone.cpp` and `morse_tree.h`
essentially verbatim. Keeping the pins identical means code moves between them
with no re-mapping, and a fix to the key debouncing can be copied across without
reading it carefully first.

The three MAX7219 pins (5, 6, 7) and the battery sense (21) are simply freed,
joining the two Sparks already kept spare. GPIO7 then goes back out to the
CC1101's GDO2 — wired unused, because a trace on an unpopulated net is free and
adding one to a finished single-layer board is not — leaving **five spare
GPIOs**.

**GPIO8/GPIO9 stay free** for the same reason as in Sparks: they are the
strapping pins and the I²C defaults, so they are the least convenient pins to
commit. With five spare there is no pressure to, and an I²C OLED showing decoded
text is now a realistic addition rather than something the pin budget forbids.

## 8. The buzzer carries two pitches

**Decision:** 700 Hz for your own keying, 550 Hz for received signal.

**Why:** on Sparks the buzzer was a secondary output — the panel was the display,
and the sidetone just confirmed the rhythm. Here it is the only real-time output
the device has, and it has to serve two directions at once. Two pitches make them
distinguishable by ear without looking at the serial monitor, which matters
because the whole point of keying is not looking at a screen.

700 Hz is the classic CW sidetone pitch, kept from Sparks. 550 Hz is far enough
below it to be unmistakable and still comfortable.

**Requires a passive buzzer.** An active buzzer contains its own oscillator and
plays one fixed note, ignoring `tone()` entirely. This was already true on Sparks
but matters more here, since the pitch distinction is now load-bearing.

## 9. The SPI pin assignment was chosen by the router, not by hand

**Decision:** MOSI on GPIO6, SCK on GPIO5, MISO on GPIO2 — replacing the
original GPIO2/3/4, which had been picked to match Sparks.

**Why:** the first autoroute attempt failed. Not marginally — it left a net
unroutable and stalled. The cause was not the router: the straight-line paths
between the connector and the MCU **crossed each other five times among seven
nets**, because the connector's left-hand pins were assigned to the MCU's
right-hand pins and vice versa.

On a two-layer board a crossing is a via. On a single-layer board it is a trace
that **cannot be routed at all**, so five crossings means the placement was
unroutable no matter how long the router ran.

**What makes the fix legal:** every SPI line here is bit-banged
(`radio.cpp`, `transfer_()`), not driven by a hardware SPI peripheral. There is
no fixed pin mapping to respect — any GPIO can carry any of these signals, and
`docs/hardware.md` had already flagged the assignment as adjustable during
layout. So the mapping is free, and the right way to pick it is to search for
the permutation with no crossings rather than to guess.

The search was restricted to **GPIO0–7**. Allowing GPIO8, GPIO9 and GPIO21 gave
a crossing-free solution 41 mm shorter, but those are the strapping pins and the
I²C defaults that §7 keeps free for an OLED. The shorter route was not worth
spending them.

**Result:** zero crossings, GPIO8/9/21 still free, three `#define` lines changed.

**What this costs:** the CC1101 pins no longer match Sparks. That is a real loss
— §7 kept them identical so code could move between the boards unread — but it
applies only to `radio.cpp`, which Sparks does not have. The files the two
projects genuinely share, `key.cpp` and `sidetone.cpp`, are on GPIO10 and GPIO20
on both boards and are unaffected.

**The general lesson, worth keeping:** when an autorouter fails on a trivial
board, the placement or the assignment is wrong, not the router's settings. Count
the crossings before touching anything else — on one layer that number is a hard
bound on what is routable.

## Open items

- **ANATEL limits** for 433 MHz short-range transmission in Brazil. Not
  researched. **Blocking** for any transmission outside a dummy load — carried
  over unresolved from Sparks §6.
- **CC1101 module pin order.** Varies between revisions. Check the silkscreen on
  the module in hand against the table in `docs/hardware.md` before routing;
  crossing VCC and GND destroys it.
- **Received-signal noise floor.** Unknown until it is on the air. The decoder's
  minimum-mark filter is a guess (`RADIO_RX_MIN_MARK_MS`) and wants tuning
  against the real band.
