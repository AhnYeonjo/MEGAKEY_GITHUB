// ============================================================================
//  shift_register.h
//  74HC595 × 2 cascaded shift register driver (16-bit output)
//  Software SPI approach (separate SPI hardware not used)
// ============================================================================
#ifndef SHIFT_REGISTER_H
#define SHIFT_REGISTER_H

#include <Arduino.h>
#include "pins_megakey.h"

class ShiftRegister16 {
public:
  void begin() {
    pinMode(SR_DATA_PIN, OUTPUT);
    pinMode(SR_CLOCK_PIN, OUTPUT);
    pinMode(SR_LATCH_PIN, OUTPUT);
    
    digitalWrite(SR_DATA_PIN, LOW);
    digitalWrite(SR_CLOCK_PIN, LOW);
    digitalWrite(SR_LATCH_PIN, LOW);
    
    _state = 0x0000;
    _dirty = true;
    flush();
  }
  
  // Set a specific bit (not applied immediately, requires flush())
  inline void setBit(uint8_t bit_idx, bool value) {
    uint16_t mask = (uint16_t)1 << bit_idx;
    uint16_t new_state = value ? (_state | mask) : (_state & ~mask);
    if (new_state != _state) {
      _state = new_state;
      _dirty = true;
    }
  }
  
  // Toggle a bit
  inline void toggleBit(uint8_t bit_idx) {
    _state ^= ((uint16_t)1 << bit_idx);
    _dirty = true;
  }
  
  // Send to hardware only when changed
  void flush() {
    if (!_dirty) return;
    
    // Fast bit-banging via direct VPORT access (does not rely on DxCore macros)
    // Transmits 16 bits MSB-first (SR2's Q1 = MUX2_OE goes first, SR1's Q0 = KVM1_LED goes last)
    // → ShiftOut MSBFIRST, sending data from bit 15 down to 0
    
    uint16_t data = _state;
    for (int8_t i = 15; i >= 0; i--) {
      // Output bit on DATA pin
      digitalWriteFast(SR_DATA_PIN, (data >> i) & 0x01);
      // CLOCK pulse (data shifts on rising edge)
      digitalWriteFast(SR_CLOCK_PIN, HIGH);
      digitalWriteFast(SR_CLOCK_PIN, LOW);
    }
    
    // LATCH pulse (output latches on rising edge)
    digitalWriteFast(SR_LATCH_PIN, HIGH);
    digitalWriteFast(SR_LATCH_PIN, LOW);
    
    _dirty = false;
  }
  
  // Read current state (for debugging)
  inline uint16_t getState() const { return _state; }
  
  // Force refresh (e.g. for glitch recovery)
  void forceRefresh() {
    _dirty = true;
    flush();
  }
  
private:
  uint16_t _state;
  bool _dirty;
};

#endif // SHIFT_REGISTER_H
