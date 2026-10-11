/*
 * Arduino IR Signal Analyzer - 완전 수정 버전
 * 첫 펄스 오버플로우 문제 해결
 */

#define IR_PIN 2
#define LED_PIN 13

// 타이밍 설정
#define MAX_PULSES 100      
#define MIN_GAP 10000       
#define MAX_GAP 100000      
#define SIGNAL_TIMEOUT 200  

// 전역 변수
unsigned int pulses[MAX_PULSES];
byte pulseCount = 0;
unsigned long startTime = 0;
bool receiving = false;
bool newData = false;
int dataOffset = 0;  // 실제 데이터 시작 위치

void setup() {
  Serial.begin(9600);
  
  pinMode(IR_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  
  Serial.println(F("=== IR Analyzer v3.0 ==="));
  Serial.println(F("Ready for signals..."));
  Serial.println();
}

void loop() {
  // IR 신호 읽기
  static int lastState = HIGH;
  static unsigned long lastTime = micros();
  
  int currentState = digitalRead(IR_PIN);
  
  if (currentState != lastState) {
    unsigned long now = micros();
    unsigned long duration = now - lastTime;
    
    if (duration > MIN_GAP && !receiving) {
      receiving = true;
      pulseCount = 0;
      startTime = millis();
      digitalWrite(LED_PIN, HIGH);
    }
    
    if (receiving && pulseCount < MAX_PULSES) {
      pulses[pulseCount++] = (duration > 65535) ? 65535 : duration;
    }
    
    lastTime = now;
    lastState = currentState;
  }
  
  // 타임아웃 체크
  if (receiving && (millis() - startTime > SIGNAL_TIMEOUT)) {
    receiving = false;
    newData = true;
    digitalWrite(LED_PIN, LOW);
  }
  
  // 데이터 처리
  if (newData) {
    analyzeSignal();
    newData = false;
    delay(500);
  }
}

// 신호 분석
void analyzeSignal() {
  Serial.println(F("======================"));
  Serial.print(F("Total Pulses: "));
  Serial.println(pulseCount);
  
  if (pulseCount < 20) {
    Serial.println(F("Signal too short"));
    return;
  }
  
  // 첫 펄스가 오버플로우(65535)면 스킵
  dataOffset = 0;
  if (pulses[0] > 60000) {
    dataOffset = 1;
    Serial.println(F("Note: First pulse overflow, skipping..."));
  }
  
  // 프로토콜 확인 및 디코딩
  if (checkNEC()) {
    decodeNEC();
  }
  else if (checkSamsung()) {
    decodeSamsung();  
  }
  else if (checkSony()) {
    decodeSony();
  }
  else {
    Serial.println(F("Protocol: Unknown"));
    showRawData();
  }
  
  Serial.println();
}

// NEC 프로토콜 확인 (9ms mark, 4.5ms space)
bool checkNEC() {
  int idx = dataOffset;
  
  if ((pulseCount - dataOffset) >= 67 && 
      pulses[idx] > 8000 && pulses[idx] < 10000 &&
      pulses[idx + 1] > 4000 && pulses[idx + 1] < 5000) {
    return true;
  }
  return false;
}

// Samsung 프로토콜 확인
bool checkSamsung() {
  int idx = dataOffset;
  
  if ((pulseCount - dataOffset) >= 60 &&
      pulses[idx] > 4000 && pulses[idx] < 5500 &&
      pulses[idx + 1] > 4000 && pulses[idx + 1] < 5500) {
    return true;
  }
  return false;
}

// Sony 프로토콜 확인
bool checkSony() {
  int idx = dataOffset;
  
  if ((pulseCount - dataOffset) >= 20 &&
      pulses[idx] > 2000 && pulses[idx] < 3000) {
    return true;
  }
  return false;
}

// NEC 디코딩
void decodeNEC() {
  Serial.println(F("Protocol: NEC"));
  
  unsigned long data = 0;
  
  // 헤더 다음부터 32비트 데이터
  for (int i = 0; i < 32; i++) {
    int idx = dataOffset + 2 + (i * 2) + 1;
    if (idx < pulseCount) {
      if (pulses[idx] > 1000) {
        data |= (1UL << i);
      }
    }
  }
  
  // NEC 형식 분석
  byte address = data & 0xFF;
  byte addressInv = (data >> 8) & 0xFF;
  byte command = (data >> 16) & 0xFF;
  byte commandInv = (data >> 24) & 0xFF;
  
  Serial.print(F("Address: 0x"));
  printHex(address);
  
  // 주소 검증
  if ((address ^ addressInv) == 0xFF) {
    Serial.print(F(" [Valid]"));
  } else {
    Serial.print(F(" [Extended]"));
  }
  
  Serial.print(F("  Command: 0x"));
  printHex(command);
  Serial.print(F(" ("));
  Serial.print(command);
  Serial.print(F(")"));
  
  // 명령 검증
  if ((command ^ commandInv) == 0xFF) {
    Serial.print(F(" [Valid]"));
  } else {
    Serial.print(F(" [Extended]"));
  }
  
  Serial.println();
  
  // 전체 32비트 코드
  Serial.print(F("Full Code: 0x"));
  Serial.println(data, HEX);
  
  // 리피트 신호 확인
  if (pulseCount > 68) {
    Serial.println(F("Repeat signal detected"));
  }
}

// Samsung 디코딩
void decodeSamsung() {
  Serial.println(F("Protocol: Samsung"));
  
  unsigned long code = 0;
  
  for (int i = 0; i < 32; i++) {
    int idx = dataOffset + 2 + (i * 2) + 1;
    if (idx < pulseCount && pulses[idx] > 1000) {
      code |= (1UL << i);
    }
  }
  
  Serial.print(F("Code: 0x"));
  Serial.println(code, HEX);
}

// Sony 디코딩
void decodeSony() {
  Serial.println(F("Protocol: Sony"));
  
  unsigned int code = 0;
  int bits = 0;
  
  for (int i = dataOffset + 1; i < pulseCount && bits < 20; i += 2) {
    if (pulses[i] > 1200) {
      code |= (1 << bits);
    }
    bits++;
  }
  
  int command = code & 0x7F;
  int address = (code >> 7) & 0xFF;
  
  Serial.print(F("Address: "));
  Serial.print(address);
  Serial.print(F("  Command: "));
  Serial.println(command);
}

// RAW 데이터 표시
void showRawData() {
  // 타이밍 샘플
  Serial.print(F("Header: "));
  Serial.print(pulses[dataOffset]);
  Serial.print(F(","));
  Serial.print(pulses[dataOffset + 1]);
  Serial.println(F(" us"));
  
  // 바이너리 패턴
  Serial.print(F("Binary: "));
  for (int i = dataOffset + 3; i < pulseCount && i < dataOffset + 67; i += 2) {
    Serial.print(pulses[i] > 1000 ? '1' : '0');
  }
  Serial.println();
  
  // 간단한 해시값 (버튼 구분용)
  unsigned long hash = 0;
  for (int i = dataOffset; i < pulseCount && i < dataOffset + 32; i++) {
    hash = hash * 31 + (pulses[i] / 100);
  }
  Serial.print(F("Button Hash: 0x"));
  Serial.println(hash, HEX);
}

// HEX 출력 헬퍼
void printHex(byte val) {
  if (val < 16) Serial.print('0');
  Serial.print(val, HEX);
}