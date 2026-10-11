// ============================================================================
//  shift_register.h
//  74HC595 × 2 캐스케이드 시프트 레지스터 드라이버 (16비트 출력)
//  소프트웨어 SPI 방식 (별도 SPI 하드웨어 미사용)
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
  
  // 특정 비트 설정 (즉시 반영하지 않음, flush() 필요)
  inline void setBit(uint8_t bit_idx, bool value) {
    uint16_t mask = (uint16_t)1 << bit_idx;
    uint16_t new_state = value ? (_state | mask) : (_state & ~mask);
    if (new_state != _state) {
      _state = new_state;
      _dirty = true;
    }
  }
  
  // 비트 토글
  inline void toggleBit(uint8_t bit_idx) {
    _state ^= ((uint16_t)1 << bit_idx);
    _dirty = true;
  }
  
  // 변경된 경우에만 하드웨어로 전송
  void flush() {
    if (!_dirty) return;
    
    // VPORT 직접 액세스로 빠른 비트뱅잉 (DxCore 매크로 의존하지 않음)
    // 16비트를 MSB-first로 전송 (SR2의 Q1 = MUX2_OE가 가장 먼저, SR1의 Q0 = KVM1_LED가 가장 마지막)
    // → ShiftOut MSBFIRST, 데이터를 비트 15부터 0까지 전송
    
    uint16_t data = _state;
    for (int8_t i = 15; i >= 0; i--) {
      // DATA 핀에 비트 출력
      digitalWriteFast(SR_DATA_PIN, (data >> i) & 0x01);
      // CLOCK 펄스 (rising edge로 데이터 시프트)
      digitalWriteFast(SR_CLOCK_PIN, HIGH);
      digitalWriteFast(SR_CLOCK_PIN, LOW);
    }
    
    // LATCH 펄스 (rising edge로 출력 래치)
    digitalWriteFast(SR_LATCH_PIN, HIGH);
    digitalWriteFast(SR_LATCH_PIN, LOW);
    
    _dirty = false;
  }
  
  // 현재 상태 읽기 (디버그용)
  inline uint16_t getState() const { return _state; }
  
  // 강제 새로고침 (예: 글리치 복구용)
  void forceRefresh() {
    _dirty = true;
    flush();
  }
  
private:
  uint16_t _state;
  bool _dirty;
};

#endif // SHIFT_REGISTER_H
