// MegaKey PCB - RP2040 Zero Control Version with KVM
// Version: 3.4 - Fixed Timer Conflict (JVS vs IR)
// Hardware: Arduino Mega 2560 PRO

//    [IMPORTANT] Force IRremote to use Timer 3 (prevents conflict with JVS Timer 2)
// This definition must be declared before including <IRremote.h>.
#define IR_USE_AVR_TIMER3 

#include "jvsio/JVSIO.h"
#include "jvsio/JVSIO.cpp"
#include "jvsio/clients/MegaClient.h"
#include <IRremote.h> 

#pragma GCC optimize("O3")

// ========== SYSTEM CONSTANTS ==========
#define FIRMWARE_VERSION "3.4-TIMER_FIX"
#define SERIAL_SPEED 250000
#define LOOP_INTERVAL_US 1000UL  // 1ms (1000Hz)
#define COIN_DEBOUNCE_MS 50

// ========== OPERATION MODES ==========
#define MODE_KEYBOARD    0
#define MODE_XINPUT      1
#define MODE_SWITCH      2
#define MODE_PS3         3
#define MODE_PS4         4
#define MODE_PS5         5
#define MODE_XBOX        6
#define MODE_JVS         7
#define MODE_DREAMCAST   8

// ========== KVM CHANNELS ==========
#define KVM_CH1          1
#define KVM_CH2          2
#define KVM_CH3          3
#define KVM_CH4          4
#define KVM_CH5_JAMMA    5

// ========== BUTTON BIT POSITIONS ==========
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
#define BIT_L3      15
#define BIT_R3      16
#define BIT_TURBO   17
#define BIT_TOUCH   18

// ========== COMMUNICATION PROTOCOL ==========
#define BINARY_HEADER    0xAA
#define BINARY_FOOTER    0x55

// ========== HARDWARE PIN DEFINITIONS ==========

// === JVS INTERFACE ===
#define JVS_IO_PIN       41     // D41

// === IR LED CONTROL PIN ===
#define IR_LED_PIN       3      // D3 (pin using Timer 3)

// === USB MULTIPLEXER ===
#define MUX1_SEL_PIN     A10    
#define MUX1_OE_PIN      A12    
#define MUX2_SEL_PIN     A14    
#define MUX2_OE_PIN      A15    

// === KVM STATUS LEDs ===
#define KVM1_LED_PIN     5      
#define KVM2_LED_PIN     7      
#define KVM3_LED_PIN     11     
#define KVM4_LED_PIN     15     
#define KVM5_LED_PIN     17     
#define STATUS_LED_PIN   13     

// === RP2040 COMMUNICATION ===
#define RP2040_1P_TX_PIN 16     
#define RP2040_2P_TX_PIN 14     

// === KVM CHANNEL SELECT BUTTONS ===
#define CHK_KVM1_PIN     2      
#define CHK_KVM2_PIN     4      
#define CHK_KVM3_PIN     6      
#define CHK_KVM4_PIN     8      
#define CHK_KVM5_PIN     10     

// === CONFIG SWITCHES ===
#define CHK_JOYSTICK_PIN 39     
#define CHK_JVS_PIN      41     
#define CHK_SWAPSTICK_PIN A8    

// === Player 1 BUTTONS ===
#define P1_START_PIN     21     
#define P1_SELECT_PIN    23     
#define P1_HOME_PIN      25     
#define P1_UP_PIN        43     
#define P1_DOWN_PIN      45     
#define P1_LEFT_PIN      47     
#define P1_RIGHT_PIN     49     
#define P1_A_PIN         A1     
#define P1_B_PIN         A3     
#define P1_X_PIN         51     
#define P1_Y_PIN         53     
#define P1_LB_PIN        A9    
#define P1_RB_PIN        A7     
#define P1_LT_PIN        A13    
#define P1_RT_PIN        A11    
#define P1_TURBO_PIN     27     
#define P1_TOUCHPAD_PIN  29     
#define P1_LANAL_PIN     35     
#define P1_RANAL_PIN     37     

// === Player 2 BUTTONS ===
#define P2_START_PIN     44     
#define P2_SELECT_PIN    46     
#define P2_HOME_PIN      48     
#define P2_UP_PIN        20     
#define P2_DOWN_PIN      22     
#define P2_LEFT_PIN      24     
#define P2_RIGHT_PIN     26     
#define P2_A_PIN         32     
#define P2_B_PIN         34     
#define P2_X_PIN         28     
#define P2_Y_PIN         30     
#define P2_LB_PIN        40     
#define P2_RB_PIN        36     
#define P2_LT_PIN        42     
#define P2_RT_PIN        38     
#define P2_TURBO_PIN     50     
#define P2_TOUCHPAD_PIN  52     
#define P2_LANAL_PIN     A4     
#define P2_RANAL_PIN     A6     

// ========== JVS CLASSES (uses Timer 2) ==========
class MegaJVSDataClient : public JVSIO::DataClient {
  int available() override { return Serial.available(); }
  void setMode(int mode) override {
    if (mode == INPUT) {
      pinMode(0, INPUT);
      pinMode(1, INPUT);
      Serial.begin(115200);
    } else {
      digitalWrite(0, HIGH);
      digitalWrite(1, LOW);
      pinMode(0, OUTPUT);
      pinMode(1, OUTPUT);
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

class MegaJVSSenseClient : public JVSIO::SenseClient {
  void begin() override {
    // Timer 2 setup (for JVS Sense signal) - this was the part that conflicted with IRremote
    TCCR2A = 0b00010010;
    OCR2A = 1;
    TCCR2B = 0b00000001;
    pinMode(9, OUTPUT);
    digitalWrite(9, LOW);
  }
  void set(bool ready) override {
    if (ready) TCCR2A &= ~0b00010000;
    else TCCR2A |= 0b00010000;
  }
};

class MegaJVSLedClient : public JVSIO::LedClient {
  void begin() override { pinMode(STATUS_LED_PIN, OUTPUT); }
  void set(bool ready) override { digitalWrite(STATUS_LED_PIN, ready ? HIGH : LOW); }
};

MegaJVSDataClient jvs_data;
MegaJVSSenseClient jvs_sense;
MegaJVSLedClient jvs_led;
JVSIO io(&jvs_data, &jvs_sense, &jvs_led);

// ========== DATA STRUCTS ==========
struct ButtonMapping {
  uint8_t pin;
  uint8_t bit_index;
};

const ButtonMapping p1_buttons[] PROGMEM = {
  {P1_SELECT_PIN, BIT_SELECT}, {P1_HOME_PIN, BIT_HOME}, {P1_START_PIN, BIT_START},
  {P1_UP_PIN, BIT_UP}, {P1_DOWN_PIN, BIT_DOWN}, {P1_RIGHT_PIN, BIT_RIGHT}, {P1_LEFT_PIN, BIT_LEFT},
  {P1_A_PIN, BIT_A}, {P1_B_PIN, BIT_B}, {P1_X_PIN, BIT_X}, {P1_Y_PIN, BIT_Y},
  {P1_LB_PIN, BIT_LB}, {P1_RB_PIN, BIT_RB}, {P1_LT_PIN, BIT_LT}, {P1_RT_PIN, BIT_RT},
  {P1_TURBO_PIN, BIT_TURBO}, {P1_TOUCHPAD_PIN, BIT_TOUCH}
};

const ButtonMapping p2_buttons[] PROGMEM = {
  {P2_SELECT_PIN, BIT_SELECT}, {P2_HOME_PIN, BIT_HOME}, {P2_START_PIN, BIT_START},
  {P2_UP_PIN, BIT_UP}, {P2_DOWN_PIN, BIT_DOWN}, {P2_RIGHT_PIN, BIT_RIGHT}, {P2_LEFT_PIN, BIT_LEFT},
  {P2_A_PIN, BIT_A}, {P2_B_PIN, BIT_B}, {P2_X_PIN, BIT_X}, {P2_Y_PIN, BIT_Y},
  {P2_LB_PIN, BIT_LB}, {P2_RB_PIN, BIT_RB}, {P2_LT_PIN, BIT_LT}, {P2_RT_PIN, BIT_RT},
  {P2_TURBO_PIN, BIT_TURBO}, {P2_TOUCHPAD_PIN, BIT_TOUCH}
};

struct __attribute__((packed)) CompactMessage {
  uint8_t header;
  uint32_t p1_buttons;
  uint32_t p2_buttons;
  uint8_t modes;
  uint8_t channel_state;
  uint8_t crc;
  uint8_t footer;
};

struct SystemState {
  uint32_t p1_buttons;
  uint32_t p2_buttons;
  
  uint8_t selected_mode;
  uint8_t kvm_channel;
  bool mode_locked;
  bool chk_joystick_prev;
  bool jvs_mode_active;
  bool swap_stick;
  
  struct {
    bool p1_lanl : 1; bool p1_ranl : 1;
    bool p2_lanl : 1; bool p2_ranl : 1;
  } analog_checks;
  
  unsigned long frame_count;
} system_state;

const char jvs_system_id[] PROGMEM = "SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551 ;Ver1.00;Enhanced";

bool kvm_button_prev_state[5] = {HIGH, HIGH, HIGH, HIGH, HIGH}; 
const uint8_t kvm_pins[] = {CHK_KVM1_PIN, CHK_KVM2_PIN, CHK_KVM3_PIN, CHK_KVM4_PIN, CHK_KVM5_PIN};

uint16_t coin_count[2];
bool coin_prev_state[2];
unsigned long coin_time[2];

// ========== FUNCTION DECLARATIONS ==========
void sendNECCommand(uint32_t command);
void sendHDMISwitchCommand(uint8_t channel);

// ========== CRC8 COMPUTATION ==========
uint8_t calculateCRC8(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0x00;
  while (len--) {
    uint8_t extract = *data++;
    for (uint8_t i = 8; i; i--) {
      uint8_t sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum) crc ^= 0x8C;
      extract >>= 1;
    }
  }
  return crc;
}

// ========== DEBUG LED FLASH ==========
void debugFlash(uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(50);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(50);
  }
}

// ========== KVM CHANNEL UPDATE ==========
void updateKVMChannel(uint8_t channel) {
  // USB MUX control
  switch (channel) {
    case KVM_CH1:
      digitalWrite(MUX1_SEL_PIN, LOW); digitalWrite(MUX1_OE_PIN, LOW); digitalWrite(MUX2_OE_PIN, HIGH); break;
    case KVM_CH2:
      digitalWrite(MUX1_SEL_PIN, HIGH); digitalWrite(MUX1_OE_PIN, LOW); digitalWrite(MUX2_OE_PIN, HIGH); break;
    case KVM_CH3:
      digitalWrite(MUX2_SEL_PIN, LOW); digitalWrite(MUX1_OE_PIN, HIGH); digitalWrite(MUX2_OE_PIN, LOW); break;
    case KVM_CH4:
      digitalWrite(MUX2_SEL_PIN, HIGH); digitalWrite(MUX1_OE_PIN, HIGH); digitalWrite(MUX2_OE_PIN, LOW); break;
    case KVM_CH5_JAMMA:
      digitalWrite(MUX1_OE_PIN, HIGH); digitalWrite(MUX2_OE_PIN, HIGH); break;
  }
  
  // HDMI IR transmit
  sendHDMISwitchCommand(channel);
}

// ========== IR TRANSMIT FUNCTION ==========
void sendHDMISwitchCommand(uint8_t channel) {
  uint32_t nec_command = 0;
  switch (channel) {
    case KVM_CH1: nec_command = 0xF807FF00; break;
    case KVM_CH2: nec_command = 0xF609FF00; break;
    case KVM_CH3: nec_command = 0xE916FF00; break;
    case KVM_CH4: nec_command = 0xF20DFF00; break;
    case KVM_CH5_JAMMA: nec_command = 0xE718FF00; break;
  }
  
  if (nec_command != 0) {
    sendNECCommand(nec_command);
  }
}

void sendNECCommand(uint32_t command) {
  // Transmit code for IRremote v3/v4 (uses Timer 3)
  // sendNECRaw takes the form (RawData, RepeatCount).
  IrSender.sendNECRaw(command, 0);
}

// ========== MODE CHECK & INPUT HANDLING ==========
void checkInitialMode() {
  bool jvs_low = (digitalRead(CHK_JVS_PIN) == LOW);
  bool joystick_low = (digitalRead(CHK_JOYSTICK_PIN) == LOW);
  
  // 1. Both ON -> keyboard (for maintenance)
  if (jvs_low && joystick_low) {
    system_state.selected_mode = MODE_KEYBOARD;
    system_state.jvs_mode_active = false;
    system_state.mode_locked = true;
  } 
  // 2. JVS only ON -> JVS mode
  else if (jvs_low) {
    system_state.selected_mode = MODE_JVS;
    system_state.jvs_mode_active = true;
    system_state.mode_locked = true;
  }
  // 3. [ADDED] Joystick only ON -> joystick mode (XInput, etc.)
  else if (joystick_low) {
    // Determine mode by checking held buttons at boot (default XInput)
    system_state.p1_buttons = scanPlayerButtons(p1_buttons); // read buttons once
    
    // Mode-selection logic based on button combination (reuses existing checkModeSelection logic)
    if (system_state.p1_buttons & (1 << BIT_X)) system_state.selected_mode = MODE_DREAMCAST;
    else if (system_state.p1_buttons & (1 << BIT_Y)) system_state.selected_mode = MODE_PS3;
    else if (system_state.p1_buttons & (1 << BIT_A)) system_state.selected_mode = MODE_PS4;
    else if (system_state.p1_buttons & (1 << BIT_B)) system_state.selected_mode = MODE_SWITCH;
    else if (system_state.p1_buttons & (1 << BIT_LB)) system_state.selected_mode = MODE_KEYBOARD;
    else if (system_state.p1_buttons & (1 << BIT_RB)) system_state.selected_mode = MODE_PS5;
    else if (system_state.p1_buttons & (1 << BIT_LT)) system_state.selected_mode = MODE_XBOX;
    else system_state.selected_mode = MODE_XINPUT; // default to XInput if no button pressed

    system_state.jvs_mode_active = false;
    system_state.mode_locked = true; // ★ IMPORTANT: mode confirmed (LED stays ON)
  }
  // 4. Both OFF -> keyboard mode (default state)
  else {
    system_state.selected_mode = MODE_KEYBOARD; // set to keyboard as desired
    system_state.jvs_mode_active = false;
    system_state.mode_locked = false; // standby state (fast blink)
  }
}

void checkModeSelection() {
  if (system_state.mode_locked) return;
  
  bool joystick_current = digitalRead(CHK_JOYSTICK_PIN);
  if (system_state.chk_joystick_prev == HIGH && joystick_current == LOW) {
    // mode-change logic (button combination)
    uint16_t p1 = system_state.p1_buttons;
    if (p1 & (1 << BIT_X)) system_state.selected_mode = MODE_DREAMCAST;
    else if (p1 & (1 << BIT_Y)) system_state.selected_mode = MODE_PS3;
    else if (p1 & (1 << BIT_A)) system_state.selected_mode = MODE_PS4;
    else if (p1 & (1 << BIT_B)) system_state.selected_mode = MODE_SWITCH;
    else if (p1 & (1 << BIT_LB)) system_state.selected_mode = MODE_KEYBOARD;
    else if (p1 & (1 << BIT_RB)) system_state.selected_mode = MODE_PS5;
    else if (p1 & (1 << BIT_LT)) system_state.selected_mode = MODE_XBOX;
    else system_state.selected_mode = MODE_XINPUT;
    
    system_state.mode_locked = true;
    debugFlash(system_state.selected_mode + 1);
  }
  system_state.chk_joystick_prev = joystick_current;
}

uint32_t scanPlayerButtons(const ButtonMapping* button_map) {
  uint32_t button_state = 0;
  for (uint8_t i = 0; i < 17; i++) {
    ButtonMapping btn;
    memcpy_P(&btn, &button_map[i], sizeof(ButtonMapping));
    if (digitalRead(btn.pin) == LOW) {
      button_state |= (1UL << btn.bit_index);
    }
  }
  return button_state;
}

void scanAllInputs() {
  system_state.p1_buttons = scanPlayerButtons(p1_buttons);
  system_state.p2_buttons = scanPlayerButtons(p2_buttons);
  
  system_state.swap_stick = (digitalRead(CHK_SWAPSTICK_PIN) == LOW);
  system_state.analog_checks.p1_lanl = (digitalRead(P1_LANAL_PIN) == LOW);
  system_state.analog_checks.p1_ranl = (digitalRead(P1_RANAL_PIN) == LOW);
  system_state.analog_checks.p2_lanl = (digitalRead(P2_LANAL_PIN) == LOW);
  system_state.analog_checks.p2_ranl = (digitalRead(P2_RANAL_PIN) == LOW);
  
  // KVM button scan (Falling Edge Detection)
  for (uint8_t i = 0; i < 5; i++) {
    bool currentState = digitalRead(kvm_pins[i]);


    if (kvm_button_prev_state[i] == HIGH && currentState == LOW) {
      uint8_t new_channel = i + 1;
      if (new_channel != system_state.kvm_channel) {
        system_state.kvm_channel = new_channel;
        updateKVMChannel(new_channel);
      }
      kvm_button_prev_state[i] = currentState;
      break; 
    }

    kvm_button_prev_state[i] = currentState;
  }
  
  // coin handling
  unsigned long current_time = millis();
  bool p1_coin = (system_state.p1_buttons & (1 << BIT_SELECT)) != 0;
  if (p1_coin && !coin_prev_state[0]) {
    if (current_time - coin_time[0] > COIN_DEBOUNCE_MS) {
      coin_count[0] = min(coin_count[0] + 1, 9999);
      coin_time[0] = current_time;
    }
  }
  coin_prev_state[0] = p1_coin;
  
  bool p2_coin = (system_state.p2_buttons & (1 << BIT_SELECT)) != 0;
  if (p2_coin && !coin_prev_state[1]) {
    if (current_time - coin_time[1] > COIN_DEBOUNCE_MS) {
      coin_count[1] = min(coin_count[1] + 1, 9999);
      coin_time[1] = current_time;
    }
  }
  coin_prev_state[1] = p2_coin;
}

void updateStatusLED() {
  static unsigned long last_blink = 0;
  static bool led_state = false;
  unsigned long current_time = millis();
  
  if (system_state.jvs_mode_active) {
    if (current_time - last_blink > 500) {
      led_state = !led_state;
      digitalWrite(STATUS_LED_PIN, led_state);
      last_blink = current_time;
    }
  } else if (!system_state.mode_locked) {
    if (current_time - last_blink > 100) {
      led_state = !led_state;
      digitalWrite(STATUS_LED_PIN, led_state);
      last_blink = current_time;
    }
  } else {
    digitalWrite(STATUS_LED_PIN, HIGH);
  }
}

void sendToRP2040() {
  CompactMessage msg;
  
  // ★ Extended to uint32_t: can transmit up to BIT_L3(15), BIT_R3(16), BIT_TURBO(17), BIT_TOUCH(18)
  uint32_t p1_data = system_state.swap_stick ? system_state.p2_buttons : system_state.p1_buttons;
  uint32_t p2_data = system_state.swap_stick ? system_state.p1_buttons : system_state.p2_buttons;
  
  msg.header = BINARY_HEADER;
  msg.p1_buttons = p1_data;
  msg.p2_buttons = p2_data;
  
  uint8_t mode_bits = (system_state.selected_mode & 0x0F) << 4;
  uint8_t analog_bits = 0;
  if (system_state.analog_checks.p1_lanl) analog_bits |= (1 << 0);
  if (system_state.analog_checks.p1_ranl) analog_bits |= (1 << 1);
  if (system_state.analog_checks.p2_lanl) analog_bits |= (1 << 2);
  if (system_state.analog_checks.p2_ranl) analog_bits |= (1 << 3);
  
  msg.modes = mode_bits | analog_bits;
  uint8_t channel_bits = (system_state.kvm_channel & 0x0F) << 4;
  uint8_t swap_bit = system_state.swap_stick ? (1 << 0) : 0;
  msg.channel_state = channel_bits | swap_bit;
  
  msg.crc = calculateCRC8((uint8_t*)&msg, sizeof(msg) - 2);
  msg.footer = BINARY_FOOTER;
  
  Serial2.write((uint8_t*)&msg, sizeof(CompactMessage));
  Serial3.write((uint8_t*)&msg, sizeof(CompactMessage));
}

void generateJVSSwitchData(uint8_t players, uint8_t bytes_per_player) {
  io.pushReport(JVSIO::kReportOk);
  
  uint16_t p1_state = system_state.swap_stick ? system_state.p2_buttons : system_state.p1_buttons;
  uint16_t p2_state = system_state.swap_stick ? system_state.p1_buttons : system_state.p2_buttons;
  
  uint8_t system_byte = 0;
  if (p1_state & (1UL << BIT_HOME)) system_byte |= 0x80;
  if (p1_state & (1UL << BIT_START)) system_byte |= 0x01;
  if (p2_state & (1UL << BIT_START)) system_byte |= 0x02;
  io.pushReport(system_byte);
  
  for (uint8_t player = 0; player < players && player < 2; player++) {
    uint16_t buttons = (player == 0) ? p1_state : p2_state;
    uint8_t byte1 = 0;
    if (p2_state & (1UL << BIT_HOME)) byte1 |= 0x40;
    if (buttons & (1UL << BIT_UP))    byte1 |= 0x20;
    if (buttons & (1UL << BIT_DOWN))  byte1 |= 0x10;
    if (buttons & (1UL << BIT_LEFT))  byte1 |= 0x08;
    if (buttons & (1UL << BIT_RIGHT)) byte1 |= 0x04;
    if (buttons & (1UL << BIT_A))     byte1 |= 0x02;
    if (buttons & (1UL << BIT_B))     byte1 |= 0x01;
    io.pushReport(byte1);
    
    if (bytes_per_player >= 2) {
      uint8_t byte2 = 0;
      if (buttons & (1UL << BIT_X))  byte2 |= 0x80;
      if (buttons & (1UL << BIT_Y))  byte2 |= 0x40;
      if (buttons & (1UL << BIT_LB)) byte2 |= 0x20;
      if (buttons & (1UL << BIT_RB)) byte2 |= 0x10;
      if (buttons & (1UL << BIT_LT)) byte2 |= 0x08;
      if (buttons & (1UL << BIT_RT)) byte2 |= 0x04;
      io.pushReport(byte2);
    }
  }
}

void processJVSCommands() {
  uint8_t len;
  uint8_t* data = io.getNextCommand(&len);
  if (!data) return;
  
  switch (*data) {
    case JVSIO::kCmdIoId:
      io.pushReport(JVSIO::kReportOk);
      for (size_t i = 0; i < strlen_P(jvs_system_id); ++i) io.pushReport(pgm_read_byte(&jvs_system_id[i]));
      io.pushReport(0);
      break;
    case JVSIO::kCmdFunctionCheck:
      io.pushReport(JVSIO::kReportOk);
      io.pushReport(0x01); io.pushReport(0x02); io.pushReport(0x0E); io.pushReport(0x00);
      io.pushReport(0x02); io.pushReport(0x02); io.pushReport(0x00); io.pushReport(0x00);
      io.pushReport(0x03); io.pushReport(0x08); io.pushReport(0x10); io.pushReport(0x00);
      io.pushReport(0x00);
      break;
    case JVSIO::kCmdSwInput:
      generateJVSSwitchData(data[1], data[2]);
      break;
    case JVSIO::kCmdCoinInput:
      {
        uint8_t slots = data[1];
        io.pushReport(JVSIO::kReportOk);
        for (uint8_t s = 0; s < slots && s < 2; s++) {
          io.pushReport((coin_count[s] >> 8) & 0xFF);
          io.pushReport(coin_count[s] & 0xFF);
        }
      }
      break;
    case JVSIO::kCmdAnalogInput:
      {
        uint8_t channels = data[1];
        io.pushReport(JVSIO::kReportOk);
        for (uint8_t ch = 0; ch < channels && ch < 8; ch++) {
          io.pushReport(0x80); io.pushReport(0x00);
        }
      }
      break;
    default:
      io.pushReport(JVSIO::kReportOk);
      break;
  }
}

// ========== INITIALIZATION ==========
void setup() {
  // 1. Fully disable SPI (more forcefully)
  SPCR = 0;                     // turn off SPI Control Register
  SPSR = 0;                     // reset SPI Status Register
  PRR0 |= (1 << PRSPI);         // completely cut SPI power
  
  // 2. Explicitly set SPI pins as GPIO
  // pins 50(PB3), 51(PB2), 52(PB1), 53(PB0)
  DDRB &= ~0x0F;                // set PB0-PB3 as inputs
  PORTB |= 0x0F;                // enable internal pull-ups
  
  // 3. Configure pin 48(PL1) separately
  DDRL &= ~(1 << PL1);          // set PL1 as input
  PORTL |= (1 << PL1);          // enable PL1 pull-up
  
  // JVSIO init (includes Timer 2 setup)
  delay(10);
  io.begin();
  
  // serial communication
  Serial2.begin(SERIAL_SPEED);
  Serial3.begin(SERIAL_SPEED);
  
  UCSR2B &= ~(1 << RXEN2);  // disable Serial2 RX -> frees pin 17 (KVM5 LED)
  UCSR3B &= ~(1 << RXEN3);  // disable Serial3 RX -> frees pin 15 (KVM4 LED)

  // pin mode setup
  pinMode(CHK_JOYSTICK_PIN, INPUT_PULLUP);
  pinMode(CHK_JVS_PIN, INPUT_PULLUP);
  pinMode(CHK_SWAPSTICK_PIN, INPUT_PULLUP);
  
  pinMode(CHK_KVM1_PIN, INPUT_PULLUP);
  pinMode(CHK_KVM2_PIN, INPUT_PULLUP);
  pinMode(CHK_KVM3_PIN, INPUT_PULLUP);
  pinMode(CHK_KVM4_PIN, INPUT_PULLUP);
  pinMode(CHK_KVM5_PIN, INPUT_PULLUP);
  
  pinMode(KVM1_LED_PIN, OUTPUT);
  pinMode(KVM2_LED_PIN, OUTPUT);
  pinMode(KVM3_LED_PIN, OUTPUT);
  pinMode(KVM4_LED_PIN, OUTPUT);
  pinMode(KVM5_LED_PIN, OUTPUT);
  pinMode(STATUS_LED_PIN, OUTPUT);
  
  pinMode(MUX1_SEL_PIN, OUTPUT);
  pinMode(MUX1_OE_PIN, OUTPUT);
  pinMode(MUX2_SEL_PIN, OUTPUT);
  pinMode(MUX2_OE_PIN, OUTPUT);
  
  digitalWrite(MUX1_OE_PIN, HIGH);
  digitalWrite(MUX2_OE_PIN, HIGH);

  // IR init (D3 = uses Timer 3)
  // [IMPORTANT] Thanks to IR_USE_AVR_TIMER3, Timer 2 (JVS) is not touched
  IrSender.begin(IR_LED_PIN, ENABLE_LED_FEEDBACK);
  
  TCCR3A &= ~((1 << COM3A1) | (1 << COM3A0) | (1 << COM3B1) | (1 << COM3B0));
  
  // button pin init
  for (uint8_t i = 0; i < 17; i++) {
    ButtonMapping btn;
    memcpy_P(&btn, &p1_buttons[i], sizeof(ButtonMapping));
    pinMode(btn.pin, INPUT_PULLUP);
    memcpy_P(&btn, &p2_buttons[i], sizeof(ButtonMapping));
    pinMode(btn.pin, INPUT_PULLUP);
  }

  pinMode(JVS_IO_PIN, INPUT_PULLUP);
  
  // state init
  memset(&system_state, 0, sizeof(system_state));
  system_state.chk_joystick_prev = HIGH;
  system_state.kvm_channel = KVM_CH1;
  
  memset(coin_count, 0, sizeof(coin_count));
  memset(coin_prev_state, 0, sizeof(coin_prev_state));
  memset(coin_time, 0, sizeof(coin_time));
  
  delay(100);
  checkInitialMode();
  updateKVMChannel(system_state.kvm_channel);
  
  // init-complete indication
  debugFlash(3);
}

void updateKVMLEDs() {
  TCCR3A &= ~((1 << COM3A1) | (1 << COM3A0));

  uint8_t ch = system_state.kvm_channel;
  
  // Set pin mode definitively every time to prevent timer interference (safety measure)
  pinMode(KVM1_LED_PIN, OUTPUT);
  
  digitalWrite(KVM1_LED_PIN, (ch == KVM_CH1) ? HIGH : LOW);
  digitalWrite(KVM2_LED_PIN, (ch == KVM_CH2) ? HIGH : LOW);
  digitalWrite(KVM3_LED_PIN, (ch == KVM_CH3) ? HIGH : LOW);
  digitalWrite(KVM4_LED_PIN, (ch == KVM_CH4) ? HIGH : LOW);
  digitalWrite(KVM5_LED_PIN, (ch == KVM_CH5_JAMMA) ? HIGH : LOW);
}

// ========== MAIN LOOP ==========
void loop() {
  updateKVMLEDs();

  // 1ms loop-interval control
  static unsigned long next_time_us = 0;
  unsigned long current_us = micros();
  if (current_us < next_time_us) return;
  next_time_us = current_us + LOOP_INTERVAL_US;
  
  system_state.frame_count++;
 
  // 1. Scan all inputs
  scanAllInputs();
  
  // 2. Check switch states (LOW means ON)
  bool jvs_switch_now = (digitalRead(CHK_JVS_PIN) == LOW);
  bool joystick_switch_now = (digitalRead(CHK_JOYSTICK_PIN) == LOW);
  
  // 3. Real-time mode-change logic
  if (jvs_switch_now && joystick_switch_now) {
    // both ON -> keyboard mode (forced lock)
    system_state.selected_mode = MODE_KEYBOARD;
    system_state.mode_locked = true;
  } 
  else if (jvs_switch_now) {
    // JVS only ON -> JVS mode
    system_state.selected_mode = MODE_JVS;
    system_state.mode_locked = true;
  } 
  else if (system_state.mode_locked && !jvs_switch_now && !joystick_switch_now) {
    // when switches turned OFF (unlock) -> return to keyboard mode (fast blink)
    system_state.mode_locked = false;
    system_state.selected_mode = MODE_KEYBOARD; // ★ changed this to KEYBOARD
  }
  
  // 4. Joystick mode selection (only acts when unlocked)
  if (!system_state.mode_locked) {
    checkModeSelection();
  }
  
  // 5. Update JVS-active flag
  system_state.jvs_mode_active = (system_state.selected_mode == MODE_JVS);
  
  // 6. Update LED state
  updateStatusLED();
  
  // 7. Handle JVS communication
  if (system_state.jvs_mode_active) {
    processJVSCommands();
  }
  
  // 8. Send data to RP2040 (always performed)
  sendToRP2040();
}