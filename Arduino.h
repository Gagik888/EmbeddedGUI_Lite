#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define PROGMEM
#define pgm_read_byte(a) (*(const uint8_t*)(a))
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define HIGH 1
#define LOW 0
#define INPUT 0
#define INPUT_PULLUP 2
#define A0 14
#define constrain(a,lo,hi) ((a)<(lo)?(lo):((a)>(hi)?(hi):(a)))
extern uint32_t g_millis;
inline uint32_t millis(){ return g_millis; }
inline uint32_t micros(){ return g_millis*1000UL; }
inline void pinMode(uint8_t,uint8_t){}
inline int digitalRead(uint8_t){ return 1; }
inline int analogRead(uint8_t){ return 512; }
