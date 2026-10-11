/*
 * Multi Source to JAMMA Converter
 * CXA2025AS + 74HC4052 + LM1881 Controller
 * 
 * Arduino Pro Micro (5V, 16MHz)
 * OLED 0.96" I2C (SSD1306, 128x64)
 * 
 * 입력: 컴포지트, S-Video, VGA 15kHz
 * 출력: JAMMA RGB
 */

#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================
// OLED 설정
// ============================================

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDR       0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================
// EEPROM 주소
// ============================================

#define EEPROM_MAGIC      0x00
#define EEPROM_INPUT      0x01
#define EEPROM_BRIGHTNESS 0x02
#define EEPROM_CONTRAST   0x03
#define EEPROM_SATURATION 0x04
#define EEPROM_HUE        0x05

#define EEPROM_MAGIC_VALUE 0xA5

// ============================================
// 핀 정의
// ============================================

// I2C (CXA2025AS, OLED)
#define PIN_SDA       2   // Pro Micro Pin 5
#define PIN_SCL       3   // Pro Micro Pin 6

// CXA2025AS 제어
#define PIN_YS        4   // Pro Micro Pin 7
#define PIN_YUV_SW    5   // Pro Micro Pin 8

// 버튼 입력
#define PIN_BTN_INPUT 6   // Pro Micro Pin 9
#define PIN_BTN_MODE  7   // Pro Micro Pin 10
#define PIN_BTN_PLUS  8   // Pro Micro Pin 11
#define PIN_BTN_MINUS 9   // Pro Micro Pin 12

// 74HC4052 제어
#define PIN_MUX_A0    10  // Pro Micro Pin 13
#define PIN_MUX_A1    14  // Pro Micro Pin 15

// ============================================
// CXA2025AS I2C 설정
// ============================================

#define CXA2025AS_ADDR  0x44

#define REG_CONTRAST    0x00
#define REG_HUE         0x01
#define REG_SATURATION  0x02
#define REG_BRIGHTNESS  0x03
#define REG_CONTROL     0x0B

// ============================================
// 입력 모드 정의
// ============================================

enum InputMode {
  INPUT_COMPOSITE = 0,
  INPUT_SVIDEO    = 1,
  INPUT_VGA       = 2,
  INPUT_COUNT     = 3
};

// 입력 모드 이름 (OLED 표시용)
const char* INPUT_NAMES[] = {
  "COMPOSITE",
  "S-VIDEO",
  "VGA 15kHz"
};

// 입력 모드 아이콘 문자
const char INPUT_ICONS[] = {'C', 'S', 'V'};

// ============================================
// 조정 모드 정의
// ============================================

enum AdjustMode {
  ADJ_BRIGHTNESS = 0,
  ADJ_CONTRAST   = 1,
  ADJ_SATURATION = 2,
  ADJ_HUE        = 3,
  ADJ_COUNT      = 4
};

// 조정 모드 이름 (OLED 표시용)
const char* ADJUST_NAMES[] = {
  "BRIGHTNESS",
  "CONTRAST",
  "SATURATION",
  "HUE"
};

// 조정 모드 약어 (상단 표시용)
const char* ADJUST_SHORT[] = {
  "BRT",
  "CON",
  "SAT",
  "HUE"
};

// ============================================
// 전역 변수
// ============================================

InputMode currentInput = INPUT_COMPOSITE;
AdjustMode currentAdjust = ADJ_BRIGHTNESS;

uint8_t brightness = 32;
uint8_t contrast   = 32;
uint8_t saturation = 32;
uint8_t hue        = 32;

// 버튼 디바운스
unsigned long lastButtonTime = 0;
const unsigned long DEBOUNCE_DELAY = 150;

// 화면 업데이트 플래그
bool needDisplayUpdate = true;

// CXA2025AS C-TRAP 상태 (Composite 모드일 때만 ON)
// 데이터시트: 0x00 레지스터 BIT6 = C-TRAP (Y 신호의 3.58MHz chroma 트랩)
//   Composite: C-TRAP=1 (composite Y에서 chroma 노치 제거 필요)
//   S-Video  : C-TRAP=0 (Y가 이미 분리된 휘도이므로 트랩 불필요)
//   VGA      : C-TRAP=0 (RGB 직접 입력이라 무관)
bool ctrap_active = false;

// OLED 사용 가능 여부 (실패 시 false로 두고 영상 처리는 계속 진행)
bool oled_ok = false;

// ============================================
// 함수 선언
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
// 초기화
// ============================================

void setup() {
  Serial.begin(9600);
  
  setupPins();
  setupI2C();
  
  // OLED 초기화 (실패해도 영상 처리는 계속)
  setupOLED();
  
  // 시작 화면 (OLED 사용 가능할 때만)
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
// 메인 루프
// ============================================

void loop() {
  handleButtons();
  
  if (needDisplayUpdate) {
    updateDisplay();
    needDisplayUpdate = false;
  }
}

// ============================================
// 핀 초기화
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
// I2C 초기화
// ============================================

void setupI2C() {
  Wire.begin();
  Wire.setClock(400000);  // 400kHz Fast Mode
}

// ============================================
// OLED 초기화
// ============================================

void setupOLED() {
  oled_ok = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!oled_ok) {
    Serial.println(F("OLED failed - continuing without display"));
    return;  // OLED 없어도 영상 처리는 계속 진행
  }
  display.clearDisplay();
  display.display();
}

// ============================================
// CXA2025AS 초기화
// ============================================

void setupCXA2025AS() {
  delay(100);
  updateAllRegisters();
  
  // REG_CONTROL (0x0B) = 0xF8 = 0b11111000
  //   BIT7 RON=1, BIT6 GON=1, BIT5 BON=1 : R/G/B 출력 활성화
  //   BIT4 PICON=1                       : Picture(RGB) 전체 ON
  //   BIT3 VOFF=1                        : V 톱니파 발진 정지 (CRT 편향 미사용이므로 OK)
  //   BIT2 FHHI=0                        : H AFC 정상 동작
  //   BIT1 CD-MODE=0                     : V countdown 자동
  //   BIT0 AKBOFF=0                      : AKB 동작 ON (자동 컷오프)
  uint8_t control = 0xF8;
  writeRegister(REG_CONTROL, control);
}

// ============================================
// 입력 모드 설정
// ============================================

void setInputMode(InputMode mode) {
  currentInput = mode;
  setMuxChannel((uint8_t)mode);
  setCXAMode(mode);
}

// ============================================
// 74HC4052 채널 선택
// ============================================

void setMuxChannel(uint8_t channel) {
  digitalWrite(PIN_MUX_A0, (channel & 0x01) ? HIGH : LOW);
  digitalWrite(PIN_MUX_A1, (channel & 0x02) ? HIGH : LOW);
}

// ============================================
// CXA2025AS 모드 설정
// ============================================

void setCXAMode(InputMode mode) {
  switch (mode) {
    case INPUT_COMPOSITE:
      digitalWrite(PIN_YS, LOW);          // Y/C block 선택
      digitalWrite(PIN_YUV_SW, LOW);      // 내부 Y/C 경로
      ctrap_active = true;                // composite Y에서 chroma 노치 ON
      break;
    case INPUT_SVIDEO:
      digitalWrite(PIN_YS, LOW);          // Y/C block 선택
      digitalWrite(PIN_YUV_SW, LOW);      // 내부 Y/C 경로
      ctrap_active = false;               // 외부 C 사용, Y는 깨끗 -> trap OFF
      break;
    case INPUT_VGA:
      digitalWrite(PIN_YS, HIGH);         // RGB block 선택 (RIN/GIN/BIN 직접)
      digitalWrite(PIN_YUV_SW, LOW);      // YS=HIGH 우선이라 의미 없음
      ctrap_active = false;               // RGB 직접 경로라 무관
      break;
  }
  
  // 0x00 레지스터 즉시 갱신 (C-TRAP 비트 + CONTRAST 값)
  writeContrastReg();
}

// ============================================
// 0x00 레지스터 쓰기 헬퍼 (C-TRAP[b6] + CONTRAST[5:0])
// ============================================
void writeContrastReg() {
  uint8_t reg = (ctrap_active ? 0x40 : 0x00) | (contrast & 0x3F);
  writeRegister(REG_CONTRAST, reg);
}

// ============================================
// I2C 레지스터 쓰기
// ============================================

void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(CXA2025AS_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

// ============================================
// 모든 레지스터 업데이트
// ============================================

void updateAllRegisters() {
  writeRegister(REG_BRIGHTNESS, brightness);
  writeContrastReg();   // 0x00에는 C-TRAP 비트 보존하여 쓰기
  writeRegister(REG_SATURATION, saturation);
  writeRegister(REG_HUE, hue);
}

// ============================================
// 버튼 처리
// ============================================

void handleButtons() {
  if (millis() - lastButtonTime < DEBOUNCE_DELAY) {
    return;
  }
  
  // 입력 선택 버튼
  if (digitalRead(PIN_BTN_INPUT) == LOW) {
    lastButtonTime = millis();
    currentInput = (InputMode)((currentInput + 1) % INPUT_COUNT);
    setInputMode(currentInput);
    saveSettings();
    needDisplayUpdate = true;
    while (digitalRead(PIN_BTN_INPUT) == LOW);
  }
  
  // 모드 선택 버튼
  if (digitalRead(PIN_BTN_MODE) == LOW) {
    lastButtonTime = millis();
    currentAdjust = (AdjustMode)((currentAdjust + 1) % ADJ_COUNT);
    needDisplayUpdate = true;
    while (digitalRead(PIN_BTN_MODE) == LOW);
  }
  
  // + 버튼
  if (digitalRead(PIN_BTN_PLUS) == LOW) {
    lastButtonTime = millis();
    adjustValue(+2);
    needDisplayUpdate = true;
    while (digitalRead(PIN_BTN_PLUS) == LOW);
  }
  
  // - 버튼
  if (digitalRead(PIN_BTN_MINUS) == LOW) {
    lastButtonTime = millis();
    adjustValue(-2);
    needDisplayUpdate = true;
    while (digitalRead(PIN_BTN_MINUS) == LOW);
  }
}

// ============================================
// 값 포인터 가져오기
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
// 값 조정
// ============================================

void adjustValue(int8_t delta) {
  uint8_t* valuePtr = getValuePtr(currentAdjust);
  
  int16_t newValue = (int16_t)*valuePtr + delta;
  if (newValue < 0) newValue = 0;
  if (newValue > 63) newValue = 63;
  *valuePtr = (uint8_t)newValue;
  
  switch (currentAdjust) {
    case ADJ_BRIGHTNESS: writeRegister(REG_BRIGHTNESS, brightness); break;
    case ADJ_CONTRAST:   writeContrastReg(); break;  // C-TRAP 비트 보존하여 0x00 쓰기
    case ADJ_SATURATION: writeRegister(REG_SATURATION, saturation); break;
    case ADJ_HUE:        writeRegister(REG_HUE, hue); break;
    default: return;
  }
  
  saveSettings();
}

// ============================================
// EEPROM 설정 로드
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
// EEPROM 설정 저장
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
// OLED 화면 업데이트
// ============================================

void updateDisplay() {
  if (!oled_ok) return;  // OLED 없으면 화면 갱신 스킵
  
  display.clearDisplay();
  
  drawHeader();
  drawInputSection();
  drawAdjustSection();
  
  display.display();
}

// ============================================
// 헤더 그리기
// ============================================

void drawHeader() {
  // 상단 라인
  display.drawLine(0, 12, 127, 12, SSD1306_WHITE);
  
  // 타이틀
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 2);
  display.print(F("MULTI-SRC"));
  
  // 현재 조정 모드 표시 (우측 상단)
  display.setCursor(90, 2);
  display.print(F("["));
  display.print(ADJUST_SHORT[currentAdjust]);
  display.print(F("]"));
}

// ============================================
// 입력 섹션 그리기
// ============================================

void drawInputSection() {
  int y = 16;
  
  // 입력 선택 박스들
  for (int i = 0; i < INPUT_COUNT; i++) {
    int x = i * 43;
    int boxWidth = 40;
    int boxHeight = 18;
    
    if (i == currentInput) {
      // 선택된 입력: 채워진 박스
      display.fillRoundRect(x, y, boxWidth, boxHeight, 3, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      // 선택 안된 입력: 빈 박스
      display.drawRoundRect(x, y, boxWidth, boxHeight, 3, SSD1306_WHITE);
      display.setTextColor(SSD1306_WHITE);
    }
    
    // 아이콘 문자
    display.setTextSize(2);
    display.setCursor(x + 14, y + 2);
    display.print(INPUT_ICONS[i]);
  }
  
  // 선택된 입력 이름 (아래에 표시)
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  
  // 이름 중앙 정렬
  int nameLen = strlen(INPUT_NAMES[currentInput]);
  int nameX = (128 - nameLen * 6) / 2;
  display.setCursor(nameX, 36);
  display.print(INPUT_NAMES[currentInput]);
}

// ============================================
// 조정 섹션 그리기
// ============================================

void drawAdjustSection() {
  int y = 46;
  
  // 구분선
  display.drawLine(0, y, 127, y, SSD1306_WHITE);
  
  y += 2;
  
  // 조정 모드 이름
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, y);
  display.print(ADJUST_NAMES[currentAdjust]);
  
  // 현재 값
  uint8_t* valuePtr = getValuePtr(currentAdjust);
  display.setCursor(100, y);
  if (*valuePtr < 10) display.print(F(" "));
  display.print(*valuePtr);
  
  // 프로그레스 바
  y += 9;
  drawProgressBar(0, y, 128, 7, *valuePtr, 63);
}

// ============================================
// 프로그레스 바 그리기
// ============================================

void drawProgressBar(int x, int y, int width, int height, int value, int maxValue) {
  // 외곽선
  display.drawRect(x, y, width, height, SSD1306_WHITE);
  
  // 채움
  int fillWidth = map(value, 0, maxValue, 0, width - 4);
  if (fillWidth > 0) {
    display.fillRect(x + 2, y + 2, fillWidth, height - 4, SSD1306_WHITE);
  }
  
  // 중앙 마커 (기본값 위치)
  int centerX = x + (width / 2);
  display.drawLine(centerX, y, centerX, y + height - 1, SSD1306_WHITE);
}
