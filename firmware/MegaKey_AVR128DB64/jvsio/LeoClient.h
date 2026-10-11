// Copyright 2020 Takashi Toyoshima <toyoshim@gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__LeoClient_H__)
#define __LeoClient_H__
#define TCCRA_REG TCCR1A
#define TCCRB_REG TCCR1B
#define OCRA_REG OCR1A

#include "BaseClient.h"

// Data+: 0 PD2 ( D0/RXD1 )
// Data-: 2 PD11( D2/SDA )
using LeoDataClient = BaseDataClient<0, 2, 2, 1, 0x0b, 0x0b>;

// Serial = USB CDC serial
// Serial1 = hardware serial port on PD2,PD3
template<> HardwareSerial& LeoDataClient::GetSerial() { return Serial1; }

// Sense: 3 (PWM OC1A - RC LPF of 100nF, 100Ω is needed to generate 2.5V)
using LeoSenseClient = BaseSenseClient<3, 0b01000000, 0b01000000, 1>;

// LED Ready: LED_BUILTIN
using LeoLedClient = BaseLedClient<LED_BUILTIN>;
#endif  // !defined(__LeoClient_H__)