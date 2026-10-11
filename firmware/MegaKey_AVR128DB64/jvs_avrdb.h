// ============================================================================
//  jvs_avrdb.h
//  JVSIO client adapter for AVR128DB64 (AVR DB-series)
//  Original: MegaClient (used ATmega2560 Timer2 OC2B)
//  Change: generates JVS Sense 4MHz toggle via TCB1 WO output (PA3)
// ============================================================================
#ifndef JVS_AVRDB_H
#define JVS_AVRDB_H

#include <Arduino.h>
#include "jvsio/JVSIO.h"
#include "pins_megakey.h"

// ============================================================================
//  JVS Data Client - JVS communication over UART0 (PA0/PA1)
//  Behaves identically to the original MegaClient
// ============================================================================
class AVRDBJVSDataClient : public JVSIO::DataClient {
  int available() override { return Serial.available(); }
  
  void setMode(int mode) override {
    if (mode == INPUT) {
      pinMode(PIN_PA0, INPUT);
      pinMode(PIN_PA1, INPUT);
      Serial.begin(115200);
    } else {
      // Output mode - JVS Idle state (TX=HIGH, RX=LOW)
      digitalWrite(PIN_PA0, HIGH);
      digitalWrite(PIN_PA1, LOW);
      pinMode(PIN_PA0, OUTPUT);
      pinMode(PIN_PA1, OUTPUT);
      Serial.end();
    }
  }
  
  void startTransaction() override { noInterrupts(); }
  void endTransaction() override { interrupts(); }
  uint8_t read() override { return Serial.read(); }
  void write(uint8_t data) override {
    if (Serial) Serial.write(data);
  }
};

// ============================================================================
//  JVS Sense Client - 4MHz toggle generation using TCB1
//  
//  Original (ATmega2560 Timer2 CTC mode):
//    TCCR2A = 0b00010010;  // OC2B toggle on compare match, CTC mode
//    OCR2A = 1;
//    TCCR2B = 0b00000001;  // No prescaler
//    → OC2B (D9) toggle @ F_CPU/(2*(1+OCR2A)) = 16MHz/4 = 4MHz
//
//  New (AVR128DB64 TCB1 PWM8 mode):
//    F_CPU = 24MHz
//    Period = CCMPH + 1 = 6 cycles → 4MHz period
//    Duty = CCMPL + 1 = 3 cycles → 50% (symmetric toggle)
//    Output pin: PA3 (TCB1 WO default output)
// ============================================================================
class AVRDBJVSSenseClient : public JVSIO::SenseClient {
  void begin() override {
    // Configure PA3 as output
    pinMode(JVS_SENSE_PIN, OUTPUT);
    digitalWrite(JVS_SENSE_PIN, LOW);  // initial state LOW
    
    // TCB1 config: 8-bit PWM mode, ~4MHz toggle
    // CCMPL = compare value (low byte), CCMPH = period (high byte)
    TCB1.CTRLA = 0;                              // disable for now
    TCB1.CCMP = (5 << 8) | 2;                    // PER=5, CMP=2 → 50% duty 4MHz
    TCB1.CTRLB = TCB_CNTMODE_PWM8_gc;            // 8-bit PWM mode (CCMPEN controlled in set())
    TCB1.INTCTRL = 0;                            // no interrupts
    TCB1.CTRLA = TCB_CLKSEL_DIV1_gc | TCB_ENABLE_bm;  // F_CPU/1, enable
    
    // Initially keep READY=false (= sense toggle active), same as original
    // However, set() is called right after begin(), so leave OFF safely here
    TCB1.CTRLB &= ~TCB_CCMPEN_bm;
  }
  
  void set(bool ready) override {
    if (ready) {
      // ready=true: disable TCB1 output → pin held LOW (downstream ready)
      TCB1.CTRLB &= ~TCB_CCMPEN_bm;
      // force output LOW (PA3)
      VPORTA.OUT &= ~(1 << 3);
    } else {
      // ready=false: enable TCB1 PWM output → 4MHz toggle on PA3 (downstream not ready/absent)
      TCB1.CTRLB |= TCB_CCMPEN_bm;
    }
  }
};

// ============================================================================
//  JVS LED Client - LED control via the STATUS_LED bit of the 74HC595
//  Receives a ShiftRegister16 instance from outside and uses it
// ============================================================================
class AVRDBJVSLedClient : public JVSIO::LedClient {
public:
  void attachSR(class ShiftRegister16* sr) { _sr = sr; }
  
  void begin() override {
    // 74HC595 is already begin()'d in the main setup(); nothing extra here
  }
  
  void set(bool ready) override;  // body depends on SR member, so implemented in .ino
  
private:
  class ShiftRegister16* _sr = nullptr;
};

#endif // JVS_AVRDB_H
