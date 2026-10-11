// VEWLIX Power PCB Controller
// Arduino Pro Micro

// === PIN DEFINITIONS ===
#define PIN_LED_PWM  9   // lighting LED MOSFET (PWM)

// relay + button LED (shared)
#define PIN_OUT_JAMMA  4   // K1/K2/K3 + BTN_LED1
#define PIN_OUT_TTX3   5   // K4 + BTN_LED2
#define PIN_OUT_NAOMI  6   // K5 + BTN_LED3
#define PIN_OUT_AC1    7   // K6 + BTN_LED4
#define PIN_OUT_AC2    8   // K7 + BTN_LED5

// latching switch inputs
#define PIN_SW_JAMMA  A2
#define PIN_SW_TTX3   A1
#define PIN_SW_NAOMI  A0
#define PIN_SW_AC1    15
#define PIN_SW_AC2    14

// brightness-adjust potentiometer
#define PIN_LED_CTRL  A3

// output pin array
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
  
  // initial state: all outputs OFF
  for (int i = 0; i < 5; i++) {
    digitalWrite(outputs[i], LOW);
  }
  analogWrite(PIN_LED_PWM, 0);
  
  // welcome effect
  welcomeArcadeBoot();
}

int getMaxBrightness() {
  return analogRead(PIN_LED_CTRL) / 4;
}

void welcomeArcadeBoot() {
  // ★ animate only LED1~LED6 (main lighting, IRLZ44N PWM)
  // ★ never toggle BTN_LED/relay output pins (shared pins - prevents relay misoperation)
  
  int maxBright = getMaxBrightness();
  if (maxBright < 32) maxBright = 32;  // ensure boot-animation visibility even if potentiometer is 0
  
  // 1. fade-in (slowly brightens)
  for (int b = 0; b <= maxBright; b++) {
    analogWrite(PIN_LED_PWM, b);
    delay(8);
  }
  delay(300);
  
  // 2. breathing effect (3 pulses - smooth brightness change)
  for (int pulse = 0; pulse < 3; pulse++) {
    // dim
    for (int b = maxBright; b >= maxBright / 3; b -= 2) {
      analogWrite(PIN_LED_PWM, b);
      delay(8);
    }
    // brighten again
    for (int b = maxBright / 3; b <= maxBright; b += 2) {
      analogWrite(PIN_LED_PWM, b);
      delay(8);
    }
  }
  delay(200);
  
  // 3. "READY" fast blink (3 times)
  for (int i = 0; i < 3; i++) {
    analogWrite(PIN_LED_PWM, maxBright / 5);
    delay(120);
    analogWrite(PIN_LED_PWM, maxBright);
    delay(180);
  }
  
  // 4. settle at final brightness (loop() keeps updating from potentiometer value)
  analogWrite(PIN_LED_PWM, maxBright);
}

void loop() {
  // lighting LED brightness (potentiometer)
  analogWrite(PIN_LED_PWM, getMaxBrightness());
  
  // latching switch state -> relay + button LED
  digitalWrite(PIN_OUT_JAMMA, !digitalRead(PIN_SW_JAMMA));
  digitalWrite(PIN_OUT_TTX3,  !digitalRead(PIN_SW_TTX3));
  digitalWrite(PIN_OUT_NAOMI, !digitalRead(PIN_SW_NAOMI));
  digitalWrite(PIN_OUT_AC1,   !digitalRead(PIN_SW_AC1));
  digitalWrite(PIN_OUT_AC2,   !digitalRead(PIN_SW_AC2));
  
  delay(50);
}