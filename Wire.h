#pragma once
#include "Arduino.h"
// Эмулятор SSD1306/SH1106 за интерфейсом TwoWire (буфер передачи 32 байта, как на AVR).
struct OledSim {
  uint8_t ram[8][132]; uint8_t page=0, col=0; int expectArgs=0; uint8_t pendingCmd=0;
  uint32_t dataBytes=0, txCount=0, maxTx=0, overflow=0; bool on=false; bool inverted=false;
  uint8_t contrast=0; uint8_t log[64]; int logn=0;
  void command(uint8_t c){
    if(expectArgs){ expectArgs--; if(pendingCmd==0x81) contrast=c; return; }
    if((c&0xF8)==0xB0){ page=c&7; return; }
    if(c<=0x0F){ col=(col&0xF0)|c; return; }
    if(c>=0x10&&c<=0x1F){ col=(col&0x0F)|((c&0x0F)<<4); return; }
    switch(c){
      case 0x20:case 0x21:case 0x22:case 0x81:case 0x8D:case 0xA8:case 0xD3:case 0xD5:case 0xD9:case 0xDA:case 0xDB:case 0xAD:
        pendingCmd=c; expectArgs=1; if(logn<64)log[logn++]=c; return;
      case 0xAF: on=true; break; case 0xAE: on=false; break;
      case 0xA7: inverted=true; break; case 0xA6: inverted=false; break;
    }
    if(logn<64)log[logn++]=c;
  }
};
extern OledSim g_oled;
class TwoWire {
  uint8_t buf[64]; size_t n=0; uint8_t addr=0;
public:
  void begin(){} void setClock(uint32_t){}
  void beginTransmission(uint8_t a){ addr=a; n=0; }
  size_t write(uint8_t b){ if(n>=32){ g_oled.overflow++; return 0;} buf[n++]=b; return 1; }
  size_t write(const uint8_t* d,size_t l){ size_t k=0; while(k<l && write(d[k]))k++; return k; }
  uint8_t endTransmission(bool=true){
    if(addr!=0x3C) return 2;
    g_oled.txCount++; if(n>g_oled.maxTx)g_oled.maxTx=n;
    if(n>=1){ uint8_t ctl=buf[0];
      if(ctl==0x00) for(size_t i=1;i<n;i++) g_oled.command(buf[i]);
      else if(ctl==0x40) for(size_t i=1;i<n;i++){ if(g_oled.col<132) g_oled.ram[g_oled.page][g_oled.col]=buf[i]; g_oled.col++; g_oled.dataBytes++; }
    }
    return 0;
  }
};
extern TwoWire Wire;
