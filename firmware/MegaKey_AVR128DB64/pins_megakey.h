// ============================================================================
//  pins_megakey.h
//  MegaKey IO PCB - AVR128DB64-I/PT 핀 정의
//  DxCore Arduino 환경용
// ============================================================================
#ifndef PINS_MEGAKEY_H
#define PINS_MEGAKEY_H

// ===== JVS 통신 (USART0: PA0/PA1) =====
//   JVS_TX/RX는 Serial0 객체로 자동 사용 (Serial)
//   JVSIO::DataClient에서 Serial.begin()/Serial.end() 호출

// ===== Timer 출력 (Hardware PWM) =====
#define IR_LED_PIN         PIN_PA2    // TCB0 WO → NEC 38kHz HDMI 스위치
#define JVS_SENSE_PIN      PIN_PA3    // TCB1 WO → JVS Sense 4MHz 토글

// ===== Player 1 버튼 (PORTA + PORTB + PORTC) =====
#define P1_UP_PIN          PIN_PA4
#define P1_DOWN_PIN        PIN_PA5
#define P1_LEFT_PIN        PIN_PA6
#define P1_RIGHT_PIN       PIN_PA7
#define P1_A_PIN           PIN_PB1
#define P1_B_PIN           PIN_PB2
#define P1_X_PIN           PIN_PB3
#define P1_Y_PIN           PIN_PB4
#define P1_LB_PIN          PIN_PB5
#define P1_RB_PIN          PIN_PB6
#define P1_LT_PIN          PIN_PB7
#define P1_RT_PIN          PIN_PC0
#define P1_START_PIN       PIN_PC1
#define P1_SELECT_PIN      PIN_PC2
#define P1_HOME_PIN        PIN_PC3
#define P1_TURBO_PIN       PIN_PC4
#define P1_TOUCHPAD_PIN    PIN_PC5
#define P1_LANAL_PIN       PIN_PC6    // L3 스틱 누름 감지
#define P1_RANAL_PIN       PIN_PC7    // R3 스틱 누름 감지

// ===== Player 2 버튼 (PORTD + PORTE + PORTF) =====
#define P2_UP_PIN          PIN_PD0
#define P2_DOWN_PIN        PIN_PD1
#define P2_LEFT_PIN        PIN_PD2
#define P2_RIGHT_PIN       PIN_PD3
#define P2_A_PIN           PIN_PD4
#define P2_B_PIN           PIN_PD5
#define P2_X_PIN           PIN_PD6
#define P2_Y_PIN           PIN_PD7
#define P2_LB_PIN          PIN_PE0
#define P2_RB_PIN          PIN_PE1
#define P2_LT_PIN          PIN_PE2
#define P2_RT_PIN          PIN_PE3
#define P2_START_PIN       PIN_PE4
#define P2_SELECT_PIN      PIN_PE5
#define P2_HOME_PIN        PIN_PE6
#define P2_TURBO_PIN       PIN_PE7
#define P2_TOUCHPAD_PIN    PIN_PF1    // USART2 RX 비활성화 후 GPIO로 사용
#define P2_LANAL_PIN       PIN_PF2
#define P2_RANAL_PIN       PIN_PF3

// ===== RP2040 TX (단방향, RX는 비활성화) =====
//   USART2 (PF0): RP2040 #1 (1P) - Serial2
//   USART3 (PB0): RP2040 #2 (2P) - Serial3

// ===== 74HC595 시프트 레지스터 SPI (PORTF) =====
#define SR_DATA_PIN        PIN_PF4    // 74HC595 SER (pin 14)
#define SR_CLOCK_PIN       PIN_PF5    // 74HC595 SRCLK (pin 11)
#define SR_LATCH_PIN       PIN_PF6    // 74HC595 RCLK (pin 12)  ⚠ RESET fuse 비활성화 필수

// 74HC595 16비트 출력 비트 할당
#define SR_BIT_KVM1_LED     0
#define SR_BIT_KVM2_LED     1
#define SR_BIT_KVM3_LED     2
#define SR_BIT_KVM4_LED     3
#define SR_BIT_KVM5_LED     4
#define SR_BIT_STATUS_LED   5
#define SR_BIT_MUX1_SEL     6
#define SR_BIT_MUX1_OE      7
#define SR_BIT_MUX2_SEL     8
#define SR_BIT_MUX2_OE      9
// bit 10~15: 예비 (확장용)

// ===== KVM 채널 선택 버튼 (PORTG 0~4) =====
#define CHK_KVM1_PIN       PIN_PG0
#define CHK_KVM2_PIN       PIN_PG1
#define CHK_KVM3_PIN       PIN_PG2
#define CHK_KVM4_PIN       PIN_PG3
#define CHK_KVM5_PIN       PIN_PG4

// ===== 모드 설정 스위치 (PORTG 5~7) =====
#define CHK_JOYSTICK_PIN   PIN_PG5
#define CHK_JVS_PIN        PIN_PG6
#define CHK_SWAPSTICK_PIN  PIN_PG7

#endif // PINS_MEGAKEY_H
