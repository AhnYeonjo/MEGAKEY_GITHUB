# MegaKey — JAMMA / Arcade Controller Firmware Collection

Firmware for custom arcade hardware: JAMMA boards, KVM switching, and console adapters.

> **Author**: An Yeon-jo
> **Boards**: AVR128DB64 · ATmega32U4 (Pro Micro) · RP2040 (GP2040-CE) · STM32

---

## Repository layout

```
MEGAKEY/
├── firmware/              # Arduino / microcontroller sketches
│   ├── MegaKey_AVR128DB64/    # Main board (v4.0, AVR128DB64-I/PT)
│   ├── MegaKey_IO/            # Previous-generation board (Pro Micro JVS IO)
│   ├── MultiSourceController/ # Multi-source → JAMMA converter (CXA2025AS)
│   ├── PowerController/       # Sequenced power controller
│   └── IR_SignalAnalyzer/     # IR (NEC) signal analysis utility
│
├── rp2040/                # RP2040 / GP2040-CE
│   ├── addons/                # Serial Input Addon (v2.4)
│   └── firmware/              # Custom board definition (Waveshare RP2040-Zero)
│
└── docs/                  # Circuit diagrams, pinmaps, reference HTML
```

---

## Modules

### `firmware/MegaKey_AVR128DB64` — main board (v4.0)

AVR128DB64-I/PT based arcade controller board.

**Hardware**
- AVR128DB64 (TQFP-64, 24 MHz internal OSCHF, 5 V)
- 74HC595 × 2 (KVM LEDs · status LEDs · MUX control)
- GP2040-CE × 2 (one-way transmission over Serial2/Serial3)
- JVS IO (USART0, TCB1 sense signal)
- HDMI switch IR (NEC, TCB0 PWM on PA2)

**Build environment**
- Arduino IDE + [DxCore](https://github.com/SpenceKonde/DxCore)
- Board: AVR DB-series → AVR128DB64 (Reset: GPIO PF6, Clock 24 MHz internal)
- Libraries: [JVSIO](https://github.com/twitter/jvsio) (bundled in `jvsio/`) · IRremote v4.x

### `firmware/MegaKey_IO` — previous-generation board

ATmega32U4 (Pro Micro) based JVS IO controller. The `jvsio/` subdirectory contains the library source.

### `firmware/MultiSourceController` — multi-source → JAMMA

CXA2025AS + 74HC4052 + LM1881 controller. Inputs: composite · S-Video · VGA 15kHz → Output: JAMMA RGB. OLED (SSD1306) UI.

### `firmware/PowerController` — power sequence controller

### `firmware/IR_SignalAnalyzer` — IR signal analysis utility

Diagnostic sketch that decodes and analyzes the NEC IR signals used to drive the HDMI switch.

### `rp2040/addons` — GP2040-CE Serial Input Addon (v2.4)

GP2040-CE add-on that receives input over UART on an RP2040-Zero and relays it as Gamepad HID.

> ⚠️ **Required settings**: disable the Display Addon and PlayerLEDAddon (to avoid I2C and WS2812 PIO conflicts).

### `rp2040/firmware/BoardConfig.h` — Waveshare RP2040-Zero board definition

Custom GP2040-CE board pinmap.

---

## Documentation

| Document | Contents |
|---|---|
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | **Start here** — whole-system composition, module map, wiring index, KVM truth table, mode table |
| [`docs/rp2040-gp2040-addon.md`](docs/rp2040-gp2040-addon.md) | GP2040-CE Serial Input Addon: CompactMessage wire format, 2-player integration with the Arduino, build/flash guide |
| [`docs/firmware-MegaKey_AVR128DB64.md`](docs/firmware-MegaKey_AVR128DB64.md) | Main board v4.0 module doc |
| [`docs/firmware-MegaKey_IO.md`](docs/firmware-MegaKey_IO.md) | Previous-gen Mega board (v3.4) |
| [`docs/firmware-MultiSourceController.md`](docs/firmware-MultiSourceController.md) | Video converter (CXA2025AS) |
| [`docs/firmware-PowerController.md`](docs/firmware-PowerController.md) | Power sequencer & lighting |
| [`docs/firmware-IR_SignalAnalyzer.md`](docs/firmware-IR_SignalAnalyzer.md) | IR capture debug utility |
| [`docs/circuit-ir-usb-mux.md`](docs/circuit-ir-usb-mux.md) | IR LED driver circuit, USB mux control, BOM, PCB layout rules |
| [`docs/sn74cb3q3253-mux.md`](docs/sn74cb3q3253-mux.md) | SN74CB3Q3253PW control semantics and wiring notes |

---

## License

Each module follows its own SPDX header. Where no notice is present, the source is MIT.
