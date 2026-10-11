# MegaKey — JAMMA / Arcade Controller Firmware Collection

아케이드 시스템(JAMMA · KVM · 콘솔)을 위한 커스텀 하드웨어 펌웨어 모음입니다.

> **개발자**: 안연조 (An Yeon-jo)
> **보드**: AVR128DB64 · ATmega32U4 (Pro Micro) · RP2040 (GP2040-CE) · STM32

---

## 구조

```
MEGAKEY/
├── firmware/              # Arduino / Microcontroller 스케치
│   ├── MegaKey_AVR128DB64/   # ★ 메인 보드 (v4.0, AVR128DB64-I/PT)
│   ├── MegaKey_IO/           # 이전 세대 보드 (Pro Micro 기반 JVS IO)
│   ├── MultiSourceController/ # 멀티소스 → JAMMA 변환기 (CXA2025AS)
│   ├── PowerController/      # 시퀀스 전원 제어기
│   └── IR_SignalAnalyzer/    # IR(NEC) 신호 분석 유틸
│
├── rp2040/                # RP2040 / GP2040-CE 관련
│   ├── addons/               # Serial Input Addon (v2.4)
│   └── firmware/             # 커스텀 보드 정의 (Waveshare RP2040-Zero)
│
└── docs/                  # 회로도 · 핀맵 · 문서
```

---

## 각 모듈

### `firmware/MegaKey_AVR128DB64` — 메인 보드 (v4.0)

AVR128DB64-I/PT 기반 아케이드 컨트롤러 보드.

**하드웨어**
- AVR128DB64 (TQFP-64, 24MHz 내장 OSCHF, 5V)
- 74HC595 × 2 (KVM LED · STATUS · MUX 제어)
- GP2040-CE × 2 (Serial2/Serial3 단방향 전송)
- JVS IO (USART0, TCB1 Sense 신호)
- HDMI 스위치 IR (NEC, TCB0 PWM on PA2)

**컴파일 환경**
- Arduino IDE + [DxCore](https://github.com/SpenceKonde/DxCore)
- Board: AVR DB-series → AVR128DB64 (Reset: GPIO PF6, Clock 24MHz internal)
- Libraries: [JVSIO](https://github.com/twitter/jvsio) · IRremote v4.x

### `firmware/MegaKey_IO` — 이전 세대 보드

ATmega32U4 (Pro Micro) 기반 JVS IO 컨트롤러. `jvsio/` 서브디렉터리에 라이브러리 소스를 포함합니다.

### `firmware/MultiSourceController` — 멀티소스 → JAMMA

CXA2025AS + 74HC4052 + LM1881 컨트롤러. 입력: 컴포지트 · S-Video · VGA 15kHz → 출력: JAMMA RGB. OLED(SSD1306) UI.

### `firmware/PowerController` — 전원 시퀀스 제어기

### `firmware/IR_SignalAnalyzer` — IR 신호 분석 유틸

HDMI 스위치 제어용 NEC IR 신호를 디코딩·분석하는 진단 스케치.

### `rp2040/addons` — GP2040-CE Serial Input Addon (v2.4)

RP2040 제로 보드에서 Serial(UART)로 입력을 받아 Gamepad HID로 중계하는 GP2040-CE 애드온.

> ⚠️ **필수 설정**: Display Addon · PlayerLEDAddon 비활성화 (I2C · WS2812 PIO 충돌 방지)

### `rp2040/firmware/BoardConfig.h` — Waveshare RP2040-Zero 보드 정의

GP2040-CE 커스텀 보드 핀맵.

---

## 라이선스

각 서브모듈의 SPDX 헤더를 따릅니다. 별도 표기가 없는 소스는 MIT입니다.
