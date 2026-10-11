// VEWLIX Power PCB Controller
// Arduino Pro Micro

// === 핀 정의 ===
#define PIN_LED_PWM  9   // 조명 LED MOSFET (PWM)

// 릴레이 + 버튼 LED (공유)
#define PIN_OUT_JAMMA  4   // K1/K2/K3 + BTN_LED1
#define PIN_OUT_TTX3   5   // K4 + BTN_LED2
#define PIN_OUT_NAOMI  6   // K5 + BTN_LED3
#define PIN_OUT_AC1    7   // K6 + BTN_LED4
#define PIN_OUT_AC2    8   // K7 + BTN_LED5

// 래칭 스위치 입력
#define PIN_SW_JAMMA  A2
#define PIN_SW_TTX3   A1
#define PIN_SW_NAOMI  A0
#define PIN_SW_AC1    15
#define PIN_SW_AC2    14

// 밝기 조절 가변저항
#define PIN_LED_CTRL  A3

// 출력 핀 배열
const int outputs[] = {PIN_OUT_JAMMA, PIN_OUT_TTX3, PIN_OUT_NAOMI, PIN_OUT_AC1, PIN_OUT_AC2};

void setup() {
  pinMode(PIN_LED_PWM, OUTPUT);
  pinMode(PIN_OUT_JAMMA, OUTPUT);
  pinMode(PIN_OUT_TTX3, OUTPUT);
  pinMode(PIN_OUT_NAOMI, OUTPUT);
  pinMode(PIN_OUT_AC1, OUTPUT);
  pinMode(PIN_OUT_AC2, OUTPUT);
  
  pinMode(PIN_SW_JAMMA, INPUT_PULLUP);
  pinMode(PIN_SW_TTX3, INPUT_PULLUP);
  pinMode(PIN_SW_NAOMI, INPUT_PULLUP);
  pinMode(PIN_SW_AC1, INPUT_PULLUP);
  pinMode(PIN_SW_AC2, INPUT_PULLUP);
  
  // 초기 상태: 모든 출력 OFF
  for (int i = 0; i < 5; i++) {
    digitalWrite(outputs[i], LOW);
  }
  analogWrite(PIN_LED_PWM, 0);
  
  // Welcome 효과
  welcomeArcadeBoot();
}

int getMaxBrightness() {
  return analogRead(PIN_LED_CTRL) / 4;
}

void welcomeArcadeBoot() {
  // ★ LED1~LED6 (메인 조명, IRLZ44N PWM)만 애니메이션
  // ★ BTN_LED/릴레이 출력 핀은 절대 토글하지 않음 (공유 핀이므로 릴레이 오작동 방지)
  
  int maxBright = getMaxBrightness();
  if (maxBright < 32) maxBright = 32;  // 가변저항이 0이어도 부팅 애니메이션 가시성 확보
  
  // 1. 페이드인 (천천히 밝아짐)
  for (int b = 0; b <= maxBright; b++) {
    analogWrite(PIN_LED_PWM, b);
    delay(8);
  }
  delay(300);
  
  // 2. 호흡 효과 (3회 펄스 - 부드럽게 밝기 변화)
  for (int pulse = 0; pulse < 3; pulse++) {
    // 어두워짐
    for (int b = maxBright; b >= maxBright / 3; b -= 2) {
      analogWrite(PIN_LED_PWM, b);
      delay(8);
    }
    // 다시 밝아짐
    for (int b = maxBright / 3; b <= maxBright; b += 2) {
      analogWrite(PIN_LED_PWM, b);
      delay(8);
    }
  }
  delay(200);
  
  // 3. "READY" 빠른 깜빡임 (3회)
  for (int i = 0; i < 3; i++) {
    analogWrite(PIN_LED_PWM, maxBright / 5);
    delay(120);
    analogWrite(PIN_LED_PWM, maxBright);
    delay(180);
  }
  
  // 4. 최종 밝기로 정착 (이후 loop()에서 가변저항 값으로 계속 갱신)
  analogWrite(PIN_LED_PWM, maxBright);
}

void loop() {
  // 조명 LED 밝기 (가변저항)
  analogWrite(PIN_LED_PWM, getMaxBrightness());
  
  // 래칭 스위치 상태 → 릴레이 + 버튼 LED
  digitalWrite(PIN_OUT_JAMMA, !digitalRead(PIN_SW_JAMMA));
  digitalWrite(PIN_OUT_TTX3,  !digitalRead(PIN_SW_TTX3));
  digitalWrite(PIN_OUT_NAOMI, !digitalRead(PIN_SW_NAOMI));
  digitalWrite(PIN_OUT_AC1,   !digitalRead(PIN_SW_AC1));
  digitalWrite(PIN_OUT_AC2,   !digitalRead(PIN_SW_AC2));
  
  delay(50);
}