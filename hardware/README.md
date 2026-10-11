# Hardware — PCB fabrication packages

JLCPCB / EasyEDA fabrication archives for the MegaKey boards. Each `.zip` is a
manufacturer order package containing the production **Gerber** layers (copper `bl`/`tl`,
soldermask `bs`/`ts`, silkscreen `ko`, drill `drl`), **pick-and-place** data (`yg/`), and
**3-D renders** (`*_top.png`, `*_snapshot.png`). The board silkscreen / netlist inside
each package has been verified 1:1 against the firmware in this repository.

> The matching editable schematic/PCB source lives in the original EasyEDA project
> (the online editor requires the designer's login). These zips are the fabricated
> output of that design.

## Package index

| Archive | Board | MCU / key parts | Firmware module |
|---|---|---|---|
| [`MegaPCB.zip`](MegaPCB.zip) | Main IO board (v3.4) | `MEGA2560PRO-EMBED` + `RP2040_ZERO_1P/2P`, `JAMMA1/2`, `JVSIO`, `DREAMCAST_1P/2P`, MUX control | [`../firmware/MegaKey_IO`](../firmware/MegaKey_IO) |
| [`POWERPCB.zip`](POWERPCB.zip) | Cabinet power board | ATX PSU control + `K1..K7` relays (JAMMA / TTX3 / Naomi / AC1 / AC2), `IRLZ44N` lighting MOSFET, `12V_PWN`, brightness pot | [`../firmware/PowerController`](../firmware/PowerController) |
| [`USBMUX.zip`](USBMUX.zip) | USB KVM switch | `U1`/`U2` = **FSUSB42** USB 2.0 HS switches, `USB_HUB`, `AMS1117-3.3`, `PC1..PC4` | `setKvmChannel()` in MegaKey_IO / AVR128DB64 |

## POWERPCB — ATX cabinet power

`POWERPCB` exists to use a standard **ATX PC power supply as the arcade cabinet's
main power source** (the VEWLIX cabinet). The board drives the ATX and the per-console
loads: relay groups select which console / harness (JAMMA, TTX3, Naomi, AC outlet 1/2)
is powered, and an `IRLZ44N` MOSFET dims the cabinet lighting from a potentiometer. The
firmware's boot light animation never toggles the relay pins, to avoid relay misoperation
on shared outputs.

## Gap — v4.0 AVR128DB64 board

The current main controller (`firmware/MegaKey_AVR128DB64`, v4.0) has **no fabrication
zip in this folder** — its design source is only in the EasyEDA project. Add its JLCPCB
export here when it is fabricated.
