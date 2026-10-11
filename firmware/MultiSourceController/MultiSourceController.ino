/*
 * Multi Source to JAMMA Converter
 * CXA2025AS + 74HC4052 + LM1881 Controller
 * 
 * Arduino Pro Micro (5V, 16MHz)
 * OLED 0.96" I2C (SSD1306, 128x64)
 * 
 * Inputs: composite, S-Video, VGA 15kHz
 * Output: JAMMA RGB
 */

#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================
// OLED configuration
// ============================================

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDR       0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================
// EEPROM addresses
// ============================================

#define EEPROM_MAGIC      0x00
#define EEPROM_INPUT      0x01
#define EEPROM_BRIGHTNESS 0x02
#define EEPROM_CONTRAST   0x03
#define EEPROM_SATURATION 0x04
#define EEPROM_HUE        0x05

#define EEPROM_MAGIC_VALUE 0xA5

// ============================================
// Pin definitions
// ============================================

// I2C (CXA2025AS, OLED)
#define PIN_SDA       2   // Pro Micro Pin 5
#define PIN_SCL       3   // Pro Micro Pin 6

// CXA2025AS control
#define PIN_YS        4   // Pro Micro Pin 7
#define PIN_YUV_SW    5   // Pro Micro Pin 8

// Button inputs
#define PIN_BTN_INPUT 6   // Pro Micro Pin 9
#define PIN_BTN_MODE  7   // Pro Micro Pin 10
#define PIN_BTN_PLUS  8   // Pro Micro Pin 11
#define PIN_BTN_MINUS 9   // Pro Micro Pin 12

// 74HC4052 control
#define PIN_MUX_A0    10  // Pro Micro Pin 13
#define PIN_MUX_A1    14  // Pro Micro Pin 15

// ============================================
// CXA2025AS I2C configuration
// ============================================

#define CXA2025AS_ADDR  0x44

#define REG_CONTRAST    0x00
#define REG_HUE         0x01
#define REG_SATURATION  0x02
#define REG_BRIGHTNESS  0x03
#define REG_CONTROL     0x0B

// ============================================
// Input mode definitions
// ============================================

enum InputMode {
  INPUT_COMPOSITE = 0,
  INPUT_SVIDEO    = 1,
  INPUT_VGA       = 2,
  INPUT_COUNT     = 3
};

// Input mode names (for OLED display)
const char* INPUT_NAMES[] = {
  "COMPOSITE",
  "S-VIDEO",
  "VGA 15kHz"
};

// Input mode icon characters
const char INPUT_ICONS[] = {'C', 'S', 'V'};

// ============================================
// Adjust mode definitions
// ============================================

enum AdjustMode {
  ADJ_BRIGHTNESS = 0,
  ADJ_CONTRAST   = 1,
  ADJ_SATURATION = 2,
  ADJ_HUE        = 3,
  ADJ_COUNT      = 4
};

// Adjust mode names (for OLED display)
const char* ADJUST_NAMES[] = {
  "BRIGHTNESS",
  "CONTRAST",
  "SATURATION",
  "HUE"
};

// Adjust mode abbreviations (for top display)
const char* ADJUST_SHORT[] = {
  "BRT",
  "CON",
  "SAT",
  "HUE"
};

// ============================================
// Global variables
// ============================================

InputMode currentInput = INPUT_COMPOSITE;
AdjustMode currentAdjust = ADJ_BRIGHTNESS;

uint8_t brightness = 32;
uint8_t contrast   = 32;
uint8_t saturation = 32;
uint8_t hue        = 32;

// Button debounce
unsigned long lastButtonTime = 0;
const unsigned long DEBOUNCE_DELAY = 150;

// Display update flag
bool needDisplayUpdate = true;

// CXA2025AS C-TRAP state (ON only in Composite mode)
// Datasheet: register 0x00 BIT6 = C-TRAP (3.58MHz chroma trap on the Y signal)
//   Composite: C-TRAP=1 (need to notch out chroma from composite Y)
//   S-Video  : C-TRAP=0 (Y is already a separated luma signal, so no trap needed)
//   VGA      : C-TRAP=0 (irrelevant, direct RGB input)
bool ctrap_active = false;

// OLED availability (set to false on failure; video processing continues regardless)
bool oled_ok = false;

// ============================================
// Function declarations
// ============================================

void setupPins();
void setupI2C();
void setupOLED();
void setupCXA2025AS();
void setInputMode(InputMode mode);
void setMuxChannel(uint8_t channel);
void setCXAMode(InputMode mode);
void writeRegister(uint8_t reg, uint8_t value);
void writeContrastReg();
void updateAllRegisters();
void handleButtons();
void adjustValue(int8_t delta);
uint8_t* getValuePtr(AdjustMode mode);
void loadSettings();
void saveSettings();
void updateDisplay();
void drawHeader();
void drawInputSection();
void drawAdjustSection();
void drawProgressBar(int x, int y, int width, int height, int value, int maxValue);

// ============================================
// Initialization
// ============================================

void setup() {
  Serial.begin(9600);
  
  setupPins();
  setupI2C();
  
  // OLED init (video processing continues even if it fails)
  setupOLED();
  
  // Splash screen (only when OLED is available)
  if (oled_ok) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println(F("MULTI"));
    display.setCursor(10, 30);
    display.println(F("SOURCE"));
    display.setTextSize(1);
    display.setCursor(10, 52);
    display.println(F("to JAMMA v1.0"));
    display.display();
    delay(1500);
  }
  
  loadSettings();
  setupCXA2025AS();
  setInputMode(currentInput);
  
  needDisplayUpdate = true;
}

// ============================================
// Main loop
// ============================================

void loop() {
  handleButtons();
  
  if (needDisplayUpdate) {
    updateDisplay();
    needDisplayUpdate = false;
  }
}

// ============================================
// Pin initialization
// ============================================

void setupPins() {
  pinMode(PIN_YS, OUTPUT);
  pinMode(PIN_YUV_SW, OUTPUT);
  pinMode(PIN_MUX_A0, OUTPUT);
  pinMode(PIN_MUX_A1, OUTPUT);
  
  pinMode(PIN_BTN_INPUT, INPUT_PULLUP);
  pinMode(PIN_BTN_MODE, INPUT_PULLUP);
  pinMode(PIN_BTN_PLUS, INPUT_PULLUP);
  pinMode(PIN_BTN_MINUS, INPUT_PULLUP);
  
  digitalWrite(PIN_YS, LOW);
  digitalWrite(PIN_YUV_SW, LOW);
  digitalWrite(PIN_MUX_A0, LOW);
  digitalWrite(PIN_MUX_A1, LOW);
}

// ============================================
// I2C initialization
// ============================================

void setupI2C() {
  Wire.begin();
  Wire.setClock(400000);  // 400kHz Fast Mode
}

// ============================================
// OLED initialization
// ============================================

void setupOLED() {
  oled_ok = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!oled_ok) {
    Serial.println(F("OLED failed - continuing without display"));
    return;  // video processing continues even without an OLED
  }
  display.clearDisplay();
  display.display();
}

// ============================================
// CXA2025AS initialization
// ============================================

void setupCXA2025AS() {
  delay(100);
  updateAllRegisters();
  
  // REG_CONTROL (0x0B) = 0xF8 = 0b11111000
  //   BIT7 RON=1, BIT6 GON=1, BIT5 BON=1 : enable R/G/B outputs
  //   BIT4 PICON=1                       : Picture(RGB) fully ON
  //   BIT3 VOFF=1                        : stop V sawtooth oscillator (OK, CRT deflection unused)
  //   BIT2 FHHI=0                        : H AFC normal operation
  //   BIT1 CD-MODE=0                     : V countdown automatic
  //   BIT0 AKBOFF=0                      : AKB operation ON (auto cutoff)
  uint8_t control = 0xF8;
  writeRegister(REG_CONTROL, control);
}

// ============================================
// Input mode selection
// ============================================

void setInputMode(InputMode mode) {
  currentInput = mode;
  setMuxChannel((uint8_t)mode);
  setCXAMode(mode);
}

// ============================================
// 74HC4052 channel selection
// ============================================

void setMuxChannel(uint8_t channel) {
  digitalWrite(PIN_MUX_A0, (channel & 0x01) ? HIGH : LOW);
  digitalWrite(PIN_MUX_A1, (channel & 0x02) ? HIGH : LOW);
}

// ============================================
// CXA2025AS mode setup
// ============================================

void setCXAMode(InputMode mode) {
  switch (mode) {
    case INPUT_COMPOSITE:
      digitalWrite(PIN_YS, LOW);          // select Y/C path
      digitalWrite(PIN_YUV_SW, LOW);      // internal Y/C path
      ctrap_active = true;                // chroma notch ON on composite Y
      break;
    case INPUT_SVIDEO:
      digitalWrite(PIN_YS, LOW);          // select Y/C path
      digitalWrite(PIN_YUV_SW, LOW);      // internal Y/C path
      ctrap_active = false;               // external C used, Y is clean -> trap OFF
      break;
    case INPUT_VGA:
      digitalWrite(PIN_YS, HIGH);         // select RGB path (RIN/GIN/BIN direct)
      digitalWrite(PIN_YUV_SW, LOW);      // meaningless, YS=HIGH takes priority
      ctrap_active = false;               // irrelevant, direct RGB path
      break;
  }
  
  // immediate update of register 0x00 (C-TRAP bit + CONTRAST value)
  writeContrastReg();
}

// ============================================
// Helper to write register 0x00 (C-TRAP[b6] + CONTRAST[5:0])
// ============================================
void writeContrastReg() {
  uint8_t reg = (ctrap_active ? 0x40 : 0x00) | (contrast & 0x3F);
  writeRegister(REG_CONTRAST, reg);
}

// ============================================
// I2C register write
// ============================================

void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(CXA2025AS_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

// ============================================
// Update all registers
// ============================================

void updateAllRegisters() {
  writeRegister(REG_BRIGHTNESS, brightness);
  writeContrastReg();   // write 0x00 preserving the C-TRAP bit
  writeRegister(REG_SATURATION, saturation);
  writeRegister(REG_HUE, hue);
}

// ============================================
// Button handling
// ============================================

void handleButtons() {
  if (millis() - lastButtonTime < DEBOUNCE_DELAY) {
    return;
  }
  
  // Input select button
  if (digitalRead(PIN_BTN_INPUT) == LOW) {
    lastButtonTime = millis();
    currentInput = (InputMode)((currentInput + 1) % INPUT_COUNT);
    setInputMode(currentInput);
    saveSettings();
    needDisplayUpdate = true;
    while (digitalRead(PIN_BTN_INPUT) == LOW);
  }
  
  // Mode select button
  if (digitalRead(PIN_BTN_MODE) == LOW) {
    lastButtonTime = millis();
    currentAdjust = (AdjustMode)((currentAdjust + 1) % ADJ_COUNT);
    needDisplayUpdate = true;
    while (digitalRead(PIN_BTN_MODE) == LOW);
  }
  
  // + button
  if (digitalRead(PIN_BTN_PLUS) == LOW) {
    lastButtonTime = millis();
    adjustValue(+2);
    needDisplayUpdate = true;
    while (digitalRead(PIN_BTN_PLUS) == LOW);
  }
  
  // - button
  if (digitalRead(PIN_BTN_MINUS) == LOW) {
    lastButtonTime = millis();
    adjustValue(-2);
    needDisplayUpdate = true;
    while (digitalRead(PIN_BTN_MINUS) == LOW);
  }
}

// ============================================
// Get pointer to a value
// ============================================

uint8_t* getValuePtr(AdjustMode mode) {
  switch (mode) {
    case ADJ_BRIGHTNESS: return &brightness;
    case ADJ_CONTRAST:   return &contrast;
    case ADJ_SATURATION: return &saturation;
    case ADJ_HUE:        return &hue;
    default:             return &brightness;
  }
}

// ============================================
// Adjust a value
// ============================================

void adjustValue(int8_t delta) {
  uint8_t* valuePtr = getValuePtr(currentAdjust);
  
  int16_t newValue = (int16_t)*valuePtr + delta;
  if (newValue < 0) newValue = 0;
  if (newValue > 63) newValue = 63;
  *valuePtr = (uint8_t)newValue;
  
  switch (currentAdjust) {
    case ADJ_BRIGHTNESS: writeRegister(REG_BRIGHTNESS, brightness); break;
    case ADJ_CONTRAST:   writeContrastReg(); break;  // write 0x00 preserving the C-TRAP bit
    case ADJ_SATURATION: writeRegister(REG_SATURATION, saturation); break;
    case ADJ_HUE:        writeRegister(REG_HUE, hue); break;
    default: return;
  }
  
  saveSettings();
}

// ============================================
// Load EEPROM settings
// ============================================

void loadSettings() {
  if (EEPROM.read(EEPROM_MAGIC) != EEPROM_MAGIC_VALUE) {
    saveSettings();
    return;
  }
  
  currentInput = (InputMode)EEPROM.read(EEPROM_INPUT);
  brightness   = EEPROM.read(EEPROM_BRIGHTNESS);
  contrast     = EEPROM.read(EEPROM_CONTRAST);
  saturation   = EEPROM.read(EEPROM_SATURATION);
  hue          = EEPROM.read(EEPROM_HUE);
  
  if (currentInput >= INPUT_COUNT) currentInput = INPUT_COMPOSITE;
  if (brightness > 63) brightness = 32;
  if (contrast > 63) contrast = 32;
  if (saturation > 63) saturation = 32;
  if (hue > 63) hue = 32;
}

// ============================================
// Save EEPROM settings
// ============================================

void saveSettings() {
  EEPROM.update(EEPROM_MAGIC, EEPROM_MAGIC_VALUE);
  EEPROM.update(EEPROM_INPUT, (uint8_t)currentInput);
  EEPROM.update(EEPROM_BRIGHTNESS, brightness);
  EEPROM.update(EEPROM_CONTRAST, contrast);
  EEPROM.update(EEPROM_SATURATION, saturation);
  EEPROM.update(EEPROM_HUE, hue);
}

// ============================================
// Update OLED display
// ============================================

void updateDisplay() {
  if (!oled_ok) return;  // skip the refresh if there is no OLED
  
  display.clearDisplay();
  
  drawHeader();
  drawInputSection();
  drawAdjustSection();
  
  display.display();
}

// ============================================
// Draw the header
// ============================================

void drawHeader() {
  // top line
  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);
  
  // title
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 2);
  display.print(F("MULTI-SRC"));
  
  // show current adjust mode (top right)
  display.setCursor(90, 2);
  display.print(F("["));
  display.print(ADJUST_SHORT[currentAdjust]);
  display.print(F("]"));
}

// ============================================
// Draw the input section
// ============================================

void drawInputSection() {
  int y = 16;
  
  // input select boxes
  for (int i = 0; i < INPUT_COUNT; i++) {
    int x = i * 43;
    int boxWidth = 40;
    int boxHeight = 18;
    
    if (i == currentInput) {
      // selected input: filled box
      display.fillRoundRect(x, y, boxWidth, boxHeight, 3, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      // unselected input: empty box
      display.drawRoundRect(x, y, boxWidth, boxHeight, 3, SSD1306_WHITE);
      display.setTextColor(SSD1306_WHITE);
    }
    
    // icon character
    display.setTextSize(2);
    display.setCursor(x + 14, y + 2);
    display.print(INPUT_ICONS[i]);
  }
  
  // selected input name (shown below)
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  
  // center the name
  int nameLen = strlen(INPUT_NAMES[currentInput]);
  int nameX = (128 - nameLen * 6) / 2;
  display.setCursor(nameX, 36);
  display.print(INPUT_NAMES[currentInput]);
}

// ============================================
// Draw the adjust section
// ============================================

void drawAdjustSection() {
  int y = 46;
  
  // separator line
  display.drawLine(0, y, 127, y, SSD1306_WHITE);
  
  y += 2;
  
  // adjust mode name
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, y);
  display.print(ADJUST_NAMES[currentAdjust]);
  
  // current value
  uint8_t* valuePtr = getValuePtr(currentAdjust);
  display.setCursor(100, y);
  if (*valuePtr < 10) display.print(F(" "));
  display.print(*valuePtr);
  
  // progress bar
  y += 9;
  drawProgressBar(0, y, 128, 7, *valuePtr, 63);
}

// ============================================
// Draw the progress bar
// ============================================

void drawProgressBar(int x, int y, int width, int height, int value, int maxValue) {
  // outline
  display.drawRect(x, y, width, height, SSD1306_WHITE);
  
  // fill
  int fillWidth = map(value, 0, maxValue, 0, width - 4);
  if (fillWidth > 0) {
    display.fillRect(x + 2, y + 2, fillWidth, height - 4, SSD1306_WHITE);
  }
  
  // center marker (default value position)
  int centerX = x + (width / 2);
  display.drawLine(centerX, y, centerX, y + height - 1, SSD1306_WHITE);
}
