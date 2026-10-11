# Hardware Designs (EasyEDA)

PCB/schematic designs corresponding to each firmware module, archived from the
developer's EasyEDA workspace (exports dated 2026-05-27).

- **Source project (login required):**
  <https://pro.easyeda.com/editor#id=96b24b612eda4555ad36c350f42e1f88>
  The live editor requires the project owner's account; the files here are the
  offline snapshots of that design.
- The PDFs are raw EasyEDA schematic exports — component notes/silkscreen text
  inside them are kept as designed. The authoritative English descriptions of
  every circuit are the [`docs/`](../docs/) pages.

## Firmware ↔ PCB map

| PCB design | Files | Key ICs | Firmware module |
|---|---|---|---|
| Main board **v3.4** | [`MegaKey_IO_PCB/Schematic_MegaKey_IO_v3.4.pdf`](MegaKey_IO_PCB/Schematic_MegaKey_IO_v3.4.pdf) | `MEGAPRO` (ATmega2560), 2× `RP2040_ZERO_1P/2P` footprints, MUX1/2_SEL/OE nets, IR_LED | [`firmware/MegaKey_IO`](../firmware/MegaKey_IO/) |
| KVM sub-board | [`MegaKey_IO_PCB/Schematic_USB_MUX_HDMI_Selector.pdf`](MegaKey_IO_PCB/Schematic_USB_MUX_HDMI_Selector.pdf) | USB mux (SN74CB3Q3253), USB hub, IR LED header, AMS1117-3.3, KF2EDR 8-pin main connector | [`docs/circuit-ir-usb-mux.md`](../docs/circuit-ir-usb-mux.md), [`docs/sn74cb3q3253-mux.md`](../docs/sn74cb3q3253-mux.md) |
| Main board **v4.0** (netlist + sheets) | [`MegaKey_AVR128DB64_PCB/MegaKey_v4.tel`](MegaKey_AVR128DB64_PCB/MegaKey_v4.tel), [`sheet1_mcu_core.svg`](MegaKey_AVR128DB64_PCB/sheet1_mcu_core.svg), [`sheet2_shift_registers.svg`](MegaKey_AVR128DB64_PCB/sheet2_shift_registers.svg) | **AVR128DB64-I/PT** QFP-64 (U1), 2× 74HC595D SOIC-16 (U2,U3), 1N5817 Schottky, UPDI 1×6 header | [`firmware/MegaKey_AVR128DB64`](../firmware/MegaKey_AVR128DB64/) |
| Video converter | [`MultiSourceController/Schematic_MultiSourceController.pdf`](MultiSourceController/Schematic_MultiSourceController.pdf) | **ATmega32U4** (Pro Micro), CXA2025AS, 74HC4052, LM1881, SSD1306 header | [`firmware/MultiSourceController`](../firmware/MultiSourceController/) |
| Power sequencer | [`PowerController/Schematic_PowerController.pdf`](PowerController/Schematic_PowerController.pdf) | **ATmega32U4** (Pro Micro), 5× G5NB-1A-E DC5 relays, 1N4007, IRLZ44N PWM | [`firmware/PowerController`](../firmware/PowerController/) |

## v4.0 main board — JLCPCB part numbers

From `MegaKey_v4.tel` (EasyEDA netlist) and the v4.0 design notes:

| Ref | Part | Package | LCSC # |
|---|---|---|---|
| U1 | AVR128DB64-I/PT | TQFP-64 (10×10, 0.5 mm) | C2912655 |
| U2, U3 | 74HC595D | SOIC-16 | C5947 |
| D1, D2 | 1N5817 (Schottky) | SOD-123 | C8598 |
| R_TX | 1 kΩ 1% | 0603 | C21190 |

Migration note: the v4.0 board is the EasyEDA **clone-and-replace** of the v3.4
board — the `MEGAPRO` module is removed and the freed nets (1P_/2P_ signals,
MUX/IR lines) are rewired to the AVR128DB64 and the 74HC595 pair, exactly as
documented in [`docs/firmware-MegaKey_AVR128DB64.md`](../docs/firmware-MegaKey_AVR128DB64.md).
