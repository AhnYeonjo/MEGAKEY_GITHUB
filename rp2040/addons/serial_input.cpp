// src/addons/serial_input.cpp
// GP2040-CE Serial Input Addon Implementation
// Version: 2.3 - Display Addon disabled, OLED used exclusively
//
// Notes:
// 1. GP2040-CE's Display Addon MUST be disabled (prevents I2C conflict)
// 2. Keep PlayerLEDAddon enabled (manages WS2812)
// 3. SerialInputAddon uses the OLED exclusively to show serial status

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
// 5x7 font data
// ========================================
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space (0)
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
// ★★★ Dreamcast communication logic (fully reworked) ★★★
// ========================================

// Dreamcast controller function code
#define DREAMCAST_FUNC_CONTROLLER 0x01000000

// Flag indicating the last word of a Maple Bus frame
#define MAPLE_FRAME_LAST_WORD 0x80

// Main commands sent by the Dreamcast host
#define MAPLE_COMMAND_DEVINFO   0x01 // device info request
#define MAPLE_COMMAND_GETCOND   0x09 // controller status request

// The Maple Bus CRC is a simple XOR of all bytes.
// Maple Bus CRC calculation helper
uint8_t calculateMapleCrc(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
    }
    return crc;
}

// PIO init function (called once from setup())
void SerialInputAddon::setupDreamcastPIO() {
    // 1. Safely find an available PIO instance and state machine (SM).
    PIO pio = pio0;
    int sm = pio_claim_unused_sm(pio, false);
    if (sm == -1) {
        pio = pio1;
        sm = pio_claim_unused_sm(pio, true);
    }

    #ifdef DEBUG_MODE
    debugPrint("Dreamcast PIO assigned to pio%d, sm%d", (pio == pio0) ? 0 : 1, sm);
    #endif

    // 2. Load the PIO program into memory.
    uint offset = pio_add_program(pio, &maple_bus_program);

    // 3. Initialize the PIO with the found resources.
    dreamcast_pio = pio;
    dreamcast_sm = sm;
    dreamcast_offset = offset;
    
    // 4. Call the helper generated in .pio.h to configure the PIO SM defaults.
    //    This function sets clock divider, FIFO config, shift direction, etc.
    maple_bus_program_init(dreamcast_pio, dreamcast_sm, dreamcast_offset, DREAMCAST_MAPLE_A_PIN, DREAMCAST_PIO_ENABLE_PIN);
}

// Configure and start the PIO in listening mode
void SerialInputAddon::startMapleListen() {
    pio_sm_set_enabled(dreamcast_pio, dreamcast_sm, false);

    // 2. Set the pin direction to input.
    //    This is how the CPU controls the PIO operating mode (RX/TX).
    pio_sm_set_consecutive_pindirs(dreamcast_pio, dreamcast_sm, DREAMCAST_MAPLE_A_PIN, 2, false); // false = Input
    
    // 3. Call the Pico SDK's pio_gpio_init() so those pins are driven by the PIO.
    pio_gpio_init(dreamcast_pio, DREAMCAST_MAPLE_A_PIN);
    pio_gpio_init(dreamcast_pio, DREAMCAST_MAPLE_A_PIN + 1);

    // 4. Force the PIO program counter (PC) to the start of the RX section.
    const uint offset_rx = 7; // Manually set offset for rx_entry (SDK < 1.5.0 compatible)
    pio_sm_exec(dreamcast_pio, dreamcast_sm, pio_encode_jmp(dreamcast_offset + offset_rx));
    
    // 5. Drain the FIFO to clear any leftover data.
    pio_sm_clear_fifos(dreamcast_pio, dreamcast_sm);

    // 6. With setup complete, enable the state machine to start listening.
    pio_sm_set_enabled(dreamcast_pio, dreamcast_sm, true);

    maple_state = LISTENING;
}


// ★★★ sendDreamcastReport function (fully rewritten) ★★★
// This function transmits the response frame prepared by the CPU via the PIO.
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

    // ★★★ Wait for the TX FIFO to drain instead of using sleep_us() ★★★
    while (!pio_sm_is_tx_fifo_empty(dreamcast_pio, dreamcast_sm)) {
        tight_loop_contents();
    }
    
    pio_sm_set_enabled(dreamcast_pio, dreamcast_sm, false);
    startMapleListen(); 
}

// ★★★ Main Dreamcast output/handling function (fully rewritten) ★★★
// src/addons/serial_input.cpp

// ★★★ Final completed processDreamcastOutput function ★★★
void SerialInputAddon::processDreamcastOutput(const GamepadState& state) {
    // --- Step 1: check current state ---
    // If the PIO is not in the 'LISTENING' state (ready to receive), return immediately to avoid bus collision.
    if (maple_state != LISTENING) {
        return;
    }

    // --- Step 2: check and process received data ---
    // Check whether the PIO's RX FIFO (receive buffer) holds data.
    if (pio_sm_get_rx_fifo_level(dreamcast_pio, dreamcast_sm) > 0) {
        
        // ★★★ Bug fix: refresh the timeout timer immediately when data is received. ★★★
        // This prevents the PIO from being disabled during normal communication.
        last_dreamcast_rx_time = time_us_32();

        // [Stability] Read out all queued requests in the FIFO, keeping only the latest.
        // Under system latency this discards already-timed-out stale requests and
        // responds to the most recent request to recover communication.
        uint32_t command_word;
        while (pio_sm_get_rx_fifo_level(dreamcast_pio, dreamcast_sm) > 0) {
            command_word = pio_sm_get(dreamcast_pio, dreamcast_sm);
        }

        // --- Step 3: parse command and build response ---
        uint8_t command = (command_word >> 8) & 0xFF;
        uint8_t response_packet[32];
        size_t response_len = 0;

        switch (command) {
            case MAPLE_COMMAND_DEVINFO: {
                // Answer to "who am I": "I am a standard Dreamcast controller."
                static const uint8_t dev_info_response[] = {
                    // --- Maple Bus frame header ---
                    0x05, 0xFE, 0x00, 0x05, 
                    // --- data payload (decomposed Little Endian) ---
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
                // Answer to "what is the button state"
                response_len = 16; // 4 words (header+function code+data)
                memset(response_packet, 0, response_len);

                // header
                response_packet[0] = 0x08; // response code: data transfer
                response_packet[1] = 0xFE; // destination address: To Host
                response_packet[2] = 0x00; // source address: Port A
                response_packet[3] = 0x02; // data length: 2 words (8 bytes)

                // payload
                ((uint32_t*)response_packet)[1] = DREAMCAST_FUNC_CONTROLLER;
                uint8_t* controller_data = &response_packet[8];

                // button state (Active Low: 0 when pressed)
                uint16_t buttons = 0xFFFF;
                if (state.buttons & GAMEPAD_MASK_B3) buttons &= ~(1 << 0); // C button (GP2040 B3/X)
                if (state.buttons & GAMEPAD_MASK_B4) buttons &= ~(1 << 1); // B button (GP2040 B4/Y)
                if (state.buttons & GAMEPAD_MASK_B1) buttons &= ~(1 << 2); // A button (GP2040 B1/A)
                if (state.buttons & GAMEPAD_MASK_B2) buttons &= ~(1 << 3); // X button (GP2040 B2/B)
                if (state.dpad & GAMEPAD_MASK_UP)    buttons &= ~(1 << 4);
                if (state.dpad & GAMEPAD_MASK_DOWN)  buttons &= ~(1 << 5);
                if (state.dpad & GAMEPAD_MASK_LEFT)  buttons &= ~(1 << 6);
                if (state.dpad & GAMEPAD_MASK_RIGHT) buttons &= ~(1 << 7);
                if (state.buttons & GAMEPAD_MASK_S2) buttons &= ~(1 << 9); // Start button

                // fill values into the data buffer
                controller_data[0] = buttons & 0xFF;
                controller_data[1] = (buttons >> 8) & 0xFF;
                controller_data[2] = state.rt;
                controller_data[3] = state.lt;
                controller_data[4] = state.lx >> 8;
                controller_data[5] = state.ly >> 8;
                controller_data[6] = 0x80; // 2nd Stick X (neutral)
                controller_data[7] = 0x80; // 2nd Stick Y (neutral)
                break;
            }
            default: // ignore unknown commands
                break;
        }

        // --- Step 4: send response ---
        if (response_len > 0) {
            response_packet[0] |= MAPLE_FRAME_LAST_WORD; // set last-frame flag
            response_packet[response_len - 1] = calculateMapleCrc(response_packet, response_len - 1); // compute and append CRC
            sendDreamcastReport(response_packet, response_len); // transmit via PIO
        }
    }
}

// ========================================
// Check whether the addon is available
// ========================================
bool SerialInputAddon::available() {    
    return true;
}

// ========================================
// UART safety check
// ========================================
bool SerialInputAddon::checkUartSafety() {
    const GamepadOptions& options = Storage::getInstance().getGamepadOptions();
    return true;
}

// ========================================
// WS2812 LED init (RP2040 Zero) - safe version
// ========================================
#ifdef USE_WS2812_LED

void SerialInputAddon::setupWS2812() {
    // 1. Explicitly select **pio1**, avoiding pio0 used by the USB host.
    //    This is the key fix for the issue.
    PIO pio = pio1;

    // 2. Find an available state machine (SM) on pio1.
    //    The 'true' option halts the system if no SM is available.
    //    This helps diagnose the rare case where other addons have consumed
    //    all of pio1 as well.
    int sm = pio_claim_unused_sm(pio, true);

    // 3. Initialize NeoPico with **the exact resources (pio1 and the just-found sm)**.
    //    - WS2812_PIN: LED data pin number (16)
    //    - 1           : number of LEDs to drive (start safely with 1)
    //    - LED_FORMAT_GRB: standard WS2812 format
    //    - pio         : the pio1 we specified
    //    - sm          : the free state machine found on pio1
    neoPico.Setup(WS2812_PIN, 1, LED_FORMAT_GRB, pio, sm);

    // 4. Turn off all LEDs after init.
    neoPico.Off();
}

void SerialInputAddon::setLEDColor(uint8_t r, uint8_t g, uint8_t b) {
    // 1. Combine r, g, b into a single 32-bit RGB color value.
    //    The NeoPico class takes this format as input.
    uint32_t pixel_color = (r << 8) | (g << 16) | b;

    // 2. Call NeoPico's PutPixel to set the LED color.
    //    - This function internally converts RGB to GRB and
    //      sends the data to the LED over the configured PIO.
    //    - PutPixel is 'blocking', so it waits until the transfer finishes.
    //      (With 1 LED it completes almost instantly.)
    neoPico.PutPixel(pixel_color);
}
#endif

// ========================================
// Initialize
// ========================================
void SerialInputAddon::setup() {
    #ifdef DEBUG_MODE
    sleep_ms(100);
    debugPrint("SerialInputAddon setup started (Player %d)", PLAYER_NUMBER);
    #endif

    // ===== Step 1: UART init (fast, non-blocking) =====
    uart_init(SERIAL_UART, SERIAL_BAUDRATE);
    gpio_set_function(SERIAL_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(SERIAL_UART, false, false);
    uart_set_format(SERIAL_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(SERIAL_UART, true);
    
    // ===== Step 2: WS2812 LED init (safely) =====
    // Initialize LED state variables
    led_state_last_time = 0;
    led_mode_change_time = 0;
    led_blink_state = false;
    led_init_state = NOT_STARTED; // init animation not started yet
    led_last_mode = 0xFF; // extra init

    #ifdef USE_WS2812_LED
    setupWS2812();
    
    #endif
    
    // ===== Step 3: OLED init (fast and safe) =====
    #ifdef USE_OLED_DISPLAY
    #ifdef DEBUG_MODE
    debugPrint("Checking OLED...");
    #endif
    
    // ✅ Try only once; on failure continue immediately
    oled_available = checkOLEDAvailable();
    
    if (oled_available) {
        #ifdef DEBUG_MODE
        debugPrint("OLED detected - initializing");
        #endif
        
        if (setupOLED()) {
            oled_clear();
            
            // ✅ simple init message
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
    
    // ===== Step 4: state reset (fast) =====
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
    
    // ===== Step 5: enable addon =====
    addon_enabled = true;
    
    #ifdef DEBUG_MODE
    debugPrint("Initializing Dreamcast Output support...");
    #endif
    gpio_init(DREAMCAST_PIO_ENABLE_PIN);
    gpio_set_dir(DREAMCAST_PIO_ENABLE_PIN, GPIO_OUT);
    gpio_put(DREAMCAST_PIO_ENABLE_PIN, 1); // set default state to 'idle' (HIGH)

    gpio_init(DREAMCAST_SENSE_PIN);
    gpio_set_dir(DREAMCAST_SENSE_PIN, GPIO_OUT);
    gpio_put(DREAMCAST_SENSE_PIN, 0);
    
    setupDreamcastPIO();
    startMapleListen(); 

    maple_state = LISTENING;
    last_dreamcast_rx_time = time_us_32(); // init timeout timer
    dreamcast_pio_active = true;           // start with PIO active

    // ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
    // ★★★ 2. Add level-shifter enable logic (P1 only, after all init completes) ★★★
    // ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
    #if PLAYER_NUMBER == 1
        // Init the control pin and set it as output.
        gpio_init(LEVEL_SHIFTER_ENABLE_PIN);
        gpio_set_dir(LEVEL_SHIFTER_ENABLE_PIN, GPIO_OUT);
        
        // Drive the OE pin HIGH to enable the TXS0108E.
        gpio_put(LEVEL_SHIFTER_ENABLE_PIN, 1);
    #endif

    #ifdef DEBUG_MODE
    debugPrint("Setup complete - UART%d on GP%d at %d baud", 
               (SERIAL_UART == uart0) ? 0 : 1, SERIAL_RX_PIN, SERIAL_BAUDRATE);
    debugPrint("WS2812 status: %s", "ENABLED");
    debugPrint("OLED status: %s", oled_available ? "ENABLED" : "DISABLED");
    #endif
    
    // ✅ Total setup() time: within ~600ms
}

// ========================================
// Debug output (USB CDC)
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
// Preprocess (data receive)
// ========================================
void SerialInputAddon::preprocess() {
    if (!addon_enabled) return;
    
    // Receive serial data
    receiveSerialData();
    
    // Connection timeout check
    if (time_us_32() - last_receive_time > CONNECTION_TIMEOUT_US) {
        if (connection_timeout == false) {
            // The exact moment state goes 'connected' -> 'disconnected'
            #ifdef DEBUG_MODE
            debugPrint("Connection timeout detected");
            #endif
            memset(&serial_state, 0, sizeof(serial_state));
        }
        connection_timeout = true; // Set true whenever the timeout condition holds
    } else {
        connection_timeout = false; // Set false whenever the timeout condition is not met
    }
    
    // Update LED state
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
// Main process
// ========================================
void SerialInputAddon::process() {
    if (!addon_enabled) return;
    
    Gamepad* gamepad = Storage::getInstance().GetGamepad();
    GamepadOptions& options = Storage::getInstance().getGamepadOptions();
    
    // ===== KeyboardMapping lazy init (once) =====
    static bool keyboard_init_attempted = false;
    if (!keyboard_init_attempted && addon_enabled) {
        keyboard_init_attempted = true;
        
        // Leave time after boot (500ms)
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

    // ===== Step 1: connection-recovery handling (no reset!) =====
    handleConnectionRecovery(gamepad);
    
    // ===== Step 2: connection state check =====
    if (connection_timeout) {
        // ✅ Without UART use GPIO only (already handled in the main loop)
        return;
    }
    
    // ★★★★★ Dreamcast PIO timeout management (new logic) ★★★★★
    if (serial_state.input_mode == ARDUINO_MODE_DREAMCAST) {
        uint32_t now = time_us_32();

        // 1. A timeout occurred and the PIO was active?
        if ((now - last_dreamcast_rx_time > DREAMCAST_RX_TIMEOUT_US) && dreamcast_pio_active) {
            #ifdef DEBUG_MODE
            debugPrint("Dreamcast RX timeout. Deactivating PIO wait.");
            #endif
            gpio_put(DREAMCAST_PIO_ENABLE_PIN, 1); // JMP the PIO to a safe loop
            dreamcast_pio_active = false;
        }
        // 2. PIO inactive but needs re-enabling? (e.g. right after entering a mode)
        else if (!dreamcast_pio_active) {
            #ifdef DEBUG_MODE
            debugPrint("Re-activating PIO for Dreamcast mode.");
            #endif
            gpio_put(DREAMCAST_PIO_ENABLE_PIN, 0); // Let the PIO detect communication
            last_dreamcast_rx_time = now; // restart the timeout timer
            dreamcast_pio_active = true;
        }
    } else {
        // When not in Dreamcast mode, always keep the PIO disabled.
        if (dreamcast_pio_active) {
            gpio_put(DREAMCAST_PIO_ENABLE_PIN, 1);
            dreamcast_pio_active = false;
        }
    }

    // ★★★ Dreamcast mode special handling ★★★
    if (serial_state.input_mode == ARDUINO_MODE_DREAMCAST) {
        Gamepad* gamepad = Storage::getInstance().GetGamepad();
        processGamepadModeInput(gamepad); // merge input
        processDreamcastOutput(gamepad->state); // output to Dreamcast
        return; // skip USB handling
    }
    
    // ★★★ Check JVS mode right here and block input ★★★
    if (serial_state.input_mode == ARDUINO_MODE_JVS) {
        // In JVS mode, exit without processing USB input.
        return; 
    }
    
    // ===== Step 3: mode conversion =====
    InputMode targetMode = convertToGP2040Mode(serial_state.input_mode);
    InputMode currentMode = options.inputMode;
    
    // ===== Step 4: auto mode-change handling =====
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
    
    // ===== Step 5: GPIO state backup (optional) =====
    memcpy(&previous_state, &gamepad->state, sizeof(GamepadState));
    
    // ===== Step 6: merge UART data into GPIO =====
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
// Connection-recovery handling (revised - keeps GPIO!)
// ========================================
void SerialInputAddon::handleConnectionRecovery(Gamepad* gamepad) {
    if (was_timeout && !connection_timeout) {
        // ✅ Keep GPIO input on reconnect
        // clearGamepadState(gamepad);  ← removed!
        was_timeout = false;
        
        #ifdef DEBUG_MODE
        debugPrint("Connection recovered - GPIO inputs preserved");
        #endif
    } else if (!was_timeout && connection_timeout) {
        was_timeout = true;
        // ✅ Keep GPIO input even on timeout
        // clearGamepadState(gamepad);  ← removed!
        
        #ifdef DEBUG_MODE
        debugPrint("Connection timeout - falling back to GPIO only");
        #endif
    }
}

// ========================================
// State update (LED + OLED)
// ========================================
void SerialInputAddon::updateStatusLED() {
    // all static variables removed
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
            // reset timer on state transition (safety)
            led_state_last_time = now;
            led_mode_change_time = now;
        }
    } else {
        if (!connection_timeout) {
            if (serial_state.input_mode != led_last_mode) { // uses 'led_last_mode'
                led_last_mode = serial_state.input_mode;
                led_mode_change_time = now;
            }
            
            uint8_t r = 0, g = 0, b = 0;
            switch (serial_state.input_mode) {
                case ARDUINO_MODE_KEYBOARD:
                    r = 60; g = 180; b = 75;  // green
                    break;
                case ARDUINO_MODE_XINPUT:
                    r = 0; g = 130; b = 200;  // blue
                    break;
                case ARDUINO_MODE_SWITCH:
                    r = 245; g = 130; b = 48;  // orange
                    break;
                case ARDUINO_MODE_PS3:
                    r = 145; g = 30; b = 180;  // purple
                    break;
                case ARDUINO_MODE_PS4:
                    r = 240; g = 50; b = 230;  // magenta
                    break;
                case ARDUINO_MODE_PS5:
                    r = 255; g = 215; b = 180;  // soft apricot
                    break;
                case ARDUINO_MODE_XBOX:
                    r = 0; g = 0; b = 125;  // dark navy
                    break;
                case ARDUINO_MODE_JVS:
                    r = 245; g = 130; b = 48;  // vivid orange
                    break;
                case ARDUINO_MODE_DREAMCAST: 
                    r=170; g=110; b=40;   // natural brown
                    break;
                default:
                    r = 230; g = 25; b = 75; // red (error)
                    break;
            }
            
            if (now - led_mode_change_time < 1000000) { // uses 'led_mode_change_time'
                if ((now / 150000) % 2 == 0) setLEDColor(r, g, b); // not connected.
                else setLEDColor(0, 0, 0);
            } else {
                setLEDColor(r, g, b);
            }
        } else {
            if (now - led_state_last_time > 500000) { // uses 'led_state_last_time'
                led_blink_state = !led_blink_state; // uses 'led_blink_state'
                if (led_blink_state) setLEDColor(128, 128, 128); // medium gray
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
// Keyboard-mode input handling (revised - merge style!)
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
    
    // D-pad state
    bool up    = (current_buttons & (1 << BIT_UP)) != 0;
    bool down  = (current_buttons & (1 << BIT_DOWN)) != 0;
    bool right = (current_buttons & (1 << BIT_RIGHT)) != 0;
    bool left  = (current_buttons & (1 << BIT_LEFT)) != 0;
    
    if (analog_mode) {
        // extra button mapping
        if (up)    gamepad->state.buttons |= GAMEPAD_MASK_E1;
        if (down)  gamepad->state.buttons |= GAMEPAD_MASK_E2;
        if (left)  gamepad->state.buttons |= GAMEPAD_MASK_E3;
        if (right) gamepad->state.buttons |= GAMEPAD_MASK_E4;
    } else {
        // normal mode
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
// Gamepad-mode input handling (revised - merge style!)
// ========================================
void SerialInputAddon::processGamepadModeInput(Gamepad* gamepad) {
    uint32_t current_buttons = (PLAYER_NUMBER == 1) ? 
                               serial_state.p1_buttons : 
                               serial_state.p2_buttons;
    
    // check pad mode
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
    
    // ===== button merge (no reset!) =====
    // ✅ keep GPIO input!
    // gamepad->state.buttons = 0;  ← removed!
    
    // ✅ add UART data
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
    
    // merge triggers
    uint8_t serial_lt = (current_buttons & (1UL << BIT_LT)) ? 255 : 0;
    uint8_t serial_rt = (current_buttons & (1UL << BIT_RT)) ? 255 : 0;
    if (serial_lt > gamepad->state.lt) gamepad->state.lt = serial_lt;
    if (serial_rt > gamepad->state.rt) gamepad->state.rt = serial_rt;
    
    // directional input
    bool up    = (current_buttons & (1UL << BIT_UP)) != 0;
    bool down  = (current_buttons & (1UL << BIT_DOWN)) != 0;
    bool right = (current_buttons & (1UL << BIT_RIGHT)) != 0;
    bool left  = (current_buttons & (1UL << BIT_LEFT)) != 0;
    
    // ===== directional input handling (per mode) =====
    switch (pad_mode) {
        case PAD_MODE_DPAD:
            // ✅ merge D-pad (no reset!)
            // gamepad->state.dpad = 0;  ← removed!
            if (up)    gamepad->state.dpad |= GAMEPAD_MASK_UP;
            if (down)  gamepad->state.dpad |= GAMEPAD_MASK_DOWN;
            if (left)  gamepad->state.dpad |= GAMEPAD_MASK_LEFT;
            if (right) gamepad->state.dpad |= GAMEPAD_MASK_RIGHT;
            
            // ✅ analog only when GPIO is neutral
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
            // ✅ merge D-pad
            // gamepad->state.dpad = 0;  ← removed!
            
            // ✅ left stick only when GPIO is neutral
            if (gamepad->state.lx == GAMEPAD_JOYSTICK_MID && 
                gamepad->state.ly == GAMEPAD_JOYSTICK_MID) {
                gamepad->state.lx = left ? GAMEPAD_JOYSTICK_MIN : 
                                   (right ? GAMEPAD_JOYSTICK_MAX : GAMEPAD_JOYSTICK_MID);
                gamepad->state.ly = up ? GAMEPAD_JOYSTICK_MIN : 
                                   (down ? GAMEPAD_JOYSTICK_MAX : GAMEPAD_JOYSTICK_MID);
            }
            
            // keep GPIO for right stick
            if (gamepad->state.rx == GAMEPAD_JOYSTICK_MID && 
                gamepad->state.ry == GAMEPAD_JOYSTICK_MID) {
                gamepad->state.rx = GAMEPAD_JOYSTICK_MID;
                gamepad->state.ry = GAMEPAD_JOYSTICK_MID;
            }
            break;
            
        case PAD_MODE_RANALOG:
            // ✅ merge D-pad
            // gamepad->state.dpad = 0;  ← removed!
            
            // keep GPIO for left stick
            if (gamepad->state.lx == GAMEPAD_JOYSTICK_MID && 
                gamepad->state.ly == GAMEPAD_JOYSTICK_MID) {
                gamepad->state.lx = GAMEPAD_JOYSTICK_MID;
                gamepad->state.ly = GAMEPAD_JOYSTICK_MID;
            }
            
            // ✅ right stick only when GPIO is neutral
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
// Reset Gamepad state
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
// Merge Gamepad state
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
// Receive serial data
// ========================================
void SerialInputAddon::receiveSerialData() {
    int bytes_processed = 0;
    
    while (uart_is_readable(SERIAL_UART) && bytes_processed < MAX_BYTES_PER_FRAME) {
        uint8_t byte = uart_getc(SERIAL_UART);
        bytes_processed++;
        
        // ===== header detection =====
        if (byte == BINARY_HEADER) {
            buffer_index = 0;
            message_buffer[buffer_index++] = byte;
        }
        // ===== receiving message =====
        else if (buffer_index > 0 && buffer_index < sizeof(CompactMessage)) {
            message_buffer[buffer_index++] = byte;
            
            // ===== message complete =====
            if (buffer_index == sizeof(CompactMessage)) {
                if (message_buffer[sizeof(CompactMessage) - 1] == BINARY_FOOTER) {
                    uint8_t calc_crc = calculateCRC8(message_buffer, sizeof(CompactMessage) - 2);
                    uint8_t msg_crc = message_buffer[sizeof(CompactMessage) - 2];
                    
                    if (calc_crc == msg_crc) {
                        CompactMessage msg;
                        memcpy(&msg, message_buffer, sizeof(CompactMessage));
                        
                        serial_state.p1_buttons = msg.p1_buttons;
                        serial_state.p2_buttons = msg.p2_buttons;
                        serial_state.input_mode = (msg.modes >> 4) & 0x0F; // upper 4 bits
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
// Convert Arduino mode to GP2040 mode
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
            return INPUT_MODE_XINPUT; // default for reboot safety
        case ARDUINO_MODE_DREAMCAST:
            return INPUT_MODE_XINPUT; // default for reboot safety
        default:                    
            return INPUT_MODE_XINPUT;
    }
}

// ========================================
// CRC8 checksum calculation
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
// Check for OLED presence (very safe version)
// ========================================
bool SerialInputAddon::checkOLEDAvailable() {
    // ★★★ 1. Ensure it runs only once (very important) ★★★
    static bool already_checked = false;
    static bool last_result = false;
    if (already_checked) {
        return last_result;
    }
    already_checked = true;

    #ifdef DEBUG_MODE
    debugPrint("Checking for OLED display (defensive method)...");
    #endif

    // ★★★ 2. Before I2C init, clean up the pin state first ★★★
    // Guards against a previous abnormal I2C shutdown.
    gpio_disable_pulls(OLED_I2C_SDA_PIN);
    gpio_disable_pulls(OLED_I2C_SCL_PIN);
    gpio_set_function(OLED_I2C_SDA_PIN, GPIO_FUNC_SIO);
    gpio_set_function(OLED_I2C_SCL_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(OLED_I2C_SDA_PIN, GPIO_IN);
    gpio_set_dir(OLED_I2C_SCL_PIN, GPIO_IN);

    // Start the I2C bus from a clean state
    i2c_init(OLED_I2C_PORT, 400 * 1000);
    gpio_set_function(OLED_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_I2C_SCL_PIN, GPIO_FUNC_I2C);
    
    // ★★★ 3. Enable internal pull-up resistors ★★★
    // Prevents floating bus lines when there are no external pull-ups.
    gpio_pull_up(OLED_I2C_SDA_PIN);
    gpio_pull_up(OLED_I2C_SCL_PIN);
    sleep_ms(10); // time for pull-ups to settle

    // ★★★ 4. Set a timeout measuring the whole probe ★★★
    uint32_t start_time = time_us_32();
    int ret;
    uint8_t dummy_data = 0; // dummy data to probe the address only

    // Attempt a write with a very short timeout (10ms).
    ret = i2c_write_timeout_us(OLED_I2C_PORT, OLED_I2C_ADDR, &dummy_data, 1, false, 10000);

    uint32_t elapsed_time = time_us_32() - start_time;

    // ★★★ 5. Double timeout check (core logic) ★★★
    // ret < 0: SDK function returned timeout/error
    // elapsed_time > 50000: the call itself stalled >50ms (hardware hang)
    if (ret < 0 || elapsed_time > 50000) {
        #ifdef DEBUG_MODE
        debugPrint("OLED not found. ret=%d, elapsed=%lu us", ret, elapsed_time);
        #endif

        // ★★★ 6. On failure fully disable the I2C hardware (most important) ★★★
        i2c_deinit(OLED_I2C_PORT);
        
        // Return the GPIO pins to input (High-Z) so they don't affect the system.
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

    // Success, so keep I2C as is.
    last_result = true;
    return true;
}

// ========================================
// Send OLED command (safe)
// ========================================
void SerialInputAddon::oled_send_cmd(uint8_t cmd) {
    if (!oled_available) return;  // return immediately if no OLED
    
    uint8_t buf[2] = {0x00, cmd};
    i2c_write_timeout_us(OLED_I2C_PORT, OLED_I2C_ADDR, buf, 2, false, 10000);
}

// ========================================
// Send OLED data (safe)
// ========================================
void SerialInputAddon::oled_send_data(uint8_t* data, size_t len) {
    if (!oled_available) return;  // return immediately if no OLED
    
    uint8_t buf[len + 1];
    buf[0] = 0x40; // Data mode
    memcpy(buf + 1, data, len);
    i2c_write_timeout_us(OLED_I2C_PORT, OLED_I2C_ADDR, buf, len + 1, false, 10000);
}

// ========================================
// Init OLED (safe)
// ========================================
bool SerialInputAddon::setupOLED() {
    if (!oled_available) {
        return false;
    }
    
    #ifdef DEBUG_MODE
    debugPrint("Initializing OLED...");
    #endif
    
    sleep_ms(50);  // ← shortened from 100ms to 50ms
    
    // OLED init sequence
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
// Clear OLED screen (safe)
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
// Draw OLED char (safe)
// ========================================
void SerialInputAddon::oled_draw_char(uint8_t x, uint8_t y, char c, uint8_t size) {
    if (!oled_available) return;
    
    int char_index = -1;
    
    // find glyph index
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
        char_index = 0; // space
    }
    
    // output glyph data
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
// Draw OLED string (safe)
// ========================================
void SerialInputAddon::oled_draw_string(uint8_t x, uint8_t y, const char* str, uint8_t size) {
    if (!oled_available) return;
    
    uint8_t cursor_x = x;
    while (*str) {
        oled_draw_char(cursor_x, y, *str++, size);
        cursor_x += 6 * size; // 5 px + 1 px gap
    }
}

// ========================================
// Convert current mode to string
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
// Convert pad mode to string
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
// check SWAP state
// ========================================
bool SerialInputAddon::isSwapped() {
    return (serial_state.channel_state & 0x01) != 0;
}

// ========================================
// Style 1: default display (safe)
// ========================================
void SerialInputAddon::updateDisplay() {
    if (!oled_available) return;  // return immediately if no OLED
    
    static uint8_t last_mode = 0xFF;
    static uint8_t last_modes = 0xFF;
    static uint8_t last_channel = 0xFF;
    static uint32_t last_update = 0;
    uint32_t now = time_us_32();
    
    // check for update every 50ms
    if (now - last_update < 50000) {
        return;
    }
    
    // update only when something changed
    if (serial_state.input_mode == last_mode && 
        serial_state.modes == last_modes &&
        serial_state.channel == last_channel) {
        return;
    }
    
    last_mode = serial_state.input_mode;
    last_modes = serial_state.modes;
    last_channel = serial_state.channel;
    last_update = now;
    
    // clear screen
    oled_clear();
    
    // ===== line 1: current mode (large) =====
    const char* mode_str = getModeString(serial_state.input_mode);
    char mode_line[32];
    sprintf(mode_line, "MODE:%s", mode_str);
    oled_draw_string(5, 0, mode_line, 2);
    
    // ===== line 2: pad-mode state =====
    const char* p1_pad = getPadModeString(serial_state.modes, true);
    const char* p2_pad = getPadModeString(serial_state.modes, false);
    
    char pad_line[32];
    sprintf(pad_line, "1P:%s 2P:%s", p1_pad, p2_pad);
    oled_draw_string(5, 24, pad_line, 1);
    
    // ===== line 3: SWAP state =====
    bool swapped = isSwapped();
    const char* swap_str = swapped ? "YES" : "NO";
    char swap_line[32];
    sprintf(swap_line, "SWAP:%s", swap_str);
    oled_draw_string(5, 40, swap_line, 1);
    
    // ===== line 4: connection state =====
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
// Style 2: compact display (safe)
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
// Style 3: player-centric display (safe)
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
// Style 4: icon style (safe)
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