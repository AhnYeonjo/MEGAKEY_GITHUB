// headers/addons/serial_input.h
// GP2040-CE Serial Input Addon Header
// Version: 2.4 - SerialInputAddon uses both OLED and WS2812 exclusively
//
// Notes:
// 1. Display Addon must be disabled! (prevents OLED I2C conflict)
// 2. PlayerLEDAddon must be disabled! (prevents WS2812 PIO conflict)

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
    #define PLAYER_NUMBER 1 // default is 1P
#endif

// ========== OLED display ==========
// ✅ SerialInputAddon uses the OLED exclusively
#define USE_OLED_DISPLAY

// ========== UART config ==========
#define SERIAL_UART uart1
#define SERIAL_RX_PIN 5
#define SERIAL_BAUDRATE 250000

#ifdef RP2040_ZERO
    // ✅ SerialInputAddon uses WS2812 exclusively
    #define USE_WS2812_LED
    #define WS2812_PIN 16
    #define WS2812_IS_RGBW false
#else
    // Pico's onboard LED
    #define STATUS_LED_PIN 25
#endif



// ========================================
// Include I2C and OLED libraries
// ========================================
#include "hardware/i2c.h"
#include "pico/binary_info.h"

// SSD1306 config
#define OLED_I2C_PORT i2c0
#define OLED_I2C_SDA_PIN 0
#define OLED_I2C_SCL_PIN 1
#define OLED_I2C_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// ========================================
// SSD1306 OLED command definitions
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


// GPIO pin definitions for Dreamcast output (adjust to your setup)
#define DREAMCAST_SENSE_PIN   12
#define DREAMCAST_MAPLE_A_PIN 10 // Maple A/B must be consecutive pins
// The Maple B pin automatically uses the next pin after A (GP11).
#define DREAMCAST_PIO_ENABLE_PIN 21
// ★★★ Communication timeout (100ms) ★★★
#define DREAMCAST_RX_TIMEOUT_US 100000

// ========== Debug mode ==========
// #define DEBUG_MODE  // uncomment to enable debug

// ========== Constant definitions ==========

// Pad mode definitions
#define PAD_MODE_DPAD    0
#define PAD_MODE_LANALOG 1
#define PAD_MODE_RANALOG 2

// Arduino mode definitions
#define ARDUINO_MODE_KEYBOARD 0
#define ARDUINO_MODE_XINPUT   1
#define ARDUINO_MODE_SWITCH   2
#define ARDUINO_MODE_PS3      3
#define ARDUINO_MODE_PS4      4
#define ARDUINO_MODE_PS5      5
#define ARDUINO_MODE_XBOX     6
#define ARDUINO_MODE_JVS      7
#define ARDUINO_MODE_DREAMCAST 8

// Button bit definitions (same as Arduino)
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

// Communication timing
#define CONNECTION_TIMEOUT_US 500000
#define MODE_CHANGE_DELAY_US 100000
#define MAX_BYTES_PER_FRAME 64

// Message header/footer
#define BINARY_HEADER 0xAA
#define BINARY_FOOTER 0x55

#if PLAYER_NUMBER == 1
    // RP2040 GPIO pin number connected to the TXS0108E OE pin in the schematic
    #define LEVEL_SHIFTER_ENABLE_PIN 28
#endif

// ========== Message struct ==========
struct __attribute__((packed)) CompactMessage {
    uint8_t header;
    uint32_t p1_buttons;
    uint32_t p2_buttons;
    uint8_t modes;
    uint8_t channel_state;
    uint8_t crc;
    uint8_t footer;
};

// ========== SerialInputAddon class ==========
class SerialInputAddon : public GPAddon {
public:
    virtual bool available();
    virtual void setup();
    virtual void preprocess();
    virtual void process();
    
    // Required GPAddon virtual functions
    virtual void postprocess(bool) {}
    virtual std::string name() { return "SerialInput"; }
    virtual void reinit() { 
        addon_enabled = true;
        connection_timeout = true;
        buffer_index = 0;
        memset(&serial_state, 0, sizeof(serial_state));
    }
    
private:
    // Serial state struct
    struct __attribute__((packed)) SerialState {
        uint32_t p1_buttons;
        uint32_t p2_buttons;
        uint8_t input_mode;
        uint8_t modes;
        uint8_t channel;
        uint8_t channel_state;
    } serial_state;
    
    // Communication buffer
    uint8_t message_buffer[sizeof(CompactMessage)];
    uint8_t buffer_index;
    
    // Connection state
    uint32_t last_receive_time;
    bool connection_timeout;
    bool addon_enabled;
    bool was_timeout;
    
    // Mode-change related
    bool mode_change_pending;
    uint32_t mode_change_time;
    
    // Gamepad state backup
    GamepadState previous_state;
    
    // Keyboard mapping init flag
    bool keyboard_mapping_initialized = false;
    
    // Error statistics
    struct {
        uint32_t crc_errors;
        uint32_t invalid_headers;
        uint32_t invalid_footers;
        uint32_t buffer_overflows;
    } error_stats;
    
    // State variables used by updateStatusLED
    uint32_t led_state_last_time;
    uint32_t led_mode_change_time;
    bool led_blink_state;
    uint8_t led_last_mode;

    // Variable tracking the init-animation state
    enum LEDInitState {
        NOT_STARTED,
        ANIMATING,
        FINISHED
    };
    LEDInitState led_init_state;
    
    // Methods
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

    // Declare a NeoPico object instead of PIO-related variables.
    NeoPico neoPico;
    #endif

    #ifdef USE_OLED_DISPLAY
    bool oled_available;  // whether the OLED is available
        
    // ===== OLED safety-check function (newly added!) =====
    bool checkOLEDAvailable();     
    // ===== OLED base functions =====
    bool setupOLED();
    void oled_send_cmd(uint8_t cmd);
    void oled_send_data(uint8_t* data, size_t len);
    void oled_clear();
    void oled_draw_char(uint8_t x, uint8_t y, char c, uint8_t size);
    void oled_draw_string(uint8_t x, uint8_t y, const char* str, uint8_t size);
    
    // ===== improved display functions =====
    const char* getModeString(uint8_t mode);
    const char* getPadModeString(uint8_t modes, bool is_player1);  // ← newly added
    bool isSwapped();                                               // ← newly added
    
    // ===== 4 display styles =====
    void updateDisplay();           // default - show all info (recommended)
    void updateDisplayCompact();    // compact - space-saving
    void updateDisplayPlayer();     // player-centric - emphasize own info
    void updateDisplayIcon();       // icon - concise and simple
    #endif
    
    // ★★★ Dreamcast-related member variables and function declarations (tidied & added) ★★★
    
    // PIO hardware resources
    PIO dreamcast_pio;
    uint dreamcast_sm;
    uint dreamcast_offset;
    uint32_t last_dreamcast_rx_time;
    bool dreamcast_pio_active;

    // Maple Bus communication state
    enum MapleBusState { LISTENING, RESPONDING };
    MapleBusState maple_state;

    // PIO init and control functions
    void setupDreamcastPIO();
    void startMapleListen(); // Function that starts the PIO in receive mode

    // Communication handling functions
    void processDreamcastOutput(const GamepadState& state);
    void sendDreamcastReport(const uint8_t* report_data, size_t len);

    #ifdef DEBUG_MODE
    void debugPrint(const char* format, ...);
    void printErrorStats();
    void printKeyboardMapping();
    #endif
};

#endif