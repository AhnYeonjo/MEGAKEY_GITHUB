// src/addons/serial_input.cpp
// GP2040-CE Serial Input Addon Implementation
// Version: 2.3 - Display Addon 비활성화, OLED 독점 사용
//
// 주의사항:
// 1. GP2040-CE의 Display Addon을 반드시 비활성화해야 함 (I2C 충돌 방지)
// 2. PlayerLEDAddon은 활성화 유지 (WS2812 관리)
// 3. SerialInputAddon이 OLED를 독점 사용하여 시리얼 상태 표시

#include "addons/serial_input.h"
#include "maple_bus.pio.h" 
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <pico/time.h>
#include <hardware/watchdog.h>
#include <hardware/clocks.h>
#include "hardware/pio_instructions.h"

#ifdef USE_WS2812_LED
//#include "ws2812.pio.h"

static const uint16_t blink_program_instructions[] = {
    //     .wrap_target
    0x80a0, //  0: pull   block
    0x6001, //  1: out    pins, 1
    0x0041, //  2: jmp    x--, 1
    //     .wrap
};

static const struct pio_program blink_program = {
    .instructions = blink_program_instructions,
    .length = 3,
    .origin = -1,
};

#endif

// ========================================
// 5x7 폰트 데이터
// ========================================
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 공백 (0)
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0 (1)
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1 (2)
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2 (3)
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3 (4)
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4 (5)
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5 (6)
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6 (7)
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7 (8)
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8 (9)
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9 (10)
    {0x7C, 0x12, 0x11, 0x12, 0x7C}, // A (11)
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B (12)
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C (13)
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D (14)
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E (15)
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F (16)
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G (17)
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H (18)
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I (19)
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J (20)
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K (21)
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L (22)
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M (23)
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N (24)
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O (25)
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P (26)
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q (27)
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R (28)
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S (29)
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T (30)
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U (31)
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V (32)
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W (33)
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X (34)
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y (35)
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z (36)
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [ (37)
    {0x55, 0x2A, 0x55, 0x2A, 0x55}, // backslash (38)
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ] (39)
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^ (40)
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _ (41)
    {0x00, 0x01, 0x02, 0x04, 0x00}, // ` (42)
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a (43)
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b (44)
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c (45)
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d (46)
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e (47)
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f (48)
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g (49)
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h (50)
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i (51)
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j (52)
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k (53)
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l (54)
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m (55)
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n (56)
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o (57)
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p (58)
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q (59)
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r (60)
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s (61)
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t (62)
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u (63)
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v (64)
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w (65)
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x (66)
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y (67)
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z (68)
    {0x00, 0x36, 0x36, 0x00, 0x00}, // : (69)
};

// ========================================
// ★★★ 드림캐스트 통신 로직 (완전 개편) ★★★
// ========================================

// 드림캐스트 컨트롤러의 기능 코드 (Function Code)
#define DREAMCAST_FUNC_CONTROLLER 0x01000000

// Maple Bus 프레임의 마지막 워드임을 나타내는 플래그
#define MAPLE_FRAME_LAST_WORD 0x80

// 드림캐스트 본체가 보내는 주요 명령어
#define MAPLE_COMMAND_DEVINFO   0x01 // 장치 정보 요청
#define MAPLE_COMMAND_GETCOND   0x09 // 컨트롤러 상태 요청

// Maple Bus의 CRC는 모든 바이트를 XOR 하는 간단한 방식입니다.
// Maple Bus CRC 계산 헬퍼 함수
uint8_t calculateMapleCrc(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
    }
    return crc;
}

// PIO 초기화 함수 (setup()에서 한 번만 호출)
void SerialInputAddon::setupDreamcastPIO() {
    // 1. 시스템에서 사용 가능한 PIO 인스턴스와 상태 머신(SM)을 안전하게 찾습니다.
    PIO pio = pio0;
    int sm = pio_claim_unused_sm(pio, false);
    if (sm == -1) {
        pio = pio1;
        sm = pio_claim_unused_sm(pio, true);
    }

    #ifdef DEBUG_MODE
    debugPrint("Dreamcast PIO assigned to pio%d, sm%d", (pio == pio0) ? 0 : 1, sm);
    #endif

    // 2. PIO 프로그램을 메모리에 로드합니다.
    uint offset = pio_add_program(pio, &maple_bus_program);

    // 3. 찾은 리소스로 PIO를 초기화합니다.
    dreamcast_pio = pio;
    dreamcast_sm = sm;
    dreamcast_offset = offset;
    
    // 4. .pio.h에 생성된 헬퍼 함수를 호출하여 PIO SM의 기본 설정을 구성합니다.
    //    이 함수는 클럭 분주율, FIFO 설정, 시프트 방향 등을 설정합니다.
    maple_bus_program_init(dreamcast_pio, dreamcast_sm, dreamcast_offset, DREAMCAST_MAPLE_A_PIN, DREAMCAST_PIO_ENABLE_PIN);
}

// PIO를 리스닝 모드로 설정하고 시작하는 함수
void SerialInputAddon::startMapleListen() {
    pio_sm_set_enabled(dreamcast_pio, dreamcast_sm, false);

    // 2. 핀 방향을 입력(Input)으로 설정합니다.
    //    이것이 CPU가 PIO의 동작 모드(RX/TX)를 제어하는 핵심입니다.
    pio_sm_set_consecutive_pindirs(dreamcast_pio, dreamcast_sm, DREAMCAST_MAPLE_A_PIN, 2, false); // false = Input
    
    // 3. Pico SDK의 pio_gpio_init()을 호출하여 해당 핀들이 PIO에 의해 제어되도록 설정합니다.
    pio_gpio_init(dreamcast_pio, DREAMCAST_MAPLE_A_PIN);
    pio_gpio_init(dreamcast_pio, DREAMCAST_MAPLE_A_PIN + 1);

    // 4. PIO 프로그램 카운터(PC)를 RX 섹션의 시작점으로 강제 이동시킵니다.
    const uint offset_rx = 7; // Manually set offset for rx_entry (SDK < 1.5.0 compatible)
    pio_sm_exec(dreamcast_pio, dreamcast_sm, pio_encode_jmp(dreamcast_offset + offset_rx));
    
    // 5. FIFO를 비워 이전에 남아있을 수 있는 데이터를 제거합니다.
    pio_sm_clear_fifos(dreamcast_pio, dreamcast_sm);

    // 6. 모든 설정이 완료되었으므로, 상태 머신을 활성화하여 리스닝을 시작합니다.
    pio_sm_set_enabled(dreamcast_pio, dreamcast_sm, true);

    maple_state = LISTENING;
}


// ★★★ sendDreamcastReport 함수 (완전 재작성) ★★★
// 이 함수는 CPU가 준비한 응답 프레임을 PIO를 통해 전송합니다.
void SerialInputAddon::sendDreamcastReport(const uint8_t* report_data, size_t len) {
    pio_sm_set_enabled(dreamcast_pio, dreamcast_sm, false);
    pio_sm_set_consecutive_pindirs(dreamcast_pio, dreamcast_sm, DREAMCAST_MAPLE_A_PIN, 2, true);
    pio_gpio_init(dreamcast_pio, DREAMCAST_MAPLE_A_PIN);
    pio_gpio_init(dreamcast_pio, DREAMCAST_MAPLE_A_PIN + 1);
    
    const uint offset_tx = 14; // Manually set offset for tx_entry (SDK < 1.5.0 compatible)
    pio_sm_exec(dreamcast_pio, dreamcast_sm, pio_encode_jmp(dreamcast_offset + offset_tx));

    pio_sm_clear_fifos(dreamcast_pio, dreamcast_sm);
    pio_sm_set_enabled(dreamcast_pio, dreamcast_sm, true);
    maple_state = RESPONDING;

    for (size_t i = 0; i < len; i += 4) {
        uint32_t word = ((uint32_t*)report_data)[i / 4];
        pio_sm_put_blocking(dreamcast_pio, dreamcast_sm, word);
    }

    // ★★★ sleep_us() 대신 TX FIFO가 비워질 때까지 대기 ★★★
    while (!pio_sm_is_tx_fifo_empty(dreamcast_pio, dreamcast_sm)) {
        tight_loop_contents();
    }
    
    pio_sm_set_enabled(dreamcast_pio, dreamcast_sm, false);
    startMapleListen(); 
}

// ★★★ 드림캐스트 출력/처리 메인 함수 (완전 재작성) ★★★
// src/addons/serial_input.cpp

// ★★★ 최종 완성된 processDreamcastOutput 함수 ★★★
void SerialInputAddon::processDreamcastOutput(const GamepadState& state) {
    // --- Step 1: 현재 상태 확인 ---
    // PIO가 데이터를 수신할 준비가 된 'LISTENING' 상태가 아니면 통신 충돌 방지를 위해 즉시 종료합니다.
    if (maple_state != LISTENING) {
        return;
    }

    // --- Step 2: 수신된 데이터 확인 및 처리 ---
    // PIO의 RX FIFO(수신 버퍼)에 데이터가 있는지 확인합니다.
    if (pio_sm_get_rx_fifo_level(dreamcast_pio, dreamcast_sm) > 0) {
        
        // ★★★ 버그 수정: 데이터 수신 감지 시, 즉시 타임아웃 타이머를 갱신합니다. ★★★
        // 이렇게 해야 정상 통신 중에 PIO가 비활성화되는 것을 막을 수 있습니다.
        last_dreamcast_rx_time = time_us_32();

        // [안정성 로직] FIFO에 쌓인 모든 요청을 읽어내어 가장 최신 요청 하나만 남깁니다.
        // 이는 시스템 지연 발생 시, 이미 시간 초과된 오래된 요청들을 무시하고
        // 가장 최근 요청에 응답하여 통신을 복구하기 위함입니다.
        uint32_t command_word;
        while (pio_sm_get_rx_fifo_level(dreamcast_pio, dreamcast_sm) > 0) {
            command_word = pio_sm_get(dreamcast_pio, dreamcast_sm);
        }

        // --- Step 3: 명령어 분석 및 응답 생성 ---
        uint8_t command = (command_word >> 8) & 0xFF;
        uint8_t response_packet[32];
        size_t response_len = 0;

        switch (command) {
            case MAPLE_COMMAND_DEVINFO: {
                // "나는 누구인가"에 대한 응답: "나는 표준 Dreamcast 컨트롤러입니다."
                static const uint8_t dev_info_response[] = {
                    // --- Maple Bus 프레임 헤더 ---
                    0x05, 0xFE, 0x00, 0x05, 
                    // --- 데이터 페이로드 (Little Endian으로 분해) ---
                    0x00, 0x00, 0x00, 0x01, // 0x01000000 (DREAMCAST_FUNC_CONTROLLER)
                    0x00, 0x00, 0x00, 0x00, 
                    0x00, 0x00, 0x00, 0x00,
                    0xF6, 0x01, 0x00, 0x00, 
                    0xAE, 0x01, 0x00, 0x00, 
                    0x2C, 0x01, 0xF4, 0x01
                };
                
                memcpy(response_packet, dev_info_response, sizeof(dev_info_response));
                response_len = sizeof(dev_info_response);
                break;
            }

            case MAPLE_COMMAND_GETCOND: {
                // "버튼 상태는 어떤가"에 대한 응답
                response_len = 16; // 4 words (헤더+기능코드+데이터)
                memset(response_packet, 0, response_len);

                // 헤더
                response_packet[0] = 0x08; // 응답 코드: 데이터 전송
                response_packet[1] = 0xFE; // 수신 주소: To Host
                response_packet[2] = 0x00; // 발신 주소: Port A
                response_packet[3] = 0x02; // 데이터 길이: 2 words (8 bytes)

                // 페이로드
                ((uint32_t*)response_packet)[1] = DREAMCAST_FUNC_CONTROLLER;
                uint8_t* controller_data = &response_packet[8];

                // 버튼 상태 (Active Low: 눌리면 0)
                uint16_t buttons = 0xFFFF;
                if (state.buttons & GAMEPAD_MASK_B3) buttons &= ~(1 << 0); // C 버튼 (GP2040 B3/X)
                if (state.buttons & GAMEPAD_MASK_B4) buttons &= ~(1 << 1); // B 버튼 (GP2040 B4/Y)
                if (state.buttons & GAMEPAD_MASK_B1) buttons &= ~(1 << 2); // A 버튼 (GP2040 B1/A)
                if (state.buttons & GAMEPAD_MASK_B2) buttons &= ~(1 << 3); // X 버튼 (GP2040 B2/B)
                if (state.dpad & GAMEPAD_MASK_UP)    buttons &= ~(1 << 4);
                if (state.dpad & GAMEPAD_MASK_DOWN)  buttons &= ~(1 << 5);
                if (state.dpad & GAMEPAD_MASK_LEFT)  buttons &= ~(1 << 6);
                if (state.dpad & GAMEPAD_MASK_RIGHT) buttons &= ~(1 << 7);
                if (state.buttons & GAMEPAD_MASK_S2) buttons &= ~(1 << 9); // Start 버튼

                // 데이터 버퍼에 값 채우기
                controller_data[0] = buttons & 0xFF;
                controller_data[1] = (buttons >> 8) & 0xFF;
                controller_data[2] = state.rt;
                controller_data[3] = state.lt;
                controller_data[4] = state.lx >> 8;
                controller_data[5] = state.ly >> 8;
                controller_data[6] = 0x80; // 2nd Stick X (중립)
                controller_data[7] = 0x80; // 2nd Stick Y (중립)
                break;
            }
            default: // 모르는 명령어는 무시
                break;
        }

        // --- Step 4: 응답 전송 ---
        if (response_len > 0) {
            response_packet[0] |= MAPLE_FRAME_LAST_WORD; // 마지막 프레임 플래그 설정
            response_packet[response_len - 1] = calculateMapleCrc(response_packet, response_len - 1); // CRC 계산 및 추가
            sendDreamcastReport(response_packet, response_len); // PIO로 전송
        }
    }
}

// ========================================
// Addon 사용 가능 여부 체크
// ========================================
bool SerialInputAddon::available() {    
    return true;
}

// ========================================
// UART 안전성 체크
// ========================================
bool SerialInputAddon::checkUartSafety() {
    const GamepadOptions& options = Storage::getInstance().getGamepadOptions();
    return true;
}

// ========================================
// WS2812 LED 초기화 (RP2040 Zero) - 안전한 버전
// ========================================
#ifdef USE_WS2812_LED

void SerialInputAddon::setupWS2812() {
    // 1. USB 호스트가 사용하는 pio0을 피해 **pio1을 명시적으로 선택**합니다.
    //    이것이 이 문제의 가장 핵심적인 해결책입니다.
    PIO pio = pio1;

    // 2. pio1에서 사용 가능한 State Machine(SM)을 찾습니다.
    //    'true' 옵션은 만약 사용 가능한 SM이 없으면 시스템을 멈추게 합니다.
    //    이것은 다른 Addon이 pio1까지 모두 사용하고 있는 극히 드문 경우를
    //    진단하기 위해 유용합니다.
    int sm = pio_claim_unused_sm(pio, true);

    // 3. NeoPico에게 **정확한 리소스(pio1, 그리고 방금 찾은 sm)**를 알려주며 초기화합니다.
    //    - WS2812_PIN: LED 데이터 핀 번호 (16)
    //    - 1           : 제어할 LED 개수 (안전하게 1개로 시작)
    //    - LED_FORMAT_GRB: 표준 WS2812 포맷
    //    - pio         : 우리가 지정한 pio1
    //    - sm          : pio1에서 찾은 비어있는 State Machine
    neoPico.Setup(WS2812_PIN, 1, LED_FORMAT_GRB, pio, sm);

    // 4. 초기화 후 모든 LED를 끕니다.
    neoPico.Off();
}

void SerialInputAddon::setLEDColor(uint8_t r, uint8_t g, uint8_t b) {
    // 1. r, g, b 값을 하나의 32비트 RGB 색상 값으로 합칩니다.
    //    NeoPico 클래스는 이 포맷을 입력으로 받습니다.
    uint32_t pixel_color = (r << 8) | (g << 16) | b;

    // 2. NeoPico의 PutPixel 함수를 호출하여 LED 색상을 설정합니다.
    //    - 이 함수는 내부적으로 RGB를 GRB로 변환하고,
    //      설정된 PIO를 통해 LED에 데이터를 전송합니다.
    //    - PutPixel은 '블로킹' 함수이므로, 데이터 전송이 끝날 때까지 기다립니다.
    //      (LED가 1개이므로 거의 즉시 완료됩니다.)
    neoPico.PutPixel(pixel_color);
}
#endif

// ========================================
// 초기화
// ========================================
void SerialInputAddon::setup() {
    #ifdef DEBUG_MODE
    sleep_ms(100);
    debugPrint("SerialInputAddon setup started (Player %d)", PLAYER_NUMBER);
    #endif

    // ===== Step 1: UART 초기화 (빠름, 블로킹 없음) =====
    uart_init(SERIAL_UART, SERIAL_BAUDRATE);
    gpio_set_function(SERIAL_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(SERIAL_UART, false, false);
    uart_set_format(SERIAL_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(SERIAL_UART, true);
    
    // ===== Step 2: WS2812 LED 초기화 (안전하게) =====
    // LED 상태 변수 초기화
    led_state_last_time = 0;
    led_mode_change_time = 0;
    led_blink_state = false;
    led_init_state = NOT_STARTED; // 초기화 애니메이션은 아직 시작 안 함
    led_last_mode = 0xFF; // 초기화 추가

    #ifdef USE_WS2812_LED
    setupWS2812();
    
    #endif
    
    // ===== Step 3: OLED 초기화 (빠르고 안전하게) =====
    #ifdef USE_OLED_DISPLAY
    #ifdef DEBUG_MODE
    debugPrint("Checking OLED...");
    #endif
    
    // ✅ 단 한 번만 시도, 실패 시 즉시 계속 진행
    oled_available = checkOLEDAvailable();
    
    if (oled_available) {
        #ifdef DEBUG_MODE
        debugPrint("OLED detected - initializing");
        #endif
        
        if (setupOLED()) {
            oled_clear();
            
            // ✅ 간단한 초기화 메시지
            char msg[32];
            sprintf(msg, "P%d SERIAL", PLAYER_NUMBER);
            oled_draw_string(10, 10, msg, 2);
            oled_draw_string(25, 35, "READY", 2);
            sleep_ms(300);
        } else {
            oled_available = false;
        }
    } else {
        #ifdef DEBUG_MODE
        debugPrint("No OLED detected - continuing without display");
        #endif
    }
    #endif  
    
    // ===== Step 4: 상태 초기화 (빠름) =====
    memset(&serial_state, 0, sizeof(serial_state));
    memset(&message_buffer, 0, sizeof(message_buffer));
    previous_state = GamepadState();
    memset(&error_stats, 0, sizeof(error_stats));
    
    buffer_index = 0;
    last_receive_time = 0;
    connection_timeout = true;
    was_timeout = true;
    mode_change_pending = false;
    mode_change_time = 0;
    keyboard_mapping_initialized = false;
    
    // ===== Step 5: Addon 활성화 =====
    addon_enabled = true;
    
    #ifdef DEBUG_MODE
    debugPrint("Initializing Dreamcast Output support...");
    #endif
    gpio_init(DREAMCAST_PIO_ENABLE_PIN);
    gpio_set_dir(DREAMCAST_PIO_ENABLE_PIN, GPIO_OUT);
    gpio_put(DREAMCAST_PIO_ENABLE_PIN, 1); // 기본 상태를 '대기' (HIGH)로 설정

    gpio_init(DREAMCAST_SENSE_PIN);
    gpio_set_dir(DREAMCAST_SENSE_PIN, GPIO_OUT);
    gpio_put(DREAMCAST_SENSE_PIN, 0);
    
    setupDreamcastPIO();
    startMapleListen(); 

    maple_state = LISTENING;
    last_dreamcast_rx_time = time_us_32(); // 타임아웃 타이머 초기화
    dreamcast_pio_active = true;           // PIO 활성 상태로 시작

    // ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
    // ★★★ 2. 레벨 시프터 활성화 로직 추가 (P1 전용, 모든 초기화 완료 후) ★★★
    // ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
    #if PLAYER_NUMBER == 1
        // 제어 핀을 초기화하고 출력으로 설정합니다.
        gpio_init(LEVEL_SHIFTER_ENABLE_PIN);
        gpio_set_dir(LEVEL_SHIFTER_ENABLE_PIN, GPIO_OUT);
        
        // OE 핀에 HIGH 신호를 보내 TXS0108E를 활성화합니다.
        gpio_put(LEVEL_SHIFTER_ENABLE_PIN, 1);
    #endif

    #ifdef DEBUG_MODE
    debugPrint("Setup complete - UART%d on GP%d at %d baud", 
               (SERIAL_UART == uart0) ? 0 : 1, SERIAL_RX_PIN, SERIAL_BAUDRATE);
    debugPrint("WS2812 status: %s", "ENABLED");
    debugPrint("OLED status: %s", oled_available ? "ENABLED" : "DISABLED");
    #endif
    
    // ✅ 전체 setup() 시간: 약 600ms 이내
}

// ========================================
// 디버그 출력 (USB CDC)
// ========================================
#ifdef DEBUG_MODE
void SerialInputAddon::debugPrint(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    printf("[P%d][%lu] %s\n", PLAYER_NUMBER, time_us_32() / 1000, buffer);
}

void SerialInputAddon::printErrorStats() {
    debugPrint("Errors - CRC:%lu Header:%lu Footer:%lu Overflow:%lu",
               error_stats.crc_errors,
               error_stats.invalid_headers,
               error_stats.invalid_footers,
               error_stats.buffer_overflows);
}

void SerialInputAddon::printKeyboardMapping() {
    const KeyboardMapping& kbMap = Storage::getInstance().getKeyboardMapping();
    
    debugPrint("Keyboard Mapping:");
    debugPrint("  Directions: Up=0x%02X Down=0x%02X Left=0x%02X Right=0x%02X",
               kbMap.keyDpadUp, kbMap.keyDpadDown, 
               kbMap.keyDpadLeft, kbMap.keyDpadRight);
    debugPrint("  Actions: B1=0x%02X B2=0x%02X B3=0x%02X B4=0x%02X",
               kbMap.keyButtonB1, kbMap.keyButtonB2,
               kbMap.keyButtonB3, kbMap.keyButtonB4);
    debugPrint("  Shoulders: L1=0x%02X R1=0x%02X L2=0x%02X R2=0x%02X",
               kbMap.keyButtonL1, kbMap.keyButtonR1,
               kbMap.keyButtonL2, kbMap.keyButtonR2);
    debugPrint("  System: S1=0x%02X S2=0x%02X A1=0x%02X",
               kbMap.keyButtonS1, kbMap.keyButtonS2, kbMap.keyButtonA1);
}
#endif

// ========================================
// 전처리 (데이터 수신)
// ========================================
void SerialInputAddon::preprocess() {
    if (!addon_enabled) return;
    
    // 시리얼 데이터 수신
    receiveSerialData();
    
    // 연결 타임아웃 체크
    if (time_us_32() - last_receive_time > CONNECTION_TIMEOUT_US) {
        if (connection_timeout == false) {
            // '연결됨' -> '끊김' 상태로 전환되는 바로 그 순간
            #ifdef DEBUG_MODE
            debugPrint("Connection timeout detected");
            #endif
            memset(&serial_state, 0, sizeof(serial_state));
        }
        connection_timeout = true; // 타임아웃 조건 만족 시, 항상 true로 설정
    } else {
        connection_timeout = false; // 타임아웃 조건 만족 안하면, 항상 false로 설정
    }
    
    // LED 상태 업데이트
    updateStatusLED();
#ifdef USE_OLED_DISPLAY
    #ifdef OLED_STYLE_FULL
        updateDisplay();
    #elif defined(OLED_STYLE_COMPACT)
        updateDisplayCompact();
    #elif defined(OLED_STYLE_PLAYER)
        updateDisplayPlayer();
    #elif defined(OLED_STYLE_ICON)
        updateDisplayIcon();
    #else
        updateDisplay();
    #endif
#endif
}

// ========================================
// 메인 처리
// ========================================
void SerialInputAddon::process() {
    if (!addon_enabled) return;
    
    Gamepad* gamepad = Storage::getInstance().GetGamepad();
    GamepadOptions& options = Storage::getInstance().getGamepadOptions();
    
    // ===== KeyboardMapping 지연 초기화 (1회만) =====
    static bool keyboard_init_attempted = false;
    if (!keyboard_init_attempted && addon_enabled) {
        keyboard_init_attempted = true;
        
        // 부팅 후 시간 여유 두기 (500ms)
        static uint32_t init_time = 0;
        if (init_time == 0) {
            init_time = to_ms_since_boot(get_absolute_time());
        }
        
        if ((to_ms_since_boot(get_absolute_time()) - init_time) > 500) {
            #ifdef DEBUG_MODE
            debugPrint("Delayed KeyboardMapping initialization");
            #endif
        }
    }

    // ===== Step 1: 연결 복구 처리 (초기화 제거!) =====
    handleConnectionRecovery(gamepad);
    
    // ===== Step 2: 연결 상태 체크 =====
    if (connection_timeout) {
        // ✅ UART 없으면 GPIO만 사용 (이미 메인 루프에서 처리됨)
        return;
    }
    
    // ★★★★★ Dreamcast PIO 타임아웃 관리 (새로운 로직) ★★★★★
    if (serial_state.input_mode == ARDUINO_MODE_DREAMCAST) {
        uint32_t now = time_us_32();

        // 1. 타임아웃이 발생했고, PIO가 활성 상태였다면?
        if ((now - last_dreamcast_rx_time > DREAMCAST_RX_TIMEOUT_US) && dreamcast_pio_active) {
            #ifdef DEBUG_MODE
            debugPrint("Dreamcast RX timeout. Deactivating PIO wait.");
            #endif
            gpio_put(DREAMCAST_PIO_ENABLE_PIN, 1); // PIO를 안전한 루프로 JMP시킴
            dreamcast_pio_active = false;
        }
        // 2. PIO가 비활성 상태인데, 다시 활성화해야 한다면? (예: 모드 진입 직후)
        else if (!dreamcast_pio_active) {
            #ifdef DEBUG_MODE
            debugPrint("Re-activating PIO for Dreamcast mode.");
            #endif
            gpio_put(DREAMCAST_PIO_ENABLE_PIN, 0); // PIO가 통신을 감지하도록 함
            last_dreamcast_rx_time = now; // 타임아웃 타이머 재시작
            dreamcast_pio_active = true;
        }
    } else {
        // Dreamcast 모드가 아닐 때는 항상 PIO를 비활성화 상태로 둡니다.
        if (dreamcast_pio_active) {
            gpio_put(DREAMCAST_PIO_ENABLE_PIN, 1);
            dreamcast_pio_active = false;
        }
    }

    // ★★★ 드림캐스트 모드 특별 처리 ★★★
    if (serial_state.input_mode == ARDUINO_MODE_DREAMCAST) {
        Gamepad* gamepad = Storage::getInstance().GetGamepad();
        processGamepadModeInput(gamepad); // 입력 병합
        processDreamcastOutput(gamepad->state); // 드림캐스트로 출력
        return; // USB 처리 건너뛰기
    }
    
    // ★★★ 바로 여기서 JVS 모드를 직접 확인하고 입력을 차단 ★★★
    if (serial_state.input_mode == ARDUINO_MODE_JVS) {
        // JVS 모드일 때는 USB 입력을 처리하지 않고 함수를 종료합니다.
        return; 
    }
    
    // ===== Step 3: 모드 변환 =====
    InputMode targetMode = convertToGP2040Mode(serial_state.input_mode);
    InputMode currentMode = options.inputMode;
    
    // ===== Step 4: 모드 자동 변경 처리 =====
    if (currentMode != targetMode) {
        if (!mode_change_pending) {
            mode_change_pending = true;
            mode_change_time = time_us_32();
            
            #ifdef DEBUG_MODE
            debugPrint("Mode change pending: %d -> %d", currentMode, targetMode);
            #endif
        } else if (time_us_32() - mode_change_time > MODE_CHANGE_DELAY_US) {
            if (PLAYER_NUMBER == 2) {
                sleep_ms(50);
            }
            
            options.inputMode = targetMode;
            Storage::getInstance().save();
            
            #ifdef DEBUG_MODE
            debugPrint("Mode changed, rebooting...");
            #endif
            
            uart_tx_wait_blocking(SERIAL_UART);
            sleep_ms(100);
            watchdog_enable(1, 1);
            while(1);
        }
    } else {
        mode_change_pending = false;
    }
    
    // ===== Step 5: GPIO 상태 백업 (선택적) =====
    memcpy(&previous_state, &gamepad->state, sizeof(GamepadState));
    
    // ===== Step 6: UART 데이터를 GPIO에 병합 =====
    if (currentMode == INPUT_MODE_KEYBOARD) {
        processKeyboardModeInput(gamepad);
    } else {
        processGamepadModeInput(gamepad);
    }
    
    #ifdef DEBUG_MODE
    static uint32_t last_stats_time = 0;
    if (time_us_32() - last_stats_time > 10000000) {
        printErrorStats();
        last_stats_time = time_us_32();
    }
    #endif
}

// ========================================
// 연결 복구 처리 (수정됨 - GPIO 유지!)
// ========================================
void SerialInputAddon::handleConnectionRecovery(Gamepad* gamepad) {
    if (was_timeout && !connection_timeout) {
        // ✅ 연결 복구 시 GPIO 입력 유지
        // clearGamepadState(gamepad);  ← 제거!
        was_timeout = false;
        
        #ifdef DEBUG_MODE
        debugPrint("Connection recovered - GPIO inputs preserved");
        #endif
    } else if (!was_timeout && connection_timeout) {
        was_timeout = true;
        // ✅ 타임아웃 시에도 GPIO 입력 유지
        // clearGamepadState(gamepad);  ← 제거!
        
        #ifdef DEBUG_MODE
        debugPrint("Connection timeout - falling back to GPIO only");
        #endif
    }
}

// ========================================
// 상태 업데이트 (LED + OLED)
// ========================================
void SerialInputAddon::updateStatusLED() {
    // static 변수들을 모두 제거함
    uint32_t now = time_us_32();
    
    #ifdef USE_WS2812_LED
    if (led_init_state != FINISHED) {
        if (led_init_state == NOT_STARTED) {
            led_init_state = ANIMATING;
            led_state_last_time = now;
        }
        if (now - led_state_last_time < 1000000) {
            if ((now / 200000) % 2 == 0) {
                if (PLAYER_NUMBER == 1) setLEDColor(0, 0, 255);
                else setLEDColor(255, 0, 0);
            } else {
                setLEDColor(0, 0, 0);
            }
            return;
        } else {
            led_init_state = FINISHED;
            // 상태 전환 시 타이머 초기화 (안전장치)
            led_state_last_time = now;
            led_mode_change_time = now;
        }
    } else {
        if (!connection_timeout) {
            if (serial_state.input_mode != led_last_mode) { // 'led_last_mode' 사용
                led_last_mode = serial_state.input_mode;
                led_mode_change_time = now;
            }
            
            uint8_t r = 0, g = 0, b = 0;
            switch (serial_state.input_mode) {
                case ARDUINO_MODE_KEYBOARD:
                    r = 60; g = 180; b = 75;  // 초록
                    break;
                case ARDUINO_MODE_XINPUT:
                    r = 0; g = 130; b = 200;  // 파랑
                    break;
                case ARDUINO_MODE_SWITCH:
                    r = 245; g = 130; b = 48;  // 주황
                    break;
                case ARDUINO_MODE_PS3:
                    r = 145; g = 30; b = 180;  // 보라색
                    break;
                case ARDUINO_MODE_PS4:
                    r = 240; g = 50; b = 230;  // 자홍
                    break;
                case ARDUINO_MODE_PS5:
                    r = 255; g = 215; b = 180;  // 부드러운 살구
                    break;
                case ARDUINO_MODE_XBOX:
                    r = 0; g = 0; b = 125;  // 어두운 남색
                    break;
                case ARDUINO_MODE_JVS:
                    r = 245; g = 130; b = 48;  // 활기찬 주황
                    break;
                case ARDUINO_MODE_DREAMCAST: 
                    r=170; g=110; b=40;   // 자연의 갈색
                    break;
                default:
                    r = 230; g = 25; b = 75; // 빨간색 (오류)
                    break;
            }
            
            if (now - led_mode_change_time < 1000000) { // 'led_mode_change_time' 사용
                if ((now / 150000) % 2 == 0) setLEDColor(r, g, b); // 접속 안됨.
                else setLEDColor(0, 0, 0);
            } else {
                setLEDColor(r, g, b);
            }
        } else {
            if (now - led_state_last_time > 500000) { // 'led_state_last_time' 사용
                led_blink_state = !led_blink_state; // 'led_blink_state' 사용
                if (led_blink_state) setLEDColor(128, 128, 128); // 중간 회색
                else setLEDColor(0, 0, 0);
                led_state_last_time = now;
            }
        }
    }
    #endif
    
    #ifdef USE_OLED_DISPLAY
    if (oled_available) {
        updateDisplay();
    }
    #endif
}

// ========================================
// 키보드 모드 입력 처리 (수정됨 - 병합 방식!)
// ========================================
void SerialInputAddon::processKeyboardModeInput(Gamepad* gamepad) {
    uint32_t current_buttons = (PLAYER_NUMBER == 1) ? 
                               serial_state.p1_buttons : 
                               serial_state.p2_buttons;
    
    uint8_t check_flags = serial_state.modes & 0x0F;
    bool analog_mode = false;
    
    if (PLAYER_NUMBER == 1) {
        analog_mode = (check_flags & 0x03) != 0;
    } else {
        analog_mode = (check_flags & 0x0C) != 0;
    }
    
    if (current_buttons & (1 << BIT_SELECT)) gamepad->state.buttons |= GAMEPAD_MASK_S1;
    if (current_buttons & (1 << BIT_HOME))   gamepad->state.buttons |= GAMEPAD_MASK_A1;
    if (current_buttons & (1 << BIT_START))  gamepad->state.buttons |= GAMEPAD_MASK_S2;
    
    if (current_buttons & (1 << BIT_A)) gamepad->state.buttons |= GAMEPAD_MASK_B1;
    if (current_buttons & (1 << BIT_B)) gamepad->state.buttons |= GAMEPAD_MASK_B2;
    if (current_buttons & (1 << BIT_X)) gamepad->state.buttons |= GAMEPAD_MASK_B3;
    if (current_buttons & (1 << BIT_Y)) gamepad->state.buttons |= GAMEPAD_MASK_B4;
    
    if (current_buttons & (1 << BIT_LB)) gamepad->state.buttons |= GAMEPAD_MASK_L1;
    if (current_buttons & (1 << BIT_RB)) gamepad->state.buttons |= GAMEPAD_MASK_R1;
    if (current_buttons & (1 << BIT_TOUCH)) gamepad->state.buttons |= GAMEPAD_MASK_A2; // ★ Touchpad (PS4) / Capture (Switch)
    
    uint8_t serial_lt = (current_buttons & (1 << BIT_LT)) ? 255 : 0;
    uint8_t serial_rt = (current_buttons & (1 << BIT_RT)) ? 255 : 0;
    if (serial_lt > gamepad->state.lt) gamepad->state.lt = serial_lt;
    if (serial_rt > gamepad->state.rt) gamepad->state.rt = serial_rt;
    
    // 방향키 상태
    bool up    = (current_buttons & (1 << BIT_UP)) != 0;
    bool down  = (current_buttons & (1 << BIT_DOWN)) != 0;
    bool right = (current_buttons & (1 << BIT_RIGHT)) != 0;
    bool left  = (current_buttons & (1 << BIT_LEFT)) != 0;
    
    if (analog_mode) {
        // 추가 버튼 매핑
        if (up)    gamepad->state.buttons |= GAMEPAD_MASK_E1;
        if (down)  gamepad->state.buttons |= GAMEPAD_MASK_E2;
        if (left)  gamepad->state.buttons |= GAMEPAD_MASK_E3;
        if (right) gamepad->state.buttons |= GAMEPAD_MASK_E4;
    } else {
        // 일반 모드
        if (up)    gamepad->state.dpad |= GAMEPAD_MASK_UP;
        if (down)  gamepad->state.dpad |= GAMEPAD_MASK_DOWN;
        if (left)  gamepad->state.dpad |= GAMEPAD_MASK_LEFT;
        if (right) gamepad->state.dpad |= GAMEPAD_MASK_RIGHT;
    }
    
    gamepad->state.lx = GAMEPAD_JOYSTICK_MID;
    gamepad->state.ly = GAMEPAD_JOYSTICK_MID;
    gamepad->state.rx = GAMEPAD_JOYSTICK_MID;
    gamepad->state.ry = GAMEPAD_JOYSTICK_MID;
}

// ========================================
// 게임패드 모드 입력 처리 (수정됨 - 병합 방식!)
// ========================================
void SerialInputAddon::processGamepadModeInput(Gamepad* gamepad) {
    uint32_t current_buttons = (PLAYER_NUMBER == 1) ? 
                               serial_state.p1_buttons : 
                               serial_state.p2_buttons;
    
    // 패드 모드 확인
    uint8_t check_flags = serial_state.modes & 0x0F;
    uint8_t pad_mode = PAD_MODE_DPAD;
    
    if (PLAYER_NUMBER == 1) {
        bool p1_lanl = (check_flags & (1 << 0)) != 0;
        bool p1_ranl = (check_flags & (1 << 1)) != 0;
        if (p1_lanl) pad_mode = PAD_MODE_LANALOG;
        else if (p1_ranl) pad_mode = PAD_MODE_RANALOG;
    } else {
        bool p2_lanl = (check_flags & (1 << 2)) != 0;
        bool p2_ranl = (check_flags & (1 << 3)) != 0;
        if (p2_lanl) pad_mode = PAD_MODE_LANALOG;
        else if (p2_ranl) pad_mode = PAD_MODE_RANALOG;
    }
    
    // ===== 버튼 병합 (초기화 제거!) =====
    // ✅ GPIO 입력 유지!
    // gamepad->state.buttons = 0;  ← 제거!
    
    // ✅ UART 데이터 추가
    if (current_buttons & (1UL << BIT_SELECT)) gamepad->state.buttons |= GAMEPAD_MASK_S1;
    if (current_buttons & (1UL << BIT_HOME))   gamepad->state.buttons |= GAMEPAD_MASK_A1;
    if (current_buttons & (1UL << BIT_START))  gamepad->state.buttons |= GAMEPAD_MASK_S2;
    
    if (current_buttons & (1UL << BIT_A)) gamepad->state.buttons |= GAMEPAD_MASK_B1;
    if (current_buttons & (1UL << BIT_B)) gamepad->state.buttons |= GAMEPAD_MASK_B2;
    if (current_buttons & (1UL << BIT_X)) gamepad->state.buttons |= GAMEPAD_MASK_B3;
    if (current_buttons & (1UL << BIT_Y)) gamepad->state.buttons |= GAMEPAD_MASK_B4;
    
    if (current_buttons & (1UL << BIT_LB)) gamepad->state.buttons |= GAMEPAD_MASK_L1;
    if (current_buttons & (1UL << BIT_RB)) gamepad->state.buttons |= GAMEPAD_MASK_R1;
    if (current_buttons & (1UL << BIT_TOUCH)) gamepad->state.buttons |= GAMEPAD_MASK_A2; // ★ Touchpad (PS4) / Capture (Switch)
    
    // 트리거 병합
    uint8_t serial_lt = (current_buttons & (1UL << BIT_LT)) ? 255 : 0;
    uint8_t serial_rt = (current_buttons & (1UL << BIT_RT)) ? 255 : 0;
    if (serial_lt > gamepad->state.lt) gamepad->state.lt = serial_lt;
    if (serial_rt > gamepad->state.rt) gamepad->state.rt = serial_rt;
    
    // 방향 입력
    bool up    = (current_buttons & (1UL << BIT_UP)) != 0;
    bool down  = (current_buttons & (1UL << BIT_DOWN)) != 0;
    bool right = (current_buttons & (1UL << BIT_RIGHT)) != 0;
    bool left  = (current_buttons & (1UL << BIT_LEFT)) != 0;
    
    // ===== 방향 입력 처리 (모드별) =====
    switch (pad_mode) {
        case PAD_MODE_DPAD:
            // ✅ D-pad 병합 (초기화 제거!)
            // gamepad->state.dpad = 0;  ← 제거!
            if (up)    gamepad->state.dpad |= GAMEPAD_MASK_UP;
            if (down)  gamepad->state.dpad |= GAMEPAD_MASK_DOWN;
            if (left)  gamepad->state.dpad |= GAMEPAD_MASK_LEFT;
            if (right) gamepad->state.dpad |= GAMEPAD_MASK_RIGHT;
            
            // ✅ 아날로그는 GPIO가 중립일 때만
            if (gamepad->state.lx == GAMEPAD_JOYSTICK_MID && 
                gamepad->state.ly == GAMEPAD_JOYSTICK_MID) {
                gamepad->state.lx = GAMEPAD_JOYSTICK_MID;
                gamepad->state.ly = GAMEPAD_JOYSTICK_MID;
            }
            
            if (gamepad->state.rx == GAMEPAD_JOYSTICK_MID && 
                gamepad->state.ry == GAMEPAD_JOYSTICK_MID) {
                gamepad->state.rx = GAMEPAD_JOYSTICK_MID;
                gamepad->state.ry = GAMEPAD_JOYSTICK_MID;
            }
            break;
            
        case PAD_MODE_LANALOG:
            // ✅ D-pad 병합
            // gamepad->state.dpad = 0;  ← 제거!
            
            // ✅ 왼쪽 스틱은 GPIO가 중립일 때만
            if (gamepad->state.lx == GAMEPAD_JOYSTICK_MID && 
                gamepad->state.ly == GAMEPAD_JOYSTICK_MID) {
                gamepad->state.lx = left ? GAMEPAD_JOYSTICK_MIN : 
                                   (right ? GAMEPAD_JOYSTICK_MAX : GAMEPAD_JOYSTICK_MID);
                gamepad->state.ly = up ? GAMEPAD_JOYSTICK_MIN : 
                                   (down ? GAMEPAD_JOYSTICK_MAX : GAMEPAD_JOYSTICK_MID);
            }
            
            // 오른쪽 스틱은 GPIO 유지
            if (gamepad->state.rx == GAMEPAD_JOYSTICK_MID && 
                gamepad->state.ry == GAMEPAD_JOYSTICK_MID) {
                gamepad->state.rx = GAMEPAD_JOYSTICK_MID;
                gamepad->state.ry = GAMEPAD_JOYSTICK_MID;
            }
            break;
            
        case PAD_MODE_RANALOG:
            // ✅ D-pad 병합
            // gamepad->state.dpad = 0;  ← 제거!
            
            // 왼쪽 스틱은 GPIO 유지
            if (gamepad->state.lx == GAMEPAD_JOYSTICK_MID && 
                gamepad->state.ly == GAMEPAD_JOYSTICK_MID) {
                gamepad->state.lx = GAMEPAD_JOYSTICK_MID;
                gamepad->state.ly = GAMEPAD_JOYSTICK_MID;
            }
            
            // ✅ 오른쪽 스틱은 GPIO가 중립일 때만
            if (gamepad->state.rx == GAMEPAD_JOYSTICK_MID && 
                gamepad->state.ry == GAMEPAD_JOYSTICK_MID) {
                gamepad->state.rx = left ? GAMEPAD_JOYSTICK_MIN : 
                                   (right ? GAMEPAD_JOYSTICK_MAX : GAMEPAD_JOYSTICK_MID);
                gamepad->state.ry = up ? GAMEPAD_JOYSTICK_MIN : 
                                   (down ? GAMEPAD_JOYSTICK_MAX : GAMEPAD_JOYSTICK_MID);
            }
            break;
    }
}

// ========================================
// Gamepad 상태 초기화
// ========================================
void SerialInputAddon::clearGamepadState(Gamepad* gamepad) {
    gamepad->state.dpad = 0;
    gamepad->state.buttons = 0;
    gamepad->state.lx = GAMEPAD_JOYSTICK_MID;
    gamepad->state.ly = GAMEPAD_JOYSTICK_MID;
    gamepad->state.rx = GAMEPAD_JOYSTICK_MID;
    gamepad->state.ry = GAMEPAD_JOYSTICK_MID;
    gamepad->state.lt = 0;
    gamepad->state.rt = 0;
}

// ========================================
// Gamepad 상태 병합
// ========================================
void SerialInputAddon::mergeGamepadStates(Gamepad* target, const GamepadState* source) {
    target->state.buttons |= source->buttons;
    target->state.dpad |= source->dpad;
    
    if (target->state.lx == GAMEPAD_JOYSTICK_MID && source->lx != GAMEPAD_JOYSTICK_MID) {
        target->state.lx = source->lx;
    }
    if (target->state.ly == GAMEPAD_JOYSTICK_MID && source->ly != GAMEPAD_JOYSTICK_MID) {
        target->state.ly = source->ly;
    }
    if (target->state.rx == GAMEPAD_JOYSTICK_MID && source->rx != GAMEPAD_JOYSTICK_MID) {
        target->state.rx = source->rx;
    }
    if (target->state.ry == GAMEPAD_JOYSTICK_MID && source->ry != GAMEPAD_JOYSTICK_MID) {
        target->state.ry = source->ry;
    }
    
    if (source->lt > target->state.lt) target->state.lt = source->lt;
    if (source->rt > target->state.rt) target->state.rt = source->rt;
}

// ========================================
// 시리얼 데이터 수신
// ========================================
void SerialInputAddon::receiveSerialData() {
    int bytes_processed = 0;
    
    while (uart_is_readable(SERIAL_UART) && bytes_processed < MAX_BYTES_PER_FRAME) {
        uint8_t byte = uart_getc(SERIAL_UART);
        bytes_processed++;
        
        // ===== 헤더 검출 =====
        if (byte == BINARY_HEADER) {
            buffer_index = 0;
            message_buffer[buffer_index++] = byte;
        }
        // ===== 메시지 수신 중 =====
        else if (buffer_index > 0 && buffer_index < sizeof(CompactMessage)) {
            message_buffer[buffer_index++] = byte;
            
            // ===== 메시지 완성 =====
            if (buffer_index == sizeof(CompactMessage)) {
                if (message_buffer[sizeof(CompactMessage) - 1] == BINARY_FOOTER) {
                    uint8_t calc_crc = calculateCRC8(message_buffer, sizeof(CompactMessage) - 2);
                    uint8_t msg_crc = message_buffer[sizeof(CompactMessage) - 2];
                    
                    if (calc_crc == msg_crc) {
                        CompactMessage msg;
                        memcpy(&msg, message_buffer, sizeof(CompactMessage));
                        
                        serial_state.p1_buttons = msg.p1_buttons;
                        serial_state.p2_buttons = msg.p2_buttons;
                        serial_state.input_mode = (msg.modes >> 4) & 0x0F; // 상위 4비트
                        serial_state.modes = msg.modes;
                        
                        serial_state.channel = (msg.channel_state >> 4) & 0x0F;
                        serial_state.channel_state = msg.channel_state;
                        
                        last_receive_time = time_us_32();
                        
                        #ifdef DEBUG_MODE
                        static uint32_t msg_count = 0;
                        if (++msg_count % 1000 == 0) {
                            debugPrint("Messages received: %lu", msg_count);
                        }
                        #endif
                    } else {
                        error_stats.crc_errors++;
                        #ifdef DEBUG_MODE
                        debugPrint("CRC error: calc=0x%02X, msg=0x%02X", calc_crc, msg_crc);
                        #endif
                    }
                } else {
                    error_stats.invalid_footers++;
                }
                buffer_index = 0;
            }
        }
        else {
            if (buffer_index > 0) {
                error_stats.invalid_headers++;
            }
            buffer_index = 0;
        }
    }
    
    if (bytes_processed >= MAX_BYTES_PER_FRAME && uart_is_readable(SERIAL_UART)) {
        error_stats.buffer_overflows++;
        #ifdef DEBUG_MODE
        static uint32_t last_overflow_report = 0;
        uint32_t now = time_us_32();
        if (now - last_overflow_report > 1000000) {
            debugPrint("Buffer overflow detected!");
            last_overflow_report = now;
        }
        #endif
    }
}

// ========================================
// Arduino 모드를 GP2040 모드로 변환
// ========================================
InputMode SerialInputAddon::convertToGP2040Mode(uint8_t arduino_mode) {
    switch (arduino_mode) {
        case ARDUINO_MODE_KEYBOARD: 
            return INPUT_MODE_KEYBOARD;
        case ARDUINO_MODE_XINPUT:   
            return INPUT_MODE_XINPUT;
        case ARDUINO_MODE_SWITCH:   
            return INPUT_MODE_SWITCH;
        case ARDUINO_MODE_PS3:      
            return INPUT_MODE_PS3;
        case ARDUINO_MODE_PS4:
            return INPUT_MODE_PS4;
        case ARDUINO_MODE_PS5:      
            return INPUT_MODE_PS5;
        case ARDUINO_MODE_XBOX:     
            return INPUT_MODE_XBOXORIGINAL;
        case ARDUINO_MODE_JVS:      
            return INPUT_MODE_XINPUT; // 재부팅 대비 기본값
        case ARDUINO_MODE_DREAMCAST:
            return INPUT_MODE_XINPUT; // 재부팅 대비 기본값
        default:                    
            return INPUT_MODE_XINPUT;
    }
}

// ========================================
// CRC8 체크섬 계산
// ========================================
uint8_t SerialInputAddon::calculateCRC8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0x00;
    
    while (len--) {
        uint8_t extract = *data++;
        
        for (uint8_t i = 8; i; i--) {
            uint8_t sum = (crc ^ extract) & 0x01;
            crc >>= 1;
            if (sum) {
                crc ^= 0x8C;
            }
            extract >>= 1;
        }
    }
    
    return crc;
}

#ifdef USE_OLED_DISPLAY

// ========================================
// OLED 존재 여부 체크 (매우 안전한 버전)
// ========================================
bool SerialInputAddon::checkOLEDAvailable() {
    // ★★★ 1. 단 한 번만 실행되도록 보장 (매우 중요) ★★★
    static bool already_checked = false;
    static bool last_result = false;
    if (already_checked) {
        return last_result;
    }
    already_checked = true;

    #ifdef DEBUG_MODE
    debugPrint("Checking for OLED display (defensive method)...");
    #endif

    // ★★★ 2. I2C 초기화 전, 핀 상태를 먼저 깨끗하게 설정 ★★★
    // 이전에 I2C가 비정상 종료되었을 경우를 대비합니다.
    gpio_disable_pulls(OLED_I2C_SDA_PIN);
    gpio_disable_pulls(OLED_I2C_SCL_PIN);
    gpio_set_function(OLED_I2C_SDA_PIN, GPIO_FUNC_SIO);
    gpio_set_function(OLED_I2C_SCL_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(OLED_I2C_SDA_PIN, GPIO_IN);
    gpio_set_dir(OLED_I2C_SCL_PIN, GPIO_IN);

    // I2C 버스를 깨끗한 상태에서 시작
    i2c_init(OLED_I2C_PORT, 400 * 1000);
    gpio_set_function(OLED_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_I2C_SCL_PIN, GPIO_FUNC_I2C);
    
    // ★★★ 3. 내부 풀업 저항 활성화 ★★★
    // 외부 풀업 저항이 없을 때, 버스 라인이 떠다니는(floating) 것을 방지합니다.
    gpio_pull_up(OLED_I2C_SDA_PIN);
    gpio_pull_up(OLED_I2C_SCL_PIN);
    sleep_ms(10); // 풀업이 안정화될 시간

    // ★★★ 4. 전체 검사 시간을 재는 타임아웃 설정 ★★★
    uint32_t start_time = time_us_32();
    int ret;
    uint8_t dummy_data = 0; // 주소만 확인하기 위한 더미 데이터

    // 매우 짧은 타임아웃(10ms)으로 쓰기를 시도합니다.
    ret = i2c_write_timeout_us(OLED_I2C_PORT, OLED_I2C_ADDR, &dummy_data, 1, false, 10000);

    uint32_t elapsed_time = time_us_32() - start_time;

    // ★★★ 5. 이중 타임아웃 체크 (핵심 로직) ★★★
    // ret < 0: SDK 함수가 타임아웃/에러를 반환함
    // elapsed_time > 50000: 함수 자체가 50ms 이상 멈춰버림 (하드웨어 멈춤)
    if (ret < 0 || elapsed_time > 50000) {
        #ifdef DEBUG_MODE
        debugPrint("OLED not found. ret=%d, elapsed=%lu us", ret, elapsed_time);
        #endif

        // ★★★ 6. 실패 시 I2C 하드웨어를 완전히 비활성화 (가장 중요) ★★★
        i2c_deinit(OLED_I2C_PORT);
        
        // GPIO 핀을 다시 입력(High-Z) 상태로 되돌려 시스템에 영향을 주지 않도록 합니다.
        gpio_disable_pulls(OLED_I2C_SDA_PIN);
        gpio_disable_pulls(OLED_I2C_SCL_PIN);
        gpio_set_function(OLED_I2C_SDA_PIN, GPIO_FUNC_SIO);
        gpio_set_function(OLED_I2C_SCL_PIN, GPIO_FUNC_SIO);
        gpio_set_dir(OLED_I2C_SDA_PIN, GPIO_IN);
        gpio_set_dir(OLED_I2C_SCL_PIN, GPIO_IN);

        last_result = false;
        return false;
    }

    #ifdef DEBUG_MODE
    debugPrint("OLED found successfully! ret=%d, elapsed=%lu us", ret, elapsed_time);
    #endif

    // 성공했으므로 I2C를 그대로 유지합니다.
    last_result = true;
    return true;
}

// ========================================
// OLED 명령어 전송 (안전)
// ========================================
void SerialInputAddon::oled_send_cmd(uint8_t cmd) {
    if (!oled_available) return;  // OLED 없으면 즉시 리턴
    
    uint8_t buf[2] = {0x00, cmd};
    i2c_write_timeout_us(OLED_I2C_PORT, OLED_I2C_ADDR, buf, 2, false, 10000);
}

// ========================================
// OLED 데이터 전송 (안전)
// ========================================
void SerialInputAddon::oled_send_data(uint8_t* data, size_t len) {
    if (!oled_available) return;  // OLED 없으면 즉시 리턴
    
    uint8_t buf[len + 1];
    buf[0] = 0x40; // Data mode
    memcpy(buf + 1, data, len);
    i2c_write_timeout_us(OLED_I2C_PORT, OLED_I2C_ADDR, buf, len + 1, false, 10000);
}

// ========================================
// OLED 초기화 (안전)
// ========================================
bool SerialInputAddon::setupOLED() {
    if (!oled_available) {
        return false;
    }
    
    #ifdef DEBUG_MODE
    debugPrint("Initializing OLED...");
    #endif
    
    sleep_ms(50);  // ← 100ms에서 50ms로 단축
    
    // OLED 초기화 시퀀스
    oled_send_cmd(OLED_DISPLAY_OFF);
    oled_send_cmd(OLED_SET_DISPLAY_CLOCK_DIV);
    oled_send_cmd(0x80);
    oled_send_cmd(OLED_SET_MULTIPLEX);
    oled_send_cmd(OLED_HEIGHT - 1);
    oled_send_cmd(OLED_SET_DISPLAY_OFFSET);
    oled_send_cmd(0x00);
    oled_send_cmd(OLED_SET_START_LINE | 0x00);
    oled_send_cmd(OLED_CHARGE_PUMP);
    oled_send_cmd(0x14);
    oled_send_cmd(OLED_MEMORY_MODE);
    oled_send_cmd(0x00);
    oled_send_cmd(OLED_SEG_REMAP | 0x01);
    oled_send_cmd(OLED_COM_SCAN_DEC);
    oled_send_cmd(OLED_SET_COM_PINS);
    oled_send_cmd(0x12);
    oled_send_cmd(OLED_SET_CONTRAST);
    oled_send_cmd(0xCF);
    oled_send_cmd(OLED_SET_PRECHARGE);
    oled_send_cmd(0xF1);
    oled_send_cmd(OLED_SET_VCOM_DETECT);
    oled_send_cmd(0x40);
    oled_send_cmd(OLED_DISPLAY_ALL_ON_RESUME);
    oled_send_cmd(OLED_NORMAL_DISPLAY);
    oled_send_cmd(OLED_DISPLAY_ON);
    
    #ifdef DEBUG_MODE
    debugPrint("OLED initialized successfully");
    #endif

    return true;
}

// ========================================
// OLED 화면 지우기 (안전)
// ========================================
void SerialInputAddon::oled_clear() {
    if (!oled_available) return;
    
    uint8_t zero[OLED_WIDTH] = {0};
    
    for (int page = 0; page < 8; page++) {
        oled_send_cmd(OLED_PAGE_ADDR);
        oled_send_cmd(page);
        oled_send_cmd(page);
        oled_send_cmd(OLED_COLUMN_ADDR);
        oled_send_cmd(0);
        oled_send_cmd(OLED_WIDTH - 1);
        oled_send_data(zero, OLED_WIDTH);
    }
}

// ========================================
// OLED 문자 출력 (안전)
// ========================================
void SerialInputAddon::oled_draw_char(uint8_t x, uint8_t y, char c, uint8_t size) {
    if (!oled_available) return;
    
    int char_index = -1;
    
    // 문자 인덱스 찾기
    if (c == ' ') {
        char_index = 0;
    } else if (c >= '0' && c <= '9') {
        char_index = 1 + (c - '0');
    } else if (c >= 'A' && c <= 'Z') {
        char_index = 11 + (c - 'A');
    } else if (c >= 'a' && c <= 'z') {
        char_index = 43 + (c - 'a');
    } else if (c == ':') {
        char_index = 69;
    } else {
        char_index = 0; // 공백
    }
    
    // 문자 데이터 출력
    uint8_t page = y / 8;
    oled_send_cmd(OLED_PAGE_ADDR);
    oled_send_cmd(page);
    oled_send_cmd(page + (size > 1 ? 1 : 0));
    oled_send_cmd(OLED_COLUMN_ADDR);
    oled_send_cmd(x);
    oled_send_cmd(x + 5 * size - 1);
    
    for (int col = 0; col < 5; col++) {
        uint8_t col_data = font5x7[char_index][col];
        for (int s = 0; s < size; s++) {
            oled_send_data(&col_data, 1);
        }
    }
}

// ========================================
// OLED 문자열 출력 (안전)
// ========================================
void SerialInputAddon::oled_draw_string(uint8_t x, uint8_t y, const char* str, uint8_t size) {
    if (!oled_available) return;
    
    uint8_t cursor_x = x;
    while (*str) {
        oled_draw_char(cursor_x, y, *str++, size);
        cursor_x += 6 * size; // 5 픽셀 + 1 픽셀 간격
    }
}

// ========================================
// 현재 모드를 문자열로 변환
// ========================================
const char* SerialInputAddon::getModeString(uint8_t mode) {
    switch (mode) {
        case ARDUINO_MODE_KEYBOARD:
            return "KEYBOARD";
        case ARDUINO_MODE_XINPUT:
            return "XINPUT";
        case ARDUINO_MODE_SWITCH:   
            return "SWITCH";
        case ARDUINO_MODE_PS3:      
            return "PS3";
        case ARDUINO_MODE_PS4:      
            return "PS4";
        case ARDUINO_MODE_PS5:      
            return "PS5";
        case ARDUINO_MODE_XBOX:     
            return "XBOX";
        case ARDUINO_MODE_JVS:      
            return "JVS";
        case ARDUINO_MODE_DREAMCAST:
            return "DREAMCAST";
        default:                    
            return "UNKNOWN";
    }
}

// ========================================
// 패드 모드를 문자열로 변환
// ========================================
const char* SerialInputAddon::getPadModeString(uint8_t modes, bool is_player1) {
    uint8_t check_flags = modes & 0x0F;
    
    if (is_player1) {
        bool p1_lanl = (check_flags & (1 << 0)) != 0;
        bool p1_ranl = (check_flags & (1 << 1)) != 0;
        
        if (p1_lanl) return "LANL";
        else if (p1_ranl) return "RANL";
        else return "DPAD";
    } else {
        bool p2_lanl = (check_flags & (1 << 2)) != 0;
        bool p2_ranl = (check_flags & (1 << 3)) != 0;
        
        if (p2_lanl) return "LANL";
        else if (p2_ranl) return "RANL";
        else return "DPAD";
    }
}

// ========================================
// SWAP 상태 확인
// ========================================
bool SerialInputAddon::isSwapped() {
    return (serial_state.channel_state & 0x01) != 0;
}

// ========================================
// 스타일 1: 기본 디스플레이 (안전)
// ========================================
void SerialInputAddon::updateDisplay() {
    if (!oled_available) return;  // OLED 없으면 즉시 리턴
    
    static uint8_t last_mode = 0xFF;
    static uint8_t last_modes = 0xFF;
    static uint8_t last_channel = 0xFF;
    static uint32_t last_update = 0;
    uint32_t now = time_us_32();
    
    // 50ms마다 업데이트 체크
    if (now - last_update < 50000) {
        return;
    }
    
    // 변경사항이 있을 때만 업데이트
    if (serial_state.input_mode == last_mode && 
        serial_state.modes == last_modes &&
        serial_state.channel == last_channel) {
        return;
    }
    
    last_mode = serial_state.input_mode;
    last_modes = serial_state.modes;
    last_channel = serial_state.channel;
    last_update = now;
    
    // 화면 지우기
    oled_clear();
    
    // ===== 첫 번째 줄: 현재 모드 (크게 표시) =====
    const char* mode_str = getModeString(serial_state.input_mode);
    char mode_line[32];
    sprintf(mode_line, "MODE:%s", mode_str);
    oled_draw_string(5, 0, mode_line, 2);
    
    // ===== 두 번째 줄: 패드 모드 상태 =====
    const char* p1_pad = getPadModeString(serial_state.modes, true);
    const char* p2_pad = getPadModeString(serial_state.modes, false);
    
    char pad_line[32];
    sprintf(pad_line, "1P:%s 2P:%s", p1_pad, p2_pad);
    oled_draw_string(5, 24, pad_line, 1);
    
    // ===== 세 번째 줄: SWAP 상태 =====
    bool swapped = isSwapped();
    const char* swap_str = swapped ? "YES" : "NO";
    char swap_line[32];
    sprintf(swap_line, "SWAP:%s", swap_str);
    oled_draw_string(5, 40, swap_line, 1);
    
    // ===== 네 번째 줄: 연결 상태 =====
    if (connection_timeout) {
        oled_draw_string(15, 56, "DISCONNECTED", 1);
    } else {
        oled_draw_string(30, 56, "CONNECTED", 1);
    }
    
    #ifdef DEBUG_MODE
    debugPrint("Display: %s | 1P:%s 2P:%s | SWAP:%s", 
               mode_str, p1_pad, p2_pad, swap_str);
    #endif
}

// ========================================
// 스타일 2: 컴팩트 디스플레이 (안전)
// ========================================
void SerialInputAddon::updateDisplayCompact() {
    if (!oled_available) return;
    
    static uint8_t last_mode = 0xFF;
    static uint8_t last_modes = 0xFF;
    static uint8_t last_channel = 0xFF;
    static uint32_t last_update = 0;
    uint32_t now = time_us_32();
    
    if (now - last_update < 50000) return;
    
    if (serial_state.input_mode == last_mode && 
        serial_state.modes == last_modes &&
        serial_state.channel == last_channel) {
        return;
    }
    
    last_mode = serial_state.input_mode;
    last_modes = serial_state.modes;
    last_channel = serial_state.channel;
    last_update = now;
    
    oled_clear();
    
    const char* mode_str = getModeString(serial_state.input_mode);
    oled_draw_string(10, 0, mode_str, 2);
    
    const char* p1_pad = getPadModeString(serial_state.modes, true);
    const char* p2_pad = getPadModeString(serial_state.modes, false);
    bool swapped = isSwapped();
    
    char status_line[32];
    sprintf(status_line, "1P:%s 2P:%s %s", 
            p1_pad, p2_pad, swapped ? "SWAP" : "");
    oled_draw_string(5, 32, status_line, 1);
    
    if (!connection_timeout) {
        oled_draw_string(45, 56, "[OK]", 1);
    }
}

// ========================================
// 스타일 3: 플레이어 중심 디스플레이 (안전)
// ========================================
void SerialInputAddon::updateDisplayPlayer() {
    if (!oled_available) return;
    
    static uint8_t last_mode = 0xFF;
    static uint8_t last_modes = 0xFF;
    static uint8_t last_channel = 0xFF;
    static uint32_t last_update = 0;
    uint32_t now = time_us_32();
    
    if (now - last_update < 50000) return;
    
    if (serial_state.input_mode == last_mode && 
        serial_state.modes == last_modes &&
        serial_state.channel == last_channel) {
        return;
    }
    
    last_mode = serial_state.input_mode;
    last_modes = serial_state.modes;
    last_channel = serial_state.channel;
    last_update = now;
    
    oled_clear();
    
    char player[16];
    sprintf(player, "P%d", PLAYER_NUMBER);
    oled_draw_string(5, 0, player, 3);
    
    const char* mode_str = getModeString(serial_state.input_mode);
    oled_draw_string(50, 8, mode_str, 1);
    
    const char* my_pad = getPadModeString(serial_state.modes, PLAYER_NUMBER == 1);
    char pad_str[16];
    sprintf(pad_str, "PAD:%s", my_pad);
    oled_draw_string(5, 30, pad_str, 2);
    
    const char* other_pad = getPadModeString(serial_state.modes, PLAYER_NUMBER != 1);
    char other_str[32];
    sprintf(other_str, "P%d:%s %s", 
            PLAYER_NUMBER == 1 ? 2 : 1, 
            other_pad,
            isSwapped() ? "SWAP" : "");
    oled_draw_string(5, 52, other_str, 1);
}

// ========================================
// 스타일 4: 아이콘 스타일 (안전)
// ========================================
void SerialInputAddon::updateDisplayIcon() {
    if (!oled_available) return;
    
    static uint8_t last_mode = 0xFF;
    static uint8_t last_modes = 0xFF;
    static uint32_t last_update = 0;
    uint32_t now = time_us_32();
    
    if (now - last_update < 50000) return;
    
    if (serial_state.input_mode == last_mode && 
        serial_state.modes == last_modes) {
        return;
    }
    
    last_mode = serial_state.input_mode;
    last_modes = serial_state.modes;
    last_update = now;
    
    oled_clear();
    
    const char* mode_icon;
    switch (serial_state.input_mode) {
        case ARDUINO_MODE_KEYBOARD: mode_icon = "KB"; break;
        case ARDUINO_MODE_XINPUT:   mode_icon = "XI"; break;
        case ARDUINO_MODE_SWITCH:   mode_icon = "SW"; break;
        case ARDUINO_MODE_PS3:      mode_icon = "P3"; break;
        case ARDUINO_MODE_PS4:      mode_icon = "P4"; break;
        case ARDUINO_MODE_PS5:      mode_icon = "P5"; break;
        case ARDUINO_MODE_XBOX:     mode_icon = "XB"; break;
        default:                    mode_icon = "??"; break;
    }
    
    oled_draw_string(40, 0, mode_icon, 3);
    
    const char* p1_icon;
    const char* p2_icon;
    
    uint8_t check_flags = serial_state.modes & 0x0F;
    if (check_flags & (1 << 0)) p1_icon = "L";
    else if (check_flags & (1 << 1)) p1_icon = "R";
    else p1_icon = "D";
    
    if (check_flags & (1 << 2)) p2_icon = "L";
    else if (check_flags & (1 << 3)) p2_icon = "R";
    else p2_icon = "D";
    
    char status[32];
    sprintf(status, "1:%s  2:%s  %s", 
            p1_icon, p2_icon, 
            isSwapped() ? "S" : "-");
    oled_draw_string(10, 48, status, 2);
}

#endif // USE_OLED_DISPLAY