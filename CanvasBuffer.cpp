#include "CanvasBuffer.h"
#include <string.h>
#include "EGUI_Font5x7.h"

static inline void applyMask(uint8_t* p, uint8_t m, EGUI_Color c) {
  if (c == EGUI_BLACK)       *p &= (uint8_t)~m;
  else if (c == EGUI_INVERT) *p ^= m;
  else                       *p |= m;
}
static inline void swap16(int16_t& a, int16_t& b) { int16_t t = a; a = b; b = t; }
static inline int16_t abs16(int16_t v) { return v < 0 ? -v : v; }

/// Наибольший dx, для которого dx^2 + dy^2 <= r^2.
static int16_t halfWidth(int16_t r, int16_t dy) {
  const int32_t t = (int32_t)r * r - (int32_t)dy * dy;
  int16_t dx = r;
  while (dx > 0 && (int32_t)dx * dx > t) --dx;
  return dx;
}

CanvasBuffer::CanvasBuffer(uint16_t w, uint16_t h, uint8_t* memory)
  : _w(w), _h(h), _mem(memory), _dirtyMin(0xFF), _dirtyMax(0) {}

// ---------------------------------------------------------------------------
// Базовые операции
// ---------------------------------------------------------------------------
void CanvasBuffer::clear(EGUI_Color c) {
  const size_t n = bufferSize();
  if (c == EGUI_INVERT) { for (size_t i = 0; i < n; ++i) _mem[i] ^= 0xFF; }
  else memset(_mem, c ? 0xFF : 0x00, n);
  markAllDirty();
}

void CanvasBuffer::pixel(int16_t x, int16_t y, EGUI_Color c) {
  if ((uint16_t)x >= _w || (uint16_t)y >= _h) return;
  const uint8_t page = (uint8_t)(y >> 3);
  applyMask(_mem + (size_t)page * _w + x, (uint8_t)(1u << (y & 7)), c);
  markPages(page, page);
}

bool CanvasBuffer::getPixel(int16_t x, int16_t y) const {
  if ((uint16_t)x >= _w || (uint16_t)y >= _h) return false;
  return (_mem[(size_t)(y >> 3) * _w + x] >> (y & 7)) & 1;
}

// Главная «рабочая лошадка»: заливка прямоугольника масками по страницам.
void CanvasBuffer::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, EGUI_Color c) {
  if (w <= 0 || h <= 0) return;
  int16_t x1 = x + w, y1 = y + h;                 // правая/нижняя границы (исключительно)
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x1 > (int16_t)_w) x1 = _w;
  if (y1 > (int16_t)_h) y1 = _h;
  if (x >= x1 || y >= y1) return;

  const uint8_t p0 = (uint8_t)(y >> 3), p1 = (uint8_t)((y1 - 1) >> 3);
  markPages(p0, p1);
  const uint16_t n = (uint16_t)(x1 - x);
  for (uint8_t p = p0; p <= p1; ++p) {
    uint8_t mask = 0xFF;
    if (p == p0) mask &= (uint8_t)(0xFF << (y & 7));
    if (p == p1) mask &= (uint8_t)(0xFF >> (7 - ((y1 - 1) & 7)));
    uint8_t* d = _mem + (size_t)p * _w + x;
    if (c == EGUI_BLACK)       { const uint8_t m = (uint8_t)~mask; for (uint16_t i = 0; i < n; ++i) d[i] &= m; }
    else if (c == EGUI_INVERT) { for (uint16_t i = 0; i < n; ++i) d[i] ^= mask; }
    else                       { for (uint16_t i = 0; i < n; ++i) d[i] |= mask; }
  }
}

void CanvasBuffer::hLine(int16_t x, int16_t y, int16_t w, EGUI_Color c) { fillRect(x, y, w, 1, c); }
void CanvasBuffer::vLine(int16_t x, int16_t y, int16_t h, EGUI_Color c) { fillRect(x, y, 1, h, c); }

void CanvasBuffer::line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, EGUI_Color c) {
  if (y0 == y1) { if (x0 > x1) swap16(x0, x1); hLine(x0, y0, x1 - x0 + 1, c); return; }
  if (x0 == x1) { if (y0 > y1) swap16(y0, y1); vLine(x0, y0, y1 - y0 + 1, c); return; }
  const bool steep = abs16(y1 - y0) > abs16(x1 - x0);
  if (steep) { swap16(x0, y0); swap16(x1, y1); }
  if (x0 > x1) { swap16(x0, x1); swap16(y0, y1); }
  const int16_t dx = x1 - x0, dy = abs16(y1 - y0);
  const int16_t ystep = (y0 < y1) ? 1 : -1;
  int16_t err = dx / 2;
  for (; x0 <= x1; ++x0) {
    if (steep) pixel(y0, x0, c); else pixel(x0, y0, c);
    err -= dy;
    if (err < 0) { y0 += ystep; err += dx; }
  }
}

void CanvasBuffer::rect(int16_t x, int16_t y, int16_t w, int16_t h, EGUI_Color c) {
  if (w <= 0 || h <= 0) return;
  hLine(x, y, w, c);
  if (h > 1) hLine(x, y + h - 1, w, c);
  if (h > 2) {                                    // боковые без углов: корректно для INVERT
    vLine(x, y + 1, h - 2, c);
    if (w > 1) vLine(x + w - 1, y + 1, h - 2, c);
  }
}

// ---------------------------------------------------------------------------
// Окружности и скруглённые прямоугольники
// ---------------------------------------------------------------------------
// corners: 1 = верх-лево, 2 = верх-право, 4 = низ-право, 8 = низ-лево.
// Крайние точки на осях НЕ рисуются (их закрывают прямые рёбра).
void CanvasBuffer::cornerArcs(int16_t cx, int16_t cy, int16_t r, uint8_t corners, EGUI_Color c) {
  int16_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
  while (x < y) {
    if (f >= 0) { --y; ddy += 2; f += ddy; }
    ++x; ddx += 2; f += ddx;
    if (corners & 4) { pixel(cx + x, cy + y, c); if (x != y) pixel(cx + y, cy + x, c); }
    if (corners & 2) { pixel(cx + x, cy - y, c); if (x != y) pixel(cx + y, cy - x, c); }
    if (corners & 8) { pixel(cx - x, cy + y, c); if (x != y) pixel(cx - y, cy + x, c); }
    if (corners & 1) { pixel(cx - x, cy - y, c); if (x != y) pixel(cx - y, cy - x, c); }
  }
}

void CanvasBuffer::circle(int16_t cx, int16_t cy, int16_t r, EGUI_Color c) {
  if (r < 0) return;
  if (r == 0) { pixel(cx, cy, c); return; }
  pixel(cx, cy + r, c); pixel(cx, cy - r, c);
  pixel(cx + r, cy, c); pixel(cx - r, cy, c);
  cornerArcs(cx, cy, r, 0x0F, c);
}

void CanvasBuffer::fillCircle(int16_t cx, int16_t cy, int16_t r, EGUI_Color c) {
  if (r < 0) return;
  const int32_t r2 = (int32_t)r * r;
  int16_t dx = r;
  for (int16_t dy = 0; dy <= r; ++dy) {
    while (dx > 0 && (int32_t)dx * dx + (int32_t)dy * dy > r2) --dx;
    fillRect(cx - dx, cy + dy, 2 * dx + 1, 1, c);
    if (dy) fillRect(cx - dx, cy - dy, 2 * dx + 1, 1, c);
  }
}

void CanvasBuffer::roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, EGUI_Color c) {
  if (w <= 0 || h <= 0) return;
  const int16_t m = ((w < h) ? w : h) / 2;
  if (r > m) r = m;
  if (r <= 0) { rect(x, y, w, h, c); return; }
  hLine(x + r, y, w - 2 * r, c);
  hLine(x + r, y + h - 1, w - 2 * r, c);
  vLine(x, y + r, h - 2 * r, c);
  vLine(x + w - 1, y + r, h - 2 * r, c);
  cornerArcs(x + r,         y + r,         r, 1, c);
  cornerArcs(x + w - r - 1, y + r,         r, 2, c);
  cornerArcs(x + w - r - 1, y + h - r - 1, r, 4, c);
  cornerArcs(x + r,         y + h - r - 1, r, 8, c);
}

void CanvasBuffer::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, EGUI_Color c) {
  if (w <= 0 || h <= 0) return;
  const int16_t m = ((w < h) ? w : h) / 2;
  if (r > m) r = m;
  if (r <= 0) { fillRect(x, y, w, h, c); return; }
  for (int16_t i = 0; i < h; ++i) {
    int16_t dy = 0;
    if (i < r) dy = r - i; else if (i > h - 1 - r) dy = i - (h - 1 - r);
    const int16_t hw = halfWidth(r, dy);
    fillRect(x + r - hw, y + i, w - 2 * r + 2 * hw, 1, c);
  }
}

void CanvasBuffer::bitmap(int16_t x, int16_t y, const uint8_t* bmp, int16_t w, int16_t h, EGUI_Color c) {
  const int16_t bpr = (w + 7) / 8;
  for (int16_t j = 0; j < h; ++j) {
    uint8_t b = 0;
    for (int16_t i = 0; i < w; ++i) {
      if (i & 7) b <<= 1; else b = pgm_read_byte(bmp + (size_t)j * bpr + (i >> 3));
      if (b & 0x80) pixel(x + i, y + j, c);
    }
  }
}

// ---------------------------------------------------------------------------
// Текст
// ---------------------------------------------------------------------------
// Быстрый путь для size==1: колонка глифа сдвигается и накладывается на 2 страницы.
void CanvasBuffer::columnMask(int16_t x, int16_t y, uint8_t mask, EGUI_Color c) {
  if ((uint16_t)x >= _w || !mask) return;
  const int16_t page = y >> 3;                    // арифметический сдвиг: работает и для y < 0
  const uint8_t sh = (uint8_t)(y & 7);
  const uint16_t v = (uint16_t)((uint16_t)mask << sh);
  const int16_t np = pages();
  if (page >= 0 && page < np && (v & 0xFF)) {
    applyMask(_mem + (size_t)page * _w + x, (uint8_t)v, c);
    markPages((uint8_t)page, (uint8_t)page);
  }
  if (sh && page + 1 >= 0 && page + 1 < np && (v >> 8)) {
    applyMask(_mem + (size_t)(page + 1) * _w + x, (uint8_t)(v >> 8), c);
    markPages((uint8_t)(page + 1), (uint8_t)(page + 1));
  }
}

void CanvasBuffer::glyph(int16_t x, int16_t y, char ch, uint8_t size, EGUI_Color c) {
  if (x >= (int16_t)_w || y >= (int16_t)_h || x + 5 * size <= 0 || y + 8 * size <= 0) return;
  uint8_t code = (uint8_t)ch;
  if (code < 32 || code > 126) code = '?';
  const uint8_t* g = EGUI_FONT5X7 + (uint16_t)(code - 32) * 5;
  for (uint8_t i = 0; i < 5; ++i) {
    const uint8_t col = pgm_read_byte(g + i);
    if (!col) continue;
    if (size == 1) columnMask(x + i, y, col, c);
    else for (uint8_t j = 0; j < 8; ++j)
      if (col & (1u << j)) fillRect(x + i * size, y + j * size, size, size, c);
  }
}

int16_t CanvasBuffer::drawChars(int16_t x, int16_t y, const char* s, bool pgm, uint8_t size, EGUI_Color c) {
  if (!size) size = 1;
  int16_t cx = x;
  for (;;) {
    const char ch = pgm ? (char)pgm_read_byte(s) : *s;
    if (!ch) break;
    ++s;
    if (ch == '\n') { cx = x; y += 8 * size; continue; }
    if (ch == '\r') continue;
    glyph(cx, y, ch, size, c);
    cx += 6 * size;
  }
  return cx;
}

int16_t CanvasBuffer::text(int16_t x, int16_t y, const char* s, uint8_t size, EGUI_Color c) {
  return s ? drawChars(x, y, s, false, size, c) : x;
}
int16_t CanvasBuffer::text(int16_t x, int16_t y, const __FlashStringHelper* s, uint8_t size, EGUI_Color c) {
  return s ? drawChars(x, y, (const char*)s, true, size, c) : x;
}
int16_t CanvasBuffer::number(int16_t x, int16_t y, long v, uint8_t size, EGUI_Color c) {
  char b[EGUI_NUM_BUF]; formatNumber(b, v);
  return drawChars(x, y, b, false, size, c);
}
int16_t CanvasBuffer::numberF(int16_t x, int16_t y, float v, uint8_t dec, uint8_t size, EGUI_Color c) {
  char b[EGUI_NUM_BUF]; formatDecimal(b, v, dec);
  return drawChars(x, y, b, false, size, c);
}

uint16_t CanvasBuffer::textWidth(const char* s, uint8_t size) {
  if (!size) size = 1;
  uint16_t n = 0;
  while (s && s[n] && s[n] != '\n') ++n;
  return n ? (uint16_t)(n * 6 * size - size) : 0;
}
uint16_t CanvasBuffer::textWidth(const __FlashStringHelper* s, uint8_t size) {
  if (!size) size = 1;
  const char* p = (const char*)s;
  uint16_t n = 0;
  if (p) for (;;) { const char ch = (char)pgm_read_byte(p + n); if (!ch || ch == '\n') break; ++n; }
  return n ? (uint16_t)(n * 6 * size - size) : 0;
}

// ---------------------------------------------------------------------------
// Форматирование чисел (без printf)
// ---------------------------------------------------------------------------
uint8_t CanvasBuffer::formatNumber(char* buf, long value) {
  const int32_t v = (int32_t)value;
  uint32_t u = (v < 0) ? (0UL - (uint32_t)v) : (uint32_t)v;
  char tmp[11]; uint8_t n = 0, len = 0;
  do { tmp[n++] = (char)('0' + (u % 10)); u /= 10; } while (u);
  if (v < 0) buf[len++] = '-';
  while (n) buf[len++] = tmp[--n];
  buf[len] = '\0';
  return len;
}

uint8_t CanvasBuffer::formatDecimal(char* buf, float v, uint8_t dec) {
  if (v != v) { memcpy(buf, "nan", 4); return 3; }
  if (dec > 4) dec = 4;
  bool neg = v < 0;
  if (neg) v = -v;
  uint32_t scale = 1;
  for (uint8_t i = 0; i < dec; ++i) scale *= 10;
  const float sc = v * (float)scale + 0.5f;
  const uint32_t s = (sc >= 4.0e9f) ? 4000000000UL : (uint32_t)sc;
  if (s == 0) neg = false;                        // без "-0.0"
  uint32_t ip = s / scale, fp = s % scale;
  char tmp[11]; uint8_t n = 0, len = 0;
  do { tmp[n++] = (char)('0' + (ip % 10)); ip /= 10; } while (ip);
  if (neg) buf[len++] = '-';
  while (n) buf[len++] = tmp[--n];
  if (dec) {
    buf[len++] = '.';
    for (uint8_t i = dec; i > 0; --i) {           // дробная часть с ведущими нулями
      uint32_t p = 1;
      for (uint8_t k = 1; k < i; ++k) p *= 10;
      buf[len++] = (char)('0' + (fp / p) % 10);
    }
  }
  buf[len] = '\0';
  return len;
}
