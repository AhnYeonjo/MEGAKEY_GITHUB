// ============================================================================
//  jvs_avrdb.h
//  AVR128DB64 (AVR DB-series) 용 JVSIO 클라이언트 어댑터
//  원본: MegaClient (ATmega2560 Timer2 OC2B 사용)
//  변경: TCB1 WO 출력 (PA3)으로 JVS Sense 4MHz 토글 생성
// ============================================================================
#ifndef JVS_AVRDB_H
#define JVS_AVRDB_H

#include <Arduino.h>
#include "jvsio/JVSIO.h"
#include "pins_megakey.h"

// ============================================================================
//  JVS Data Client - UART0 (PA0/PA1)을 통한 JVS 통신
//  원본 MegaClient와 동작 동일
// ============================================================================
class AVRDBJVSDataClient : public JVSIO::DataClient {
  int available() override { return Serial.available(); }
  
  void setMode(int mode) override {
    if (mode == INPUT) {
      pinMode(PIN_PA0, INPUT);
      pinMode(PIN_PA1, INPUT);
      Serial.begin(115200);
    } else {
      // 출력 모드 - JVS Idle 상태 (TX=HIGH, RX=LOW)
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
//  JVS Sense Client - TCB1을 사용한 4MHz 토글 생성
//  
//  원본 (ATmega2560 Timer2 CTC mode):
//    TCCR2A = 0b00010010;  // OC2B toggle on compare match, CTC mode
//    OCR2A = 1;
//    TCCR2B = 0b00000001;  // No prescaler
//    → OC2B (D9) 토글 @ F_CPU/(2*(1+OCR2A)) = 16MHz/4 = 4MHz
//
//  신규 (AVR128DB64 TCB1 PWM8 mode):
//    F_CPU = 24MHz
//    Period = CCMPH + 1 = 6 cycles → 4MHz 주기
//    Duty = CCMPL + 1 = 3 cycles → 50% (대칭 토글)
//    출력 핀: PA3 (TCB1 WO 기본 출력)
// ============================================================================
class AVRDBJVSSenseClient : public JVSIO::SenseClient {
  void begin() override {
    // PA3을 출력으로 설정
    pinMode(JVS_SENSE_PIN, OUTPUT);
    digitalWrite(JVS_SENSE_PIN, LOW);  // 초기 상태 LOW
    
    // TCB1 설정: 8-bit PWM mode, ~4MHz 토글
    // CCMPL = compare value (low byte), CCMPH = period (high byte)
    TCB1.CTRLA = 0;                              // 일단 비활성화
    TCB1.CCMP = (5 << 8) | 2;                    // PER=5, CMP=2 → 50% duty 4MHz
    TCB1.CTRLB = TCB_CNTMODE_PWM8_gc;            // 8-bit PWM mode (CCMPEN은 set()에서 제어)
    TCB1.INTCTRL = 0;                            // 인터럽트 없음
    TCB1.CTRLA = TCB_CLKSEL_DIV1_gc | TCB_ENABLE_bm;  // F_CPU/1, 활성화
    
    // 초기는 READY=false (= sense 토글 활성) 상태로 두는 게 원본과 동일
    // 단, begin() 직후 set()이 호출되므로 여기서는 안전하게 OFF
    TCB1.CTRLB &= ~TCB_CCMPEN_bm;
  }
  
  void set(bool ready) override {
    if (ready) {
      // ready=true: TCB1 출력 비활성화 → 핀이 LOW로 유지됨 (downstream 준비됨)
      TCB1.CTRLB &= ~TCB_CCMPEN_bm;
      // 출력 강제 LOW (PA3)
      VPORTA.OUT &= ~(1 << 3);
    } else {
      // ready=false: TCB1 PWM 출력 활성화 → PA3에 4MHz 토글 (downstream 미준비/없음)
      TCB1.CTRLB |= TCB_CCMPEN_bm;
    }
  }
};

// ============================================================================
//  JVS LED Client - 74HC595의 STATUS_LED 비트를 통한 LED 제어
//  외부에서 ShiftRegister16 인스턴스를 전달받아 사용
// ============================================================================
class AVRDBJVSLedClient : public JVSIO::LedClient {
public:
  void attachSR(class ShiftRegister16* sr) { _sr = sr; }
  
  void begin() override {
    // 74HC595는 이미 메인 setup()에서 begin() 됨, 여기서 추가 작업 없음
  }
  
  void set(bool ready) override;  // 본체에서 SR 멤버 의존하므로 .ino에서 구현
  
private:
  class ShiftRegister16* _sr = nullptr;
};

#endif // JVS_AVRDB_H
