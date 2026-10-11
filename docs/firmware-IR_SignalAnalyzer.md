# IR_SignalAnalyzer — NEC IR Capture Utility

Target: any 5 V AVR Arduino (built for the Mega generation). A **diagnostic sketch**,
not part of the runtime system: it captures raw IR pulse timings so you can identify
(or verify) the NEC codes your HDMI switch expects before wiring them into
`MegaKey_AVR128DB64`.

## How it works

- IR receiver module on **pin 2** (state flips on every mark/space edge), status LED
  on pin 13.
- Pulses are timestamped into `pulses[]` (max 100); a gap > `MIN_GAP (10 ms)` ends a
  signal, `SIGNAL_TIMEOUT (200 ms)` closes the capture window.
- Raw timings print over serial — decode against the NEC frame
  (9 ms leader + address/command + repeats) and paste the resulting code into the
  main board's IR table.
- This "fully revised" version fixes the **first-pulse overflow** problem of earlier
  captures (edge timing right after `attachInterrupt` could be measured from a stale
  `micros()` base).

## Build

Arduino IDE, any AVR board, no libraries (bare `attachInterrupt` + `micros()`).
