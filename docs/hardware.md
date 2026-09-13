# Hardware notes

## Block diagram

```
   [KEY] ──────▶ ┌──────────────┐            ┌──────────┐
                 │  ESP32-C3    │ ──SPI───▶  │  CC1101  │ ──▶ SMA ──▶ whip
   [BUZZER] ◀─── │  Supermini   │ ◀──GDO0──▶ │  module  │
                 │              │ ╌╌GDO2╌╌╌  │          │   (wired, unused)
                 └──────────────┘            └──────────┘
                        │
                    USB-C (power + serial)
```

Everything on the board is 3.3 V. The ESP32-C3 Supermini's onboard regulator
supplies the CC1101 directly; there is no boost, no level shifter and no charge
circuit. That is the whole supply design.

The ESP32-C3 does all decision-making: key timing, dot/dash classification, tree
walking, letter identification, gap detection, radio keying, sidetone, and
decoding the received signal. The CC1101 is a dumb carrier switch in both
directions — see *OOK and why there is no packet format* below.

## Pin assignment (ESP32-C3 Supermini)

The Supermini breaks out GPIO 0–10, 20, 21 — 13 usable pins.

| GPIO | Function | Socket pin | Notes |
|---|---|---|---|
| 2 | CC1101 MOSI | 3 | |
| 3 | CC1101 SCK | 4 | |
| 4 | CC1101 MISO | 5 | |
| 7 | CC1101 GDO2 | 6 | wired, unused — see below |
| 0 | CC1101 GDO0 | 7 | TX key out / RX data in |
| 1 | CC1101 CSN | 8 | |
| 10 | Morse key | — | `INPUT_PULLUP`, active low |
| 20 | Buzzer | — | sidetone |

Listed in socket-pin order, because that is the order the wiring follows.

**Total: 8 of 13 — 5 spare (GPIO5, 6, 8, 9, 21).**

### GDO2 is wired but not used

Nothing in the firmware reads it. Keying and carrier detection both run on
GDO0, and a second status pin adds nothing to the current design.

It is connected anyway because the pin cost nothing and the alternative was
discovering a use for it after the board was etched. A trace on an unpopulated
net is free; adding one to a finished single-layer board is not.

The use worth keeping in mind is **carrier sense** (`IOCFG2 = 0x0E`): the chip
asserts the pin when RSSI crosses a threshold, giving a hardware squelch. That
would let the receiver reject band noise *before* it reaches the timing code,
which is the weakest part of the RX path today — see *OOK and why there is no
packet format* and `RADIO_RX_MIN_MARK_MS` in `firmware/cc1101-morse/radio.h`.

Until then it is configured three-state (`0x2E`) and held as an input on the
ESP32-C3 side, so neither chip drives it.

The pin numbers match Sparks exactly for the parts they share, so firmware moves
between the two boards without re-mapping. The freed pins are the three the
MAX7219 used (5, 6, 7), the battery sense (21), and the two Sparks already kept
spare (8, 9) — of which GPIO7 is spent again on GDO2 above, leaving five.

GPIO8/GPIO9 are the strapping pins and the I²C defaults; leaving them free keeps
room for an I²C peripheral — a small OLED for the decoded text is the obvious
candidate, and with five pins spare there is no longer any pressure against it.

Pin numbers above are a starting proposal — adjust during KiCad layout if routing
prefers a different assignment. Only the CC1101 SPI grouping is semi-fixed.

## Power supply

**USB-C only.** The ESP32-C3 Supermini has a USB-C socket and an onboard 3.3 V
regulator. Plug it in and the board runs.

| Rail | Source | Feeds |
|---|---|---|
| `+3V3` | Supermini onboard regulator | CC1101, buzzer, key pull-up |
| `GND` | — | everything |

This is a genuine simplification rather than a deferral, and it follows directly
from dropping the MAX7219. Sparks needed 5 V *only* for that chip, and needing
5 V from a 3.7 V cell meant a boost converter, which meant the HW-357 module, a
trimmer that ships uncalibrated and can destroy the board if it is not set first,
and a ~35% efficiency tax on battery life. None of that exists here.

### Battery pads (J3, unpopulated)

A 2-pin header brings `VIN` and `GND` out to the board edge:

| Pin | Net |
|---|---|
| 1 | `VIN` (Supermini 5V/VIN pin) |
| 2 | `GND` |

To go portable later: a LiPo cell and a TP4056 charge module wire to these two
pins and nothing else changes. The CC1101 and ESP32-C3 are both 3.3 V, so a
3.7 V cell feeds `VIN` directly and the onboard regulator handles it — **no
boost converter**, which is the part of Sparks' power chain that caused all the
trouble.

If that happens, add a resistor divider into a spare ADC pin (GPIO21 is already
free and was Sparks' battery sense) for a charge readout. Not on the board now:
an unpopulated divider is two more parts and two more traces for a feature that
does nothing while the board runs on USB.

## Power budget

There is no battery, so this only sizes the USB draw.

| Load | Draw |
|---|---|
| ESP32-C3 active | ~30 mA |
| CC1101 RX | ~16 mA |
| CC1101 TX @ 10 dBm | ~35 mA |
| Buzzer | ~20 mA |

- **Receive (idle listening): ~46 mA**
- **Transmit with sidetone: ~85 mA**

Any USB port supplies this without thinking about it. Worth recording because it
sets the cell size *if* the battery option is ever taken: 85 mA worst case means
a 1000 mAh cell runs roughly 12 hours — and unlike Sparks there is no boost
converter eating 35% of it, so that number is real rather than derated.

## OOK and why there is no packet format

The CC1101 is normally used as a packet radio: you hand it bytes, it adds a
preamble, sync word and CRC, and the receiver hands bytes back. **None of that is
used here.**

Morse on a radio is **OOK** — on-off keying. The carrier is switched on for a
mark and off for a space, and the *timing of those switches is the message*.
There is nothing to packetise: the dot/dash structure already encodes everything,
and wrapping it in a frame would mean buffering the operator's keying, sending it
as a block, and replaying it at the far end. That destroys the thing that makes
Morse Morse, which is that it is live.

So the chip is configured as a carrier switch in both directions:

- **Transmit** — `MOD_FORMAT = OOK`, `GDO0` configured as the TX data input in
  async transparent mode. The firmware drives GDO0 high and the carrier comes on;
  drives it low and it goes off. Exactly like keying a transmitter.
- **Receive** — same mode, opposite direction. GDO0 becomes an output that
  follows the detected carrier, and the firmware times that pin with the same
  code that times the key.

One decoder serves both, which is the nice consequence: a received mark and a
keyed mark are the same event with a different source, so `decoder.cpp` does not
know or care which it is handling.

### Register settings that matter

| Register | Value | Why |
|---|---|---|
| `MDMCFG2` | `0x30` | OOK modulation, no sync word, no Manchester |
| `PKTCTRL0` | `0x32` | async serial mode — no packet engine at all |
| `IOCFG0` | `0x0D` (RX) / `0x2E` (TX) | GDO0 = serial data out / high-impedance for TX input |
| `IOCFG2` | `0x2E` | GDO2 three-state — wired to GPIO7 but unused |
| `FREQ2/1/0` | 433.92 MHz | the band centre |
| `PATABLE` | `0xC0` | +10 dBm; lower it for bench work |

The full init table lives in `firmware/cc1101-morse/radio.cpp` — that is the
authoritative copy, and it carries per-register comments.

> [!NOTE]
> Async mode means **no error correction and no addressing**. Anything on the
> channel keys the receiver, including garage doors, weather stations and car
> fobs, which are all over 433 MHz. Expect noise-triggered garbage between real
> letters; `decoder.cpp` filters implausibly short marks, but it cannot
> distinguish a neighbour's doorbell from a deliberate dash.

## CC1101 module and antenna

The module is the **V2.0 SMA variant**, marked `433M`, with a gold SMA female
socket on the board edge. It ships with a short black whip antenna carrying a
male SMA plug, which **screws onto** that socket.

The antenna is therefore **detachable, not built in** — the two simply come in
the same bag. Nothing to buy, but the mechanical design has to account for it:

- The SMA socket sticks out from the module's edge and is rigid. It sets a hard
  constraint on where the CC1101 can sit inside the case.
- The case needs either an opening for the whip, or an internal antenna position
  with the whip removed. Antennas do not work well inside a sealed enclosure,
  especially near a copper ground plane.
- A panel-mount SMA extension (pigtail) is the usual fix: socket on the case
  wall, short coax to the module. Adds a part but frees the module's placement.
- The whip is removable, so it can be unscrewed for storage — worth designing for
  if the device is meant to be pocketable.

**Never key the transmitter with the antenna unscrewed.** With nothing to radiate
into, power reflects back into the PA and can damage it. For bench testing
without emitting, use a 50 Ω dummy load in place of the whip.

### The module is socketed, not soldered

The CC1101 is removable. It is an expensive part relative to the rest of the
board, it is the part most likely to be swapped (different band, different module
revision), and it carries the rigid SMA connector that constrains the case — all
reasons to keep it removable.

**The module has male pins, so the board carries a female socket** (`J2`, a 2×4
pin socket). The module plugs down into it. Worth stating explicitly because the
two footprints have different drill diameters and are not interchangeable — the
wrong one means a board that is etched and drilled but will not accept the part:

| | Board carries | Drill |
|---|---|---|
| `PinSocket_2x04` | a female socket body — **this board** | larger |
| `PinHeader_2x04` | male pins | smaller |

Check the module in hand rather than trusting this table: revisions vary, and
some ship with female headers instead.

| Socket pin | CC1101 | ESP32-C3 |
|---|---|---|
| 1 | VCC | `+3V3` |
| 2 | GND | `GND` |
| 3 | MOSI | GPIO2 |
| 4 | SCK | GPIO3 |
| 5 | MISO/GDO1 | GPIO4 |
| 6 | GDO2 | GPIO7 — wired, unused |
| 7 | GDO0 | GPIO0 |
| 8 | CSN | GPIO1 |

Taken from the silkscreen of the module in hand. **Sparks wires a different
order** (GND on 1, VCC on 2, SCK before MOSI) — its module is a different
revision. Do not copy one board's table onto the other; read the silkscreen of
the module you are actually plugging in.

Getting the supply pair backwards destroys the module, and on an etched board
that is not recoverable.

> [!NOTE]
> Pin order varies between CC1101 module revisions. Check the silkscreen on the
> module in hand against this table before routing — getting VCC and GND crossed
> destroys it.

## The buzzer must be passive

`tone()` generates a square wave at a chosen frequency. That requires a
**passive** buzzer, which is just a piezo element and sounds whatever it is
driven with. An **active** buzzer contains its own oscillator and plays one fixed
note whenever it has power — it ignores `tone()` entirely.

This matters more here than it did on Sparks, because the buzzer is now the only
real-time output the device has. It carries the sidetone on transmit *and* the
received signal on receive, and being able to shift pitch between the two is what
makes it possible to tell them apart by ear.

| Direction | Pitch | Why |
|---|---|---|
| Transmit (your key) | 700 Hz | classic CW sidetone |
| Receive (incoming) | 550 Hz | lower, so the two never blur together |

They are set in `firmware/cc1101-morse/sidetone.h`.

## Key debounce

The key is a plain tactile switch to ground with `INPUT_PULLUP`, debounced in
firmware at 15 ms (`KEY_DEBOUNCE_MS` in `key.h`). No RC network on the board.

15 ms is comfortably shorter than the shortest meaningful Morse symbol: a dot at
the 30 WPM ceiling is 40 ms. A hardware debounce would be two more parts for no
benefit.

## KiCad reference

Verified against the KiCad 10.0.5 stock libraries.

### Symbols

| Part | Library | Symbol |
|---|---|---|
| Tactile switch | `Switch` | `SW_Push` |
| Buzzer | `Device` | `Buzzer` |
| Resistor / capacitor | `Device` | `R` / `C` |

### Symbols NOT in the stock libraries

Both are boards mounted on headers rather than bare chips, so represent each with
a generic connector sized to its pin count:

- **ESP32-C3 Supermini** — stock libs carry only the bare die and the WROOM
  modules, not this board. Use the local `ESP32-C3-SuperMini` library in
  `hardware/kicad/libraries/`.
- **CC1101 module** — socketed, not soldered. Use a generic
  `Connector_Generic:Conn_02x04_Odd_Even` symbol with the footprint
  `Connector_PinSocket_2.54mm:PinSocket_2x04_P2.54mm_Vertical`. No dedicated
  symbol is kept: the board only carries the socket, so a generic connector
  describes it accurately.

### Footprints — all through-hole

| Part | Footprint |
|---|---|
| CC1101 socket | `Connector_PinSocket_2.54mm:PinSocket_2x04_P2.54mm_Vertical` |
| ESP32-C3 Supermini | local `ESP32-C3-SuperMini.pretty` |
| Tactile switch | `Button_Switch_THT:SW_PUSH_6mm` |
| Buzzer | `Buzzer_Beeper:Buzzer_12x9.5RM7.6` |
| Battery header | `Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical` |
| Decoupling cap | `Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm` |

**Nothing is SMD.** Sparks used 0805 parts throughout because 26 LEDs had to sit
flush against an engraved panel; without the panel there is no reason to. Every
part here is a through-hole part soldered from the underside, which is the
easiest possible assembly and matches the single-layer constraint — through-hole
joints on a one-sided board are soldered on the side *without* the components,
so there is nothing to work around.

## Single-layer routing

The board routes on one face with **no jumper wires**. This is the design's main
claim, so it is worth recording why it holds.

There are four things to connect and only three real nets bundles:

- **SPI + GDO0 + GDO2** — six parallel signals from the Supermini to the CC1101
  socket.
  Both parts are headers with 2.54 mm pitch, so the five traces run as a straight
  bundle between them with nothing to cross.
- **Key** — one trace to a switch, plus ground.
- **Buzzer** — one trace, plus ground.
- **Power** — `+3V3` and `GND` to the socket.

Ground is the only net that reaches everything, and on a single-layer board it
is routed as a **perimeter trace** around the board edge with short stubs inward,
rather than as a pour. Every ground-connected part sits at the board edge, which
is arranged deliberately:

```
   ┌──────────────────────────────────┐
   │  ○ J3                      BZ1 ○ │   ← ground perimeter
   │                                  │
   │   ┌──────────┐      ┌────────┐   │
   │   │ ESP32-C3 │═════▶│  J2    │   │   ← SPI bundle, 6 traces
   │   │ Supermini│      │ CC1101 │   │
   │   └──────────┘      └────────┘   │
   │                                  │
   │  ○ SW1                    SMA ▶  │
   └──────────────────────────────────┘
```

Placement rule for layout: **keep the CC1101 socket on the board edge with the
SMA pointing outward**, and put the Supermini directly beside it so the SPI
bundle is as short as possible. SPI at the CC1101's clock rates is not
especially fussy, but a short bundle also means a short board.

## Design rules

| Setting | Value |
|---|---|
| Track width | 0.4 mm |
| Clearance | 0.4 mm |
| Net classes | none — everything on `Default` |

**No net classes.** They exist to give groups of nets different physical rules,
and nothing here needs different rules: eight digital signals at Morse speeds,
plus power and ground. The SPI clock runs at a few hundred kHz and GDO0 switches
at the speed of a hand. Nothing is fast, high-current, or impedance-controlled.

0.4 mm is wider than a fab house's usual 0.25 mm default, which is the right
direction for laser etching — the cut widens as it bites, so the copper that
survives is narrower than drawn, and roughness at the trace edge matters more
than it would with photolithography. The board has twelve nets and acres of
spare copper, so the margin is free.

**Worth considering later: widen `GND` to ~0.8 mm.** Ground is routed as a
perimeter trace rather than a pour (see *Single-layer routing* above), which
makes it a long thin strip near the board edge carrying every return current
and absorbing whatever handling the board gets. The electrical case is nil at
85 mA; the mechanical one is real, and a partially-lifted ground trace fails
intermittently rather than cleanly — the worst kind of fault to chase. One net
class, no routing cost.

## Fabrication

Copper engraved with a fiber laser. Relevant consequences:

- **One face only.** Sparks needed two-sided etching with registration between
  the faces, and alignment holes to achieve it. Here there is nothing on the
  back, so the board goes on the laser once and comes off finished.
- **Trace count is low** — roughly 12 nets against Sparks' 13 plus 26 LED
  connections. Every trace is engraving time and a potential defect, so this is
  a real reduction in both.
- **2.54 mm pitch everywhere.** Every part is a through-hole header, switch or
  buzzer, all on a 2.54 mm or wider grid. That sidesteps needing to characterise
  the laser's minimum trace/space entirely — any fiber laser holds this
  comfortably.
- **Drilling:** roughly 34 holes (2×8 socket, 2×8 Supermini headers, 4 switch,
  2 buzzer, 2 battery header). All 0.8–1.0 mm, no fine work.
