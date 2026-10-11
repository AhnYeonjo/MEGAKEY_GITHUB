# PowerController — Cabinet Power Sequencer & Lighting

Target: **Arduino Pro Micro** (ATmega32U4). Controls 5 relay groups (with shared
button LEDs) and the cabinet lighting via a PWM MOSFET.

## Outputs

| Pin | Relay/LED group | Feeds |
|---|---|---|
| 4 | K1/K2/K3 + BTN_LED1 | JAMMA harness |
| 5 | K4 + BTN_LED2 | TTX3 |
| 6 | K5 + BTN_LED3 | Naomi |
| 7 | K6 + BTN_LED4 | AC outlet 1 |
| 8 | K7 + BTN_LED5 | AC outlet 2 |
| 9 | LED PWM (IRLZ44N) | main lighting strip |

## Inputs

Latching switches: JAMMA `A2`, TTX3 `A1`, Naomi `A0`, AC1 `15`, AC2 `14`
(all `INPUT_PULLUP`). Brightness potentiometer on `A3`.

## Behaviour

- On boot: all relay outputs LOW (off), then a **welcome light animation** that only
  touches the PWM lighting pin — it deliberately never toggles the shared
  relay/BTN_LED pins, so relays cannot chatter during the animation.
- Each switch latches its group; lighting brightness tracks the potentiometer
  (`analogRead/4`, floored at 32 for animation visibility).

## Build

Arduino IDE, board *Pro Micro (5V/16MHz)*, no external libraries. Relays through
driver transistors with flyback diodes; do not drive relay coils from the GPIO
directly.
