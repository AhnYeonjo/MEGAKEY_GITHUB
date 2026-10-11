// headers/addons/serial_input.h
// GP2040-CE Serial Input Addon Header
// Version: 2.4 - SerialInputAddon이 OLED + WS2812 모두 독점 사용
//
// 주의사항:
// 1. Display Addon 비활성화 필수! (OLED I2C 충돌 방지)
// 2. PlayerLEDAddon 비활성화 필수! (WS2812 PIO 충돌 방지)

#ifndef _SERIAL_INPUT_H_
#define _SERIAL_INPUT_H_

#define RP2040_ZERO
//#define RP2040_ZERO
#include "gpaddon.h"
#include "GamepadEnums.h"
#include "storagemanager.h"
#include <hardware/uart.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <pico/time.h>
#include <string>

#ifndef PLAYER_NUMBER
    #define PLAYER_NUMBER 1 // 기본값은 1P로 설정
#endif

// ========== OLED 디스플레이 ==========
// ✅ SerialInputAddon이 OLED 독점 사용
#define USE_OLED_DISPLAY

// ========== UART 설정 ==========
#define SERIAL_UART uart1
#define SERIAL_RX_PIN 5
#define SERIAL_BAUDRATE 250000

#ifdef RP2040_ZERO
    // ✅ SerialInputAddon이 WS2812 독점 사용
    #define USE_WS2812_LED
    #define WS2812_PIN 16
    #define WS2812_IS_RGBW false
#else
    // Pico의 일반 LED
    #define STATUS_LED_PIN 25
#endif



// ========================================
// I2C 및 OLED 라이브러리 포함
// ========================================
#include "hardware/i2c.h"
#include "pico/binary_info.h"

// SSD1306 설정
#define OLED_I2C_PORT i2c0
#define OLED_I2C_SDA_PIN 0
#define OLED_I2C_SCL_PIN 1
#define OLED_I2C_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// ========================================
// SSD1306 OLED 명령어 정의
// ========================================

#define OLED_SET_CONTRAST 0x81
#define OLED_DISPLAY_ALL_ON_RESUME 0xA4
#define OLED_DISPLAY_ALL_ON 0xA5
#define OLED_NORMAL_DISPLAY 0xA6
#define OLED_INVERT_DISPLAY 0xA7
#define OLED_DISPLAY_OFF 0xAE
#define OLED_DISPLAY_ON 0xAF
#define OLED_SET_DISPLAY_OFFSET 0xD3
#define OLED_SET_COM_PINS 0xDA
#define OLED_SET_VCOM_DETECT 0xDB
#define OLED_SET_DISPLAY_CLOCK_DIV 0xD5
#define OLED_SET_PRECHARGE 0xD9
#define OLED_SET_MULTIPLEX 0xA8
#define OLED_SET_LOW_COLUMN 0x00
#define OLED_SET_HIGH_COLUMN 0x10
#define OLED_SET_START_LINE 0x40
#define OLED_MEMORY_MODE 0x20
#define OLED_COLUMN_ADDR 0x21
#define OLED_PAGE_ADDR 0x22
#define OLED_COM_SCAN_INC 0xC0
#define OLED_COM_SCAN_DEC 0xC8
#define OLED_SEG_REMAP 0xA0
#define OLED_CHARGE_PUMP 0x8D
#define OLED_EXTERNAL_VCC 0x01
#define OLED_SWITCH_CAP_VCC 0x02


// 드림캐스트 출력용 GPIO 핀 정의 (사용자 환경에 맞게 수정)
#define DREAMCAST_SENSE_PIN   12
#define DREAMCAST_MAPLE_A_PIN 10 // Maple A/B는 연속된 핀이어야 함
// Maple B 핀은 A핀에 이어지는 다음 핀 (GP11)을 자동으로 사용합니다.
#define DREAMCAST_PIO_ENABLE_PIN 21
// ★★★ 통신 타임아웃 정의 (100ms) ★★★
#define DREAMCAST_RX_TIMEOUT_US 100000

// ========== 디버그 모드 ==========
// #define DEBUG_MODE  // 주석 해제로 디버그 활성화

// ========== 상수 정의 ==========

// 패드 모드 정의
#define PAD_MODE_DPAD    0
#define PAD_MODE_LANALOG 1
#define PAD_MODE_RANALOG 2

// Arduino 모드 정의
#define ARDUINO_MODE_KEYBOARD 0
#define ARDUINO_MODE_XINPUT   1
#define ARDUINO_MODE_SWITCH   2
#define ARDUINO_MODE_PS3      3
#define ARDUINO_MODE_PS4      4
#define ARDUINO_MODE_PS5      5
#define ARDUINO_MODE_XBOX     6
#define ARDUINO_MODE_JVS      7
#define ARDUINO_MODE_DREAMCAST 8

// 버튼 비트 정의 (Arduino와 동일)
#define BIT_SELECT  0
#define BIT_HOME    1
#define BIT_START   2
#define BIT_UP      3
#define BIT_DOWN    4
#define BIT_RIGHT   5
#define BIT_LEFT    6
#define BIT_A       7
#define BIT_B       8
#define BIT_X       9
#define BIT_Y       10
#define BIT_LB      11
#define BIT_RB      12
#define BIT_LT      13
#define BIT_RT      14
#define BIT_TOUCH   18

// 통신 타이밍
#define CONNECTION_TIMEOUT_US 500000
#define MODE_CHANGE_DELAY_US 100000
#define MAX_BYTES_PER_FRAME 64

// 메시지 헤더/푸터
#define BINARY_HEADER 0xAA
#define BINARY_FOOTER 0x55

#if PLAYER_NUMBER == 1
    // 회로도에서 TXS0108E의 OE 핀에 연결할 RP2040의 GPIO 핀 번호
    #define LEVEL_SHIFTER_ENABLE_PIN 28
#endif

// ========== 메시지 구조체 ==========
struct __attribute__((packed)) CompactMessage {
    uint8_t header;
    uint32_t p1_buttons;
    uint32_t p2_buttons;
    uint8_t modes;
    uint8_t channel_state;
    uint8_t crc;
    uint8_t footer;
};

// ========== SerialInputAddon 클래스 ==========
class SerialInputAddon : public GPAddon {
public:
    virtual bool available();
    virtual void setup();
    virtual void preprocess();
    virtual void process();
    
    // GPAddon 필수 가상 함수들
    virtual void postprocess(bool) {}
    virtual std::string name() { return "SerialInput"; }
    virtual void reinit() { 
        addon_enabled = true;
        connection_timeout = true;
        buffer_index = 0;
        memset(&serial_state, 0, sizeof(serial_state));
    }
    
private:
    // 시리얼 상태 구조체
    struct __attribute__((packed)) SerialState {
        uint32_t p1_buttons;
        uint32_t p2_buttons;
        uint8_t input_mode;
        uint8_t modes;
        uint8_t channel;
        uint8_t channel_state;
    } serial_state;
    
    // 통신 버퍼
    uint8_t message_buffer[sizeof(CompactMessage)];
    uint8_t buffer_index;
    
    // 연결 상태
    uint32_t last_receive_time;
    bool connection_timeout;
    bool addon_enabled;
    bool was_timeout;
    
    // 모드 변경 관련
    bool mode_change_pending;
    uint32_t mode_change_time;
    
    // Gamepad 상태 백업
    GamepadState previous_state;
    
    // 키보드 매핑 초기화 플래그
    bool keyboard_mapping_initialized = false;
    
    // 에러 통계
    struct {
        uint32_t crc_errors;
        uint32_t invalid_headers;
        uint32_t invalid_footers;
        uint32_t buffer_overflows;
    } error_stats;
    
    // updateStatusLED에서 사용할 상태 변수들
    uint32_t led_state_last_time;
    uint32_t led_mode_change_time;
    bool led_blink_state;
    uint8_t led_last_mode;

    // 초기화 애니메이션 상태를 관리할 변수
    enum LEDInitState {
        NOT_STARTED,
        ANIMATING,
        FINISHED
    };
    LEDInitState led_init_state;
    
    // 메서드들
    bool checkUartSafety();
    void receiveSerialData();
    void processKeyboardModeInput(Gamepad* gamepad);
    void processGamepadModeInput(Gamepad* gamepad);
    void clearGamepadState(Gamepad* gamepad);
    void mergeGamepadStates(Gamepad* target, const GamepadState* source);
    uint8_t calculateCRC8(const uint8_t* data, uint8_t len);
    InputMode convertToGP2040Mode(uint8_t arduino_mode);
    void updateStatusLED();
    void handleConnectionRecovery(Gamepad* gamepad);
  
    #ifdef USE_WS2812_LED
    void setupWS2812();
    void setLEDColor(uint8_t r, uint8_t g, uint8_t b);

    // PIO 관련 변수들 대신 NeoPico 객체를 선언합니다.
    NeoPico neoPico;
    #endif

    #ifdef USE_OLED_DISPLAY
    bool oled_available;  // OLED 사용 가능 여부
        
    // ===== OLED 안전 체크 함수 (새로 추가!) =====
    bool checkOLEDAvailable();     
    // ===== OLED 기본 함수들 =====
    bool setupOLED();
    void oled_send_cmd(uint8_t cmd);
    void oled_send_data(uint8_t* data, size_t len);
    void oled_clear();
    void oled_draw_char(uint8_t x, uint8_t y, char c, uint8_t size);
    void oled_draw_string(uint8_t x, uint8_t y, const char* str, uint8_t size);
    
    // ===== 개선된 디스플레이 함수들 =====
    const char* getModeString(uint8_t mode);
    const char* getPadModeString(uint8_t modes, bool is_player1);  // ← 새로 추가
    bool isSwapped();                                               // ← 새로 추가
    
    // ===== 4가지 디스플레이 스타일 =====
    void updateDisplay();           // 기본 - 모든 정보 표시 (추천)
    void updateDisplayCompact();    // 컴팩트 - 공간 절약
    void updateDisplayPlayer();     // 플레이어 중심 - 내 정보 강조
    void updateDisplayIcon();       // 아이콘 - 간결하고 심플
    #endif
    
    // ★★★ 드림캐스트 관련 멤버 변수 및 함수 선언 (정리 및 추가) ★★★
    
    // PIO 하드웨어 리소스
    PIO dreamcast_pio;
    uint dreamcast_sm;
    uint dreamcast_offset;
    uint32_t last_dreamcast_rx_time;
    bool dreamcast_pio_active;

    // Maple Bus 통신 상태 관리
    enum MapleBusState { LISTENING, RESPONDING };
    MapleBusState maple_state;

    // PIO 초기화 및 제어 함수
    void setupDreamcastPIO();
    void startMapleListen(); // PIO를 수신 모드로 시작하는 함수

    // 통신 처리 함수
    void processDreamcastOutput(const GamepadState& state);
    void sendDreamcastReport(const uint8_t* report_data, size_t len);

    #ifdef DEBUG_MODE
    void debugPrint(const char* format, ...);
    void printErrorStats();
    void printKeyboardMapping();
    #endif
};

#endif