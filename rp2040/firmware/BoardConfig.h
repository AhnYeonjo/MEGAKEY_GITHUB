/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: Copyright (c) 2024 OpenStickCommunity (gp2040-ce.info)
 */

#ifndef PICO_BOARD_CONFIG_H_
#define PICO_BOARD_CONFIG_H_

#include "enums.pb.h"
#include "class/hid/hid.h"

#define BOARD_CONFIG_LABEL "Waveshare Zero"

// Main pin mapping Configuration
//                                                  // GP2040 | Xinput | Switch  | PS3/4/5  | Dinput | Arcade |
#define GPIO_PIN_28 GpioAction::NONE     // UP     | UP     | UP      | UP       | UP     | UP     |
#define GPIO_PIN_08 GpioAction::NONE   // DOWN   | DOWN   | DOWN    | DOWN     | DOWN   | DOWN   |
#define GPIO_PIN_17 GpioAction::NONE  // RIGHT  | RIGHT  | RIGHT   | RIGHT    | RIGHT  | RIGHT  |
#define GPIO_PIN_18 GpioAction::NONE   // LEFT   | LEFT   | LEFT    | LEFT     | LEFT   | LEFT   |
#define GPIO_PIN_19 GpioAction::NONE     // B1     | A      | B       | Cross    | 2      | K1     |
#define GPIO_PIN_07 GpioAction::NONE     // B2     | B      | A       | Circle   | 3      | K2     |
#define GPIO_PIN_06 GpioAction::NONE     // R2     | RT     | ZR      | R2       | 8      | K3     |
#define GPIO_PIN_09 GpioAction::NONE     // L2     | LT     | ZL      | L2       | 7      | K4     |
#define GPIO_PIN_03 GpioAction::NONE     // B3     | X      | Y       | Square   | 1      | P1     |
#define GPIO_PIN_04 GpioAction::NONE     // B4     | Y      | X       | Triangle | 4      | P2     |
#define GPIO_PIN_12 GpioAction::NONE     // R1     | RB     | R       | R1       | 6      | P3     |
#define GPIO_PIN_13 GpioAction::NONE     // L1     | LB     | L       | L1       | 5      | P4     |
#define GPIO_PIN_02 GpioAction::NONE     // S1     | Back   | Minus   | Select   | 9      | Coin   |
#define GPIO_PIN_29 GpioAction::BUTTON_PRESS_S2     // S2     | Start  | Plus    | Start    | 10     | Start  |
#define GPIO_PIN_26 GpioAction::NONE     // L3     | LS     | LS      | L3       | 11     | LS     |
#define GPIO_PIN_27 GpioAction::NONE     // R3     | RS     | RS      | R3       | 12     | RS     |
#define GPIO_PIN_14 GpioAction::NONE     // A1     | Guide  | Home    | PS       | 13     | ~      |
#define GPIO_PIN_15 GpioAction::NONE     // A2     | ~      | Capture | ~       5 | 14     | ~      |
#define GPIO_PIN_00 GpioAction::NONE 
#define GPIO_PIN_01 GpioAction::NONE 
#define GPIO_PIN_05 GpioAction::NONE
#define GPIO_PIN_10 GpioAction::NONE 
#define GPIO_PIN_11 GpioAction::NONE

// Keyboard Mapping Configuration

#if PLAYER_NUMBER == 1
    // ===================================
    // === Player 1 Keyboard Mapping ===
    // ===================================
    // 방향키 - 화살표 키
    #define KEY_DPAD_UP     HID_KEY_ARROW_UP
    #define KEY_DPAD_DOWN   HID_KEY_ARROW_DOWN
    #define KEY_DPAD_LEFT   HID_KEY_ARROW_LEFT
    #define KEY_DPAD_RIGHT  HID_KEY_ARROW_RIGHT

    #define KEY_BUTTON_E1   HID_KEY_G  // ANAL UP 버튼
    #define KEY_BUTTON_E2   HID_KEY_B  // ANAL DOWN 버튼
    #define KEY_BUTTON_E3   HID_KEY_V  // ANAL LEFT 버튼
    #define KEY_BUTTON_E4   HID_KEY_N  // ANAL RIGHT 버튼

    // 액션 버튼 - MAME 표준 (A/S/Z/X 등)
    #define KEY_BUTTON_B1   HID_KEY_A  // A 버튼
    #define KEY_BUTTON_B2   HID_KEY_S  // B 버튼
    #define KEY_BUTTON_R2   HID_KEY_D  // RT 버튼
    #define KEY_BUTTON_L2   HID_KEY_F  // LT 버튼
    #define KEY_BUTTON_B3   HID_KEY_Q  // X 버튼
    #define KEY_BUTTON_B4   HID_KEY_W  // Y 버튼
    #define KEY_BUTTON_R1   HID_KEY_E  // RB 버튼
    #define KEY_BUTTON_L1   HID_KEY_R  // LB 버튼
    
    // 시스템 버튼
    #define KEY_BUTTON_S1   HID_KEY_5  // SELECT (코인)
    #define KEY_BUTTON_S2   HID_KEY_1  // START (1P)
    #define KEY_BUTTON_L3   HID_KEY_Z
    #define KEY_BUTTON_R3   HID_KEY_X
    #define KEY_BUTTON_A1   HID_KEY_ESCAPE // HOME (ESC)
    #define KEY_BUTTON_A2   HID_KEY_TAB    // Capture (TAB)

#else // PLAYER_NUMBER == 2
    // ===================================
    // === Player 2 Keyboard Mapping ===
    // ===================================
    // 방향키 - U/J/H/K
    #define KEY_DPAD_UP     HID_KEY_U
    #define KEY_DPAD_DOWN   HID_KEY_J
    #define KEY_DPAD_LEFT   HID_KEY_H
    #define KEY_DPAD_RIGHT  HID_KEY_K

    #define KEY_BUTTON_E1   HID_KEY_O  // ANAL UP 버튼
    #define KEY_BUTTON_E2   HID_KEY_L  // ANAL DOWN 버튼
    #define KEY_BUTTON_E3   HID_KEY_I  // ANAL LEFT 버튼
    #define KEY_BUTTON_E4   HID_KEY_P  // ANAL RIGHT 버튼    

    // 액션 버튼 - 숫자 키패드
    #define KEY_BUTTON_B1   HID_KEY_KEYPAD_4
    #define KEY_BUTTON_B2   HID_KEY_KEYPAD_5
    #define KEY_BUTTON_R2   HID_KEY_KEYPAD_6
    #define KEY_BUTTON_L2   HID_KEY_KEYPAD_3
    #define KEY_BUTTON_B3   HID_KEY_KEYPAD_7
    #define KEY_BUTTON_B4   HID_KEY_KEYPAD_8
    #define KEY_BUTTON_R1   HID_KEY_KEYPAD_9
    #define KEY_BUTTON_L1   HID_KEY_KEYPAD_0
    
    // 시스템 버튼
    #define KEY_BUTTON_S1   HID_KEY_6
    #define KEY_BUTTON_S2   HID_KEY_2
    #define KEY_BUTTON_L3   HID_KEY_KEYPAD_ENTER
    #define KEY_BUTTON_R3   HID_KEY_KEYPAD_DECIMAL
    #define KEY_BUTTON_A1   HID_KEY_ESCAPE
    #define KEY_BUTTON_A2   HID_KEY_TAB
#endif

// 핫키 기능 버튼 (공통)
#define KEY_BUTTON_FN   -1
#endif