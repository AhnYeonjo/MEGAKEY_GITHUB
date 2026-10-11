# MegaKey_IO — Previous-Generation Board (v3.4)

Target: **Arduino Mega 2560 PRO** (ATmega2560) · the direct predecessor of
`MegaKey_AVR128DB64` (v4.0). Same architecture and same wire protocol — the v4.0
board is a port of this sketch to the AVR DB series.

## Role

Identical to the main board: scans the 1P/2P panels at 1 kHz (`LOOP_INTERVAL_US =
1000`), broadcasts the 13-byte `CompactMessage` on `Serial2`/`Serial3` to the two
GP2040-CE boards, drives the KVM USB multiplexer + status LEDs, sends NEC IR to the
HDMI switch, and acts as a JVS IO device in `MODE_JVS (7)`.

Wire format and mode table: [`rp2040-gp2040-addon.md`](rp2040-gp2040-addon.md).

## Differences vs. v4.0 (AVR128DB64)

| Aspect | Mega v3.4 (this) | AVR128DB64 v4.0 |
|---|---|---|
| MCU | ATmega2560 16 MHz | AVR128DB64 24 MHz internal |
| IR timer | `#define IR_USE_AVR_TIMER3` (forced, avoids Timer2/JVS clash) | DxCore assigns TCB automatically |
| JVS sense | Timer2 | TCB1 PWM |
| UART registers | `UCSR2B/UCSR3B` | `USART2/3.CTRLB` |
| LED/MUX pins | raw GPIO + UART RX disabled to free pins 15/17 (`UCSR2B &= ~(1<<RXEN2)` etc.) | 74HC595 shift-register bits |
| Library layout | `jvsio/` in sketch folder (bundled) | same |

## Files

| File | Purpose |
|---|---|
| `MegaKey_IO.ino` | Full sketch (v3.4-TIMER_FIX) |
| `jvsio/` | Bundled JVSIO library incl. `clients/MegaClient.h` |

## Build

Arduino IDE, board *Arduino Mega 2560*, libraries: JVSIO (bundled), IRremote (v3-style
`<IRremote.h>` include). No special fuse changes needed.
