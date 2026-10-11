// ============================================================================
//  MegaKey PCB v4.0 - AVR128DB64-I/PT Edition
//  
//  Hardware:
//    - AVR128DB64-I/PT (TQFP-64, 24MHz internal OSCHF, 5V operation)
//    - 74HC595 × 2 (KVM LED + STATUS + MUX control, 16-bit SPI)
//    - 2× GP2040-CE (one-way transmit over Serial2/Serial3)
//    - JVS IO (Serial0/USART0, TCB1 Sense signal)
//    - HDMI switch IR (NEC, TCB0 PWM on PA2)
//
//  Porting changes (original MegaKey_IO_PCB.ino → v4.0):
//    1. Remove #define IR_USE_AVR_TIMER3 (DxCore handles TCB automatically)
//    2. All pin definitions → PIN_Pxn (pins_megakey.h)
//    3. Timer2 (JVS Sense) → TCB1 PWM (jvs_avrdb.h)
//    4. UCSR{2,3}B registers → USART{2,3}.CTRLB
//    5. Remove SPI-disable code (AVR DB is inactive unless SPI begin() is called)
//    6. Remove direct DDRB/PORTB manipulation (port layout changed)
//    7. KVM LED, STATUS LED, MUX control → delegated to 74HC595 bits
//
//  Compile environment:
//    - Arduino IDE + DxCore (https://github.com/SpenceKonde/DxCore)
//    - Board: AVR DB-series → AVR128DB64
//    - Reset pin: GPIO (PF6, no Reset)   ← set in Tools menu
//    - Clock: 24MHz internal
//    - Libraries:
//        * JVSIO (https://github.com/twitter/jvsio)
//        * IRremote v4.x (DxCore-compatible version)
// ============================================================================

#include "jvsio/JVSIO.h"
#include "jvsio/JVSIO.cpp"
#include <IRremote.hpp>     // v4.x header (older versions: IRremote.h)

#include "pins_megakey.h"
#include "shift_register.h"
#include "jvs_avrdb.h"

#pragma GCC optimize("O3")

// ========== System constants ==========
#define FIRMWARE_VERSION   "4.0-AVR128DB64"
#define SERIAL_SPEED       250000UL
#define JVS_SERIAL_SPEED   115200UL
#define LOOP_INTERVAL_US   1000UL      // 1ms (1000Hz)
#define COIN_DEBOUNCE_MS   50

// ========== Operating modes ==========
#define MODE_KEYBOARD    0
#define MODE_XINPUT      1
#define MODE_SWITCH      2
#define MODE_PS3         3
#define MODE_PS4         4
#define MODE_PS5         5
#define MODE_XBOX        6
#define MODE_JVS         7
#define MODE_DREAMCAST   8

// ========== KVM channel ==========
#define KVM_CH1          1
#define KVM_CH2          2
#define KVM_CH3          3
#define KVM_CH4          4
#define KVM_CH5_JAMMA    5

// ========== Button bit positions (same as RP2040 protocol) ==========
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

// ========== Communication protocol ==========
#define BINARY_HEADER    0xAA
#define BINARY_FOOTER    0x55

// ========== Global objects ==========
ShiftRegister16 sr;
AVRDBJVSDataClient   jvs_data;
AVRDBJVSSenseClient  jvs_sense;
AVRDBJVSLedClient    jvs_led;
JVSIO io(&jvs_data, &jvs_sense, &jvs_led);

// AVRDBJVSLedClient::set() implementation (in .ino due to sr dependency)
void AVRDBJVSLedClient::set(bool ready) {
  if (_sr) {
    _sr->setBit(SR_BIT_STATUS_LED, ready);
    _sr->flush();
  }
}

// ========== Data structures ==========
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
  uint8_t  header;
  uint32_t p1_buttons;
  uint32_t p2_buttons;
  uint8_t  modes;
  uint8_t  channel_state;
  uint8_t  crc;
  uint8_t  footer;
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
const uint8_t kvm_led_bits[] = {SR_BIT_KVM1_LED, SR_BIT_KVM2_LED, SR_BIT_KVM3_LED, SR_BIT_KVM4_LED, SR_BIT_KVM5_LED};

uint16_t coin_count[2];
bool coin_prev_state[2];
unsigned long coin_time[2];

// ========== Function declarations ==========
void sendNECCommand(uint32_t command);
void sendHDMISwitchCommand(uint8_t channel);
uint32_t scanPlayerButtons(const ButtonMapping* button_map);

// ========== CRC8 computation ==========
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

// ========== Debugging LED flash (blinks STATUS LED via 74HC595) ==========
void debugFlash(uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    sr.setBit(SR_BIT_STATUS_LED, true);
    sr.flush();
    delay(50);
    sr.setBit(SR_BIT_STATUS_LED, false);
    sr.flush();
    delay(50);
  }
}

// ========== KVM channel update ==========
void updateKVMChannel(uint8_t channel) {
  // USB MUX control (change 74HC595 bits)
  // Original logic: digitalWrite(MUX1_SEL_PIN, X); digitalWrite(MUX1_OE_PIN, X); ...
  switch (channel) {
    case KVM_CH1:
      sr.setBit(SR_BIT_MUX1_SEL, LOW);
      sr.setBit(SR_BIT_MUX1_OE,  LOW);
      sr.setBit(SR_BIT_MUX2_OE,  HIGH);
      break;
    case KVM_CH2:
      sr.setBit(SR_BIT_MUX1_SEL, HIGH);
      sr.setBit(SR_BIT_MUX1_OE,  LOW);
      sr.setBit(SR_BIT_MUX2_OE,  HIGH);
      break;
    case KVM_CH3:
      sr.setBit(SR_BIT_MUX2_SEL, LOW);
      sr.setBit(SR_BIT_MUX1_OE,  HIGH);
      sr.setBit(SR_BIT_MUX2_OE,  LOW);
      break;
    case KVM_CH4:
      sr.setBit(SR_BIT_MUX2_SEL, HIGH);
      sr.setBit(SR_BIT_MUX1_OE,  HIGH);
      sr.setBit(SR_BIT_MUX2_OE,  LOW);
      break;
    case KVM_CH5_JAMMA:
      sr.setBit(SR_BIT_MUX1_OE, HIGH);
      sr.setBit(SR_BIT_MUX2_OE, HIGH);
      break;
  }
  sr.flush();
  
  // HDMI IR transmit
  sendHDMISwitchCommand(channel);
}

// ========== IR transmit function (IRremote v4.x) ==========
void sendHDMISwitchCommand(uint8_t channel) {
  uint32_t nec_command = 0;
  switch (channel) {
    case KVM_CH1:       nec_command = 0xF807FF00; break;
    case KVM_CH2:       nec_command = 0xF609FF00; break;
    case KVM_CH3:       nec_command = 0xE916FF00; break;
    case KVM_CH4:       nec_command = 0xF20DFF00; break;
    case KVM_CH5_JAMMA: nec_command = 0xE718FF00; break;
  }
  
  if (nec_command != 0) {
    sendNECCommand(nec_command);
  }
}

void sendNECCommand(uint32_t command) {
  // Transmit code for IRremote v4.x (TCB used automatically, PA2 output)
  IrSender.sendNECRaw(command, 0);
}

// ========== Mode check and input handling ==========
void checkInitialMode() {
  bool jvs_low = (digitalRead(CHK_JVS_PIN) == LOW);
  bool joystick_low = (digitalRead(CHK_JOYSTICK_PIN) == LOW);
  
  // 1. Both on -> keyboard (for maintenance)
  if (jvs_low && joystick_low) {
    system_state.selected_mode = MODE_KEYBOARD;
    system_state.jvs_mode_active = false;
    system_state.mode_locked = true;
  }
  // 2. JVS only on -> JVS mode
  else if (jvs_low) {
    system_state.selected_mode = MODE_JVS;
    system_state.jvs_mode_active = true;
    system_state.mode_locked = true;
  }
  // 3. Joystick only on -> joystick mode (XInput etc.)
  else if (joystick_low) {
    system_state.p1_buttons = scanPlayerButtons(p1_buttons);
    
    if      (system_state.p1_buttons & (1UL << BIT_X))  system_state.selected_mode = MODE_DREAMCAST;
    else if (system_state.p1_buttons & (1UL << BIT_Y))  system_state.selected_mode = MODE_PS3;
    else if (system_state.p1_buttons & (1UL << BIT_A))  system_state.selected_mode = MODE_PS4;
    else if (system_state.p1_buttons & (1UL << BIT_B))  system_state.selected_mode = MODE_SWITCH;
    else if (system_state.p1_buttons & (1UL << BIT_LB)) system_state.selected_mode = MODE_KEYBOARD;
    else if (system_state.p1_buttons & (1UL << BIT_RB)) system_state.selected_mode = MODE_PS5;
    else if (system_state.p1_buttons & (1UL << BIT_LT)) system_state.selected_mode = MODE_XBOX;
    else                                                  system_state.selected_mode = MODE_XINPUT;
    
    system_state.jvs_mode_active = false;
    system_state.mode_locked = true;
  }
  // 4. Both off -> keyboard mode (default state)
  else {
    system_state.selected_mode = MODE_KEYBOARD;
    system_state.jvs_mode_active = false;
    system_state.mode_locked = false;
  }
}

void checkModeSelection() {
  if (system_state.mode_locked) return;
  
  bool joystick_current = digitalRead(CHK_JOYSTICK_PIN);
  if (system_state.chk_joystick_prev == HIGH && joystick_current == LOW) {
    uint32_t p1 = system_state.p1_buttons;
    if      (p1 & (1UL << BIT_X))  system_state.selected_mode = MODE_DREAMCAST;
    else if (p1 & (1UL << BIT_Y))  system_state.selected_mode = MODE_PS3;
    else if (p1 & (1UL << BIT_A))  system_state.selected_mode = MODE_PS4;
    else if (p1 & (1UL << BIT_B))  system_state.selected_mode = MODE_SWITCH;
    else if (p1 & (1UL << BIT_LB)) system_state.selected_mode = MODE_KEYBOARD;
    else if (p1 & (1UL << BIT_RB)) system_state.selected_mode = MODE_PS5;
    else if (p1 & (1UL << BIT_LT)) system_state.selected_mode = MODE_XBOX;
    else                            system_state.selected_mode = MODE_XINPUT;
    
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
  
  // Coin handling (P1 SELECT as coin1, P2 SELECT as coin2)
  unsigned long current_time = millis();
  bool p1_coin = (system_state.p1_buttons & (1UL << BIT_SELECT)) != 0;
  if (p1_coin && !coin_prev_state[0]) {
    if (current_time - coin_time[0] > COIN_DEBOUNCE_MS) {
      if (coin_count[0] < 9999) coin_count[0]++;
      coin_time[0] = current_time;
    }
  }
  coin_prev_state[0] = p1_coin;
  
  bool p2_coin = (system_state.p2_buttons & (1UL << BIT_SELECT)) != 0;
  if (p2_coin && !coin_prev_state[1]) {
    if (current_time - coin_time[1] > COIN_DEBOUNCE_MS) {
      if (coin_count[1] < 9999) coin_count[1]++;
      coin_time[1] = current_time;
    }
  }
  coin_prev_state[1] = p2_coin;
}

// ========== STATUS LED update (STATUS_LED bit of 74HC595) ==========
void updateStatusLED() {
  static unsigned long last_blink = 0;
  static bool led_state = false;
  unsigned long current_time = millis();
  
  if (system_state.jvs_mode_active) {
    if (current_time - last_blink > 500) {
      led_state = !led_state;
      sr.setBit(SR_BIT_STATUS_LED, led_state);
      last_blink = current_time;
    }
  } else if (!system_state.mode_locked) {
    if (current_time - last_blink > 100) {
      led_state = !led_state;
      sr.setBit(SR_BIT_STATUS_LED, led_state);
      last_blink = current_time;
    }
  } else {
    sr.setBit(SR_BIT_STATUS_LED, true);
  }
}

// ========== KVM LED update (KVM1~5 bits of 74HC595) ==========
void updateKVMLEDs() {
  uint8_t ch = system_state.kvm_channel;
  for (uint8_t i = 0; i < 5; i++) {
    sr.setBit(kvm_led_bits[i], (ch == (i + 1)));
  }
}

// ========== RP2040 transmit ==========
void sendToRP2040() {
  CompactMessage msg;
  
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
  
  Serial2.write((uint8_t*)&msg, sizeof(CompactMessage));  // RP2040 #1 (1P)
  Serial3.write((uint8_t*)&msg, sizeof(CompactMessage));  // RP2040 #2 (2P)
}

// ========== JVS response generation ==========
void generateJVSSwitchData(uint8_t players, uint8_t bytes_per_player) {
  io.pushReport(JVSIO::kReportOk);
  
  uint32_t p1_state = system_state.swap_stick ? system_state.p2_buttons : system_state.p1_buttons;
  uint32_t p2_state = system_state.swap_stick ? system_state.p1_buttons : system_state.p2_buttons;
  
  uint8_t system_byte = 0;
  if (p1_state & (1UL << BIT_HOME))  system_byte |= 0x80;
  if (p1_state & (1UL << BIT_START)) system_byte |= 0x01;
  if (p2_state & (1UL << BIT_START)) system_byte |= 0x02;
  io.pushReport(system_byte);
  
  for (uint8_t player = 0; player < players && player < 2; player++) {
    uint32_t buttons = (player == 0) ? p1_state : p2_state;
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
      for (size_t i = 0; i < strlen_P(jvs_system_id); ++i) {
        io.pushReport(pgm_read_byte(&jvs_system_id[i]));
      }
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
      
    case JVSIO::kCmdCoinInput: {
        uint8_t slots = data[1];
        io.pushReport(JVSIO::kReportOk);
        for (uint8_t s = 0; s < slots && s < 2; s++) {
          io.pushReport((coin_count[s] >> 8) & 0xFF);
          io.pushReport(coin_count[s] & 0xFF);
        }
      }
      break;
      
    case JVSIO::kCmdAnalogInput: {
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

// ============================================================================
//  setup() - initialization
// ============================================================================
void setup() {
  // ----- 1. 74HC595 init (first, stabilizes LED/MUX at boot) -----
  sr.begin();
  // Initial MUX state: all USB blocked (like KVM5 JAMMA mode)
  sr.setBit(SR_BIT_MUX1_OE, HIGH);
  sr.setBit(SR_BIT_MUX2_OE, HIGH);
  sr.flush();
  
  // ----- 2. Attach SR to JVS Led Client -----
  jvs_led.attachSR(&sr);
  
  // ----- 3. UART init (Serial2/Serial3 for RP2040 transmit, JVS in io.begin()) -----
  Serial2.begin(SERIAL_SPEED);   // PF0 (USART2 TX) → RP2040 #1
  Serial3.begin(SERIAL_SPEED);   // PB0 (USART3 TX) → RP2040 #2
  
  // Disable RX (use PF1, PB1 as GPIO)
  USART2.CTRLB &= ~USART_RXEN_bm;
  USART3.CTRLB &= ~USART_RXEN_bm;
  
  // ----- 4. JVS IO init (Serial0=PA0/PA1, TCB1 Sense=PA3) -----
  delay(10);
  io.begin();
  
  // ----- 5. Pin mode configuration -----
  // Mode switches
  pinMode(CHK_JOYSTICK_PIN, INPUT_PULLUP);
  pinMode(CHK_JVS_PIN,      INPUT_PULLUP);
  pinMode(CHK_SWAPSTICK_PIN, INPUT_PULLUP);
  
  // KVM channel buttons
  pinMode(CHK_KVM1_PIN, INPUT_PULLUP);
  pinMode(CHK_KVM2_PIN, INPUT_PULLUP);
  pinMode(CHK_KVM3_PIN, INPUT_PULLUP);
  pinMode(CHK_KVM4_PIN, INPUT_PULLUP);
  pinMode(CHK_KVM5_PIN, INPUT_PULLUP);
  
  // Player 1/2 buttons - auto-configured from PROGMEM mapping
  for (uint8_t i = 0; i < 17; i++) {
    ButtonMapping btn;
    memcpy_P(&btn, &p1_buttons[i], sizeof(ButtonMapping));
    pinMode(btn.pin, INPUT_PULLUP);
    memcpy_P(&btn, &p2_buttons[i], sizeof(ButtonMapping));
    pinMode(btn.pin, INPUT_PULLUP);
  }
  // Analog stick press-check pins (used as digital inputs)
  pinMode(P1_LANAL_PIN, INPUT_PULLUP);
  pinMode(P1_RANAL_PIN, INPUT_PULLUP);
  pinMode(P2_LANAL_PIN, INPUT_PULLUP);
  pinMode(P2_RANAL_PIN, INPUT_PULLUP);
  
  // ----- 6. IR transmitter init (IRremote v4.x, uses TCB0, output pin PA2) -----
  IrSender.begin(IR_LED_PIN, ENABLE_LED_FEEDBACK);
  
  // ----- 7. System state init -----
  memset(&system_state, 0, sizeof(system_state));
  system_state.chk_joystick_prev = HIGH;
  system_state.kvm_channel = KVM_CH1;
  
  memset(coin_count, 0, sizeof(coin_count));
  memset(coin_prev_state, 0, sizeof(coin_prev_state));
  memset(coin_time, 0, sizeof(coin_time));
  
  // ----- 8. Initial mode decision and KVM channel setup -----
  delay(100);
  checkInitialMode();
  updateKVMChannel(system_state.kvm_channel);
  
  // Init-complete indication (STATUS LED blinks 3 times)
  debugFlash(3);
}

// ============================================================================
//  loop() - main loop (1ms = 1000Hz period)
// ============================================================================
void loop() {
  // KVM LEDs refreshed every loop (SR flush is a no-op if unchanged)
  updateKVMLEDs();
  
  // 1ms loop interval control
  static unsigned long next_time_us = 0;
  unsigned long current_us = micros();
  if (current_us < next_time_us) return;
  next_time_us = current_us + LOOP_INTERVAL_US;
  
  system_state.frame_count++;
  
  // 1. Scan all inputs
  scanAllInputs();
  
  // 2. Check switch states
  bool jvs_switch_now      = (digitalRead(CHK_JVS_PIN) == LOW);
  bool joystick_switch_now = (digitalRead(CHK_JOYSTICK_PIN) == LOW);
  
  // 3. Real-time mode-change logic
  if (jvs_switch_now && joystick_switch_now) {
    system_state.selected_mode = MODE_KEYBOARD;
    system_state.mode_locked = true;
  } else if (jvs_switch_now) {
    system_state.selected_mode = MODE_JVS;
    system_state.mode_locked = true;
  } else if (system_state.mode_locked && !jvs_switch_now && !joystick_switch_now) {
    system_state.mode_locked = false;
    system_state.selected_mode = MODE_KEYBOARD;
  }
  
  // 4. Joystick mode selection (only when unlocked)
  if (!system_state.mode_locked) {
    checkModeSelection();
  }
  
  // 5. Update JVS-active status
  system_state.jvs_mode_active = (system_state.selected_mode == MODE_JVS);
  
  // 6. STATUS LED update
  updateStatusLED();
  
  // 7. Process JVS communication
  if (system_state.jvs_mode_active) {
    processJVSCommands();
  }
  
  // 8. Batch shift-register output (send LED/MUX changes once via SPI)
  sr.flush();
  
  // 9. Transmit data to RP2040 (always performed)
  sendToRP2040();
}
