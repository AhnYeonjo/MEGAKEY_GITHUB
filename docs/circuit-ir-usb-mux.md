# IR LED Driver & USB Multiplexer Circuit

Replaces the legacy `complete_circuit_diagram.html`, whose pin map contradicted the
firmware. Everything below is cross-checked against the sketches in this repo.

## 1. IR LED transmitter (HDMI switch control)

```
MCU IR pin ──[ 1kΩ ]──┬── base ─ 2N2222 (NPN) ── emitter ── GND
                      │                  collector
                 (optional              
                  pulldown)              ┌── anode ── IR LED 940nm ──┐
                                         │        (cathode to GND)   │
                                    +5V ─┴──[ 100Ω ]─────────────────┘
```

| Item | Value |
|---|---|
| Signal | NEC protocol, 38 kHz carrier |
| Drive | 2N2222 low-side switch |
| Base resistor | 1 kΩ |
| LED series resistor | 100 Ω (from +5 V) |

**IR pin by board generation** (the old diagram showed Mega `D46` — wrong):

| Board | IR pin | Timer | Evidence |
|---|---|---|---|
| Mega 2560 (v3.4) | **D3** | Timer3 (`IR_USE_AVR_TIMER3`) | `#define IR_LED_PIN 3` in `firmware/MegaKey_IO/MegaKey_IO.ino` |
| AVR128DB64 (v4.0) | **PA2** | TCB0 PWM | `#define IR_LED_PIN PIN_PA2` in `pins_megakey.h` |

Mount the LED aimed at the HDMI-switch receiver. Use
[`firmware/IR_SignalAnalyzer`](../firmware/IR_SignalAnalyzer/) to capture/verify codes.

## 2. USB multiplexer (KVM channel select)

Two **SN74CB3Q3253PW** dual 2:1 FET muxes switch the pair of GP2040-CE USB data
lines to one of four hosts; see [`sn74cb3q3253-mux.md`](sn74cb3q3253-mux.md) for the
chip-level wiring.

### Control signals by board generation

The old diagram claimed GPIO `A6–A9` on the Mega — wrong (A7/A9 are the 1P RB/LB
button pins on that board). Actual assignments:

| Signal | Mega 2560 (v3.4) — direct GPIO | AVR128DB64 (v4.0) — 74HC595 bit |
|---|---|---|
| MUX1_SEL | `A10` | bit 6 |
| MUX1_OE  | `A12` | bit 7 |
| MUX2_SEL | `A14` | bit 8 |
| MUX2_OE  | `A15` | bit 9 |

Evidence: `#define MUX*_PIN` + `setKvmChannel()` in `firmware/MegaKey_IO/MegaKey_IO.ino`;
`SR_BIT_MUX*` + `updateKvm()` in `firmware/MegaKey_AVR128DB64/`.

### Channel truth table

`X` = don't care. OE is **active low** (0 = chip enabled, 1 = Hi-Z/disconnected).

| Channel | MUX1_SEL | MUX1_OE | MUX2_SEL | MUX2_OE | USB path |
|---|---|---|---|---|---|
| CH1 | 0 | 0 | X | 1 | MUX1 common → PC1 |
| CH2 | 1 | 0 | X | 1 | MUX1 common → PC2 |
| CH3 | X | 1 | 0 | 0 | MUX2 common → PC3 |
| CH4 | X | 1 | 1 | 0 | MUX2 common → PC4 |
| CH5 (JAMMA) | X | 1 | X | 1 | **all USB blocked** — pads go through JVS, HDMI video only |

## 3. Bill of materials

**IR circuit**
- IR LED 940 nm × 1
- 2N2222 NPN transistor × 1
- Resistor 1 kΩ × 1, 100 Ω × 1

**USB multiplexer**
- SN74CB3Q3253PW × 2
- 0.1 µF ceramic decoupling capacitor × 2
- USB Type-A ports × 4–5, USB Type-B (upstream) × 1–2

**Optional**
- TVS diodes (ESD protection on USB ports)
- Ferrite beads (noise filtering)
- Status LEDs (KVM channel indicators — driven from 74HC595 bits 0–4 on v4.0)

## 4. Important notes

- The SN74CB3Q3253PW runs on a **3.3 V supply**; its I/Os are 5 V tolerant.
- Maintain **90 Ω ±10%** USB differential impedance.
- Keep the D+/D- length difference within **5 mm**.
- Place 0.1 µF decoupling capacitors next to each MUX chip.
- When CH5 is selected both chips are disabled, so the hosts see the device unplug
  (this is intended: JAMMA uses the JVS bus, not USB).

## 5. PCB layout recommendations

- **Layer stackup:** minimum 2 layers (signal/GND); 4 layers recommended
  (signal/GND/power/signal).
- **Trace width:** USB data 0.2 mm; power ≥ 0.5 mm.
- **Vias:** minimize on high-speed paths (≤ 2 per channel).
- **Ground:** unbroken ground plane under the differential pairs.
- **Placement:** keep the MUX chips close to the USB connectors.
- **ESD:** TVS diode on every external USB port.
