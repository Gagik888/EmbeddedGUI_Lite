#include "CanvasBuffer.h"
#include <string.h>

CanvasBuffer::CanvasBuffer(uint16_t w, uint16_t h, uint8_t* memory)
  : Adafruit_GFX(w, h), _w(w), _h(h), _mem(memory),
    _dirtyTop((int16_t)h), _dirtyBottom(-1) {}

uint16_t CanvasBuffer::bytesPerRow() const {
#if defined(EGUI_COLOR_RGB565)
  return (uint16_t)(_w * 2);
#else
  return (uint16_t)((_w + 7) / 8);
#endif
}

void CanvasBuffer::drawPixel(int16_t x, int16_t y, uint16_t color) {
  // Отрицательные значения int16_t превращаются в большие uint16_t,
  // поэтому одно сравнение отсекает обе границы.
  if ((uint16_t)x >= _w || (uint16_t)y >= _h) return;
#if defined(EGUI_COLOR_RGB565)
  reinterpret_cast<uint16_t*>(_mem)[(size_t)y * _w + x] = color;
#else
  uint8_t* p = _mem + (size_t)y * ((_w + 7) / 8) + (x >> 3);
  const uint8_t m = (uint8_t)(0x80 >> (x & 7));
  if (color) *p |= m; else *p &= (uint8_t)~m;
#endif
  markRow(y);
}

uint16_t CanvasBuffer::getPixel(int16_t x, int16_t y) const {
  if ((uint16_t)x >= _w || (uint16_t)y >= _h) return 0;
#if defined(EGUI_COLOR_RGB565)
  return reinterpret_cast<uint16_t*>(_mem)[(size_t)y * _w + x];
#else
  return (_mem[(size_t)y * ((_w + 7) / 8) + (x >> 3)] >> (7 - (x & 7))) & 1;
#endif
}

void CanvasBuffer::fillScreen(uint16_t color) {
#if defined(EGUI_COLOR_RGB565)
  uint16_t* p = reinterpret_cast<uint16_t*>(_mem);
  const size_t n = (size_t)_w * _h;
  for (size_t i = 0; i < n; ++i) p[i] = color;
#else
  memset(_mem, color ? 0xFF : 0x00, bufferSize());
#endif
  markAllDirty();
}

void CanvasBuffer::markAllDirty() { _dirtyTop = 0; _dirtyBottom = (int16_t)(_h - 1); }
void CanvasBuffer::clearDirty()   { _dirtyTop = (int16_t)_h; _dirtyBottom = -1; }
