#include "Widgets.h"
#include <Arduino.h>
#include <string.h>

// ===========================================================================
// Label
// ===========================================================================
Label::Label(int16_t x, int16_t y, int16_t w, const char* text, uint8_t textSize)
  : UIWidget(x, y, w, 8 * (textSize ? textSize : 1)),
    _size(textSize ? textSize : 1), _align(Left), _color(EGUI_WHITE) {
  _buf[0] = '\0';
  assign(text, false);
}

Label::Label(int16_t x, int16_t y, int16_t w, const __FlashStringHelper* text, uint8_t textSize)
  : UIWidget(x, y, w, 8 * (textSize ? textSize : 1)),
    _size(textSize ? textSize : 1), _align(Left), _color(EGUI_WHITE) {
  _buf[0] = '\0';
  assign((const char*)text, true);
}

// Копирует строку и заодно сравнивает: dirty ставится только при реальном изменении.
void Label::assign(const char* s, bool pgm) {
  if (!s) s = "";
  bool changed = false;
  uint8_t i = 0;
  for (; i < EGUI_LABEL_CAPACITY - 1; ++i) {
    const char ch = pgm ? (char)pgm_read_byte(s + i) : s[i];
    if (_buf[i] != ch) { changed = true; _buf[i] = ch; }
    if (!ch) break;
  }
  if (i == EGUI_LABEL_CAPACITY - 1) {            // обрезано
    if (_buf[i] != '\0') { changed = true; _buf[i] = '\0'; }
  }
  if (changed) invalidate();
}

void Label::setText(const char* text)                 { assign(text, false); }
void Label::setText(const __FlashStringHelper* text)  { assign((const char*)text, true); }

void Label::setInt(long value) {
  char tmp[EGUI_NUM_BUF];
  CanvasBuffer::formatNumber(tmp, value);
  assign(tmp, false);
}

void Label::setFloat(float v, uint8_t decimals) {
  char tmp[EGUI_NUM_BUF];
  CanvasBuffer::formatDecimal(tmp, v, decimals);
  assign(tmp, false);
}

void Label::draw(CanvasBuffer& c) {
  int16_t ox = 0;
  if (_align != Left) {
    const int16_t tw = (int16_t)CanvasBuffer::textWidth(_buf, _size);
    ox = (_align == Center) ? (_w - tw) / 2 : (_w - tw);
  }
  c.text(_x + ox, _y, _buf, _size, _color);
}

// ===========================================================================
// Button
// ===========================================================================
Button::Button(int16_t x, int16_t y, int16_t w, int16_t h, const char* text)
  : UIWidget(x, y, w, h), _text(text), _cb(nullptr), _ctx(nullptr),
    _pressedUntil(0), _pressed(0), _pgm(0) {}

Button::Button(int16_t x, int16_t y, int16_t w, int16_t h, const __FlashStringHelper* text)
  : UIWidget(x, y, w, h), _text((const char*)text), _cb(nullptr), _ctx(nullptr),
    _pressedUntil(0), _pressed(0), _pgm(1) {}

bool Button::onEvent(InputEvent e) {
  if (e != InputEvent::Select) return false;
  _pressed = 1;
  _pressedUntil = millis() + EGUI_BUTTON_FLASH_MS;
  invalidate();
  if (_cb) _cb(_ctx);              // callback может вызвать push()/pop(): они отложенные
  return true;
}

void Button::update(uint32_t nowMs) {
  if (_pressed && (int32_t)(nowMs - _pressedUntil) >= 0) { _pressed = 0; invalidate(); }
}

void Button::draw(CanvasBuffer& c) {
  const int16_t r = (_h >= 12) ? 3 : 1;
  if (_pressed) {
    c.fillRoundRect(_x, _y, _w, _h, r, EGUI_WHITE);
  } else {
    c.roundRect(_x, _y, _w, _h, r, EGUI_WHITE);
    if (_focused) c.roundRect(_x + 1, _y + 1, _w - 2, _h - 2, r - 1, EGUI_WHITE);  // фокус = двойная рамка
  }
  if (!_text) return;
  const EGUI_Color tc = _pressed ? EGUI_BLACK : EGUI_WHITE;
  if (_pgm) {
    const __FlashStringHelper* t = (const __FlashStringHelper*)_text;
    const int16_t tw = (int16_t)CanvasBuffer::textWidth(t, 1);
    c.text(_x + (_w - tw) / 2, _y + (_h - 8) / 2, t, 1, tc);
  } else {
    const int16_t tw = (int16_t)CanvasBuffer::textWidth(_text, 1);
    c.text(_x + (_w - tw) / 2, _y + (_h - 8) / 2, _text, 1, tc);
  }
}

// ===========================================================================
// ProgressBar (целочисленная математика: без float, быстро и компактно на AVR)
// ===========================================================================
static uint16_t fraction16(int16_t v, int16_t mn, int16_t mx) {
  const int32_t span = (int32_t)mx - mn;
  if (span <= 0) return 0;
  int32_t off = (int32_t)v - mn;
  if (off < 0) off = 0;
  if (off > span) off = span;
  return (uint16_t)(((uint32_t)off * 65535UL) / (uint32_t)span);
}

ProgressBar::ProgressBar(int16_t x, int16_t y, int16_t w, int16_t h)
  : UIWidget(x, y, w, h), _min(0), _max(100), _target(0), _target16(0), _shown16(0),
    _last(0), _smooth(1), _percent(0), _started(0) {}

void ProgressBar::setRange(int16_t minV, int16_t maxV) {
  if (maxV <= minV) maxV = minV + 1;
  _min = minV; _max = maxV;
  if (_target < _min) _target = _min;
  if (_target > _max) _target = _max;
  _target16 = fraction16(_target, _min, _max);
  if (!_smooth) _shown16 = _target16;
  invalidate();
}

void ProgressBar::setValue(int16_t v) {
  if (v < _min) v = _min;
  if (v > _max) v = _max;
  if (v == _target) return;
  _target = v;
  _target16 = fraction16(v, _min, _max);
  if (!_smooth) _shown16 = _target16;
  invalidate();
}

void ProgressBar::update(uint32_t nowMs) {
  if (!_started) { _started = 1; _last = nowMs; return; }
  uint32_t dt = nowMs - _last;
  _last = nowMs;
  if (_shown16 == _target16) return;
  if (!_smooth) { _shown16 = _target16; invalidate(); return; }
  if (dt > EGUI_PROGRESS_SMOOTH_MS) dt = EGUI_PROGRESS_SMOOTH_MS;
  // Экспоненциальное сглаживание: за SMOOTH_MS проходит всю оставшуюся дистанцию.
  const int32_t diff = (int32_t)_target16 - (int32_t)_shown16;
  int32_t step = (diff * (int32_t)dt) / (int32_t)EGUI_PROGRESS_SMOOTH_MS;
  if (step == 0) step = (diff > 0) ? 1 : -1;
  if ((diff > 0 && step >= diff) || (diff < 0 && step <= diff)) _shown16 = _target16;
  else _shown16 = (uint16_t)((int32_t)_shown16 + step);
  invalidate();
}

void ProgressBar::draw(CanvasBuffer& c) {
  c.rect(_x, _y, _w, _h, EGUI_WHITE);
  const int16_t innerW = _w - 4;
  if (innerW > 0 && _h > 4) {
    const int16_t fw = (int16_t)(((uint32_t)_shown16 * (uint32_t)innerW + 32768UL) >> 16);
    if (fw > 0) c.fillRect(_x + 2, _y + 2, fw, _h - 4, EGUI_WHITE);
  }
  if (_percent) {
    char tmp[6];
    const uint8_t n = CanvasBuffer::formatNumber(tmp, (long)(((uint32_t)_shown16 * 100UL + 32768UL) >> 16));
    tmp[n] = '%'; tmp[n + 1] = '\0';
    const int16_t tw = (int16_t)CanvasBuffer::textWidth(tmp, 1);
    // INVERT: на залитой части текст чёрный, на пустой - белый
    c.text(_x + (_w - tw) / 2, _y + (_h - 8) / 2, tmp, 1, EGUI_INVERT);
  }
}

// ===========================================================================
// DynamicGraph
// ===========================================================================
DynamicGraph::DynamicGraph(int16_t x, int16_t y, int16_t w, int16_t h, int16_t* storage, uint16_t capacity)
  : UIWidget(x, y, w, h), _data(storage), _cap(capacity ? capacity : 1), _count(0), _head(0),
    _min(0), _max(100), _autoscale(1), _grid(1) {}

void DynamicGraph::pushSample(int16_t v) {
  _data[_head] = v;
  _head = (uint16_t)((_head + 1) % _cap);
  if (_count < _cap) ++_count;
  invalidate();
}

void DynamicGraph::clear() { _count = 0; _head = 0; invalidate(); }

int16_t DynamicGraph::sampleAt(uint16_t n, uint16_t i) const {
  int32_t idx = (int32_t)_head - (int32_t)n + (int32_t)i;
  while (idx < 0) idx += _cap;
  return _data[idx];
}

void DynamicGraph::draw(CanvasBuffer& c) {
  c.rect(_x, _y, _w, _h, EGUI_WHITE);
  const int16_t iw = _w - 2, ih = _h - 2;
  if (iw < 2 || ih < 2) return;
  const int16_t ix = _x + 1, iy = _y + 1;

  if (_grid) {                                     // пунктирная сетка
    for (uint8_t i = 1; i < 4; ++i) {
      const int16_t gy = iy + (ih * i) / 4;
      for (int16_t gx = ix; gx < ix + iw; gx += 4) c.pixel(gx, gy, EGUI_WHITE);
    }
  }

  const uint16_t window = (_cap < (uint16_t)iw) ? _cap : (uint16_t)iw;
  const uint16_t n = (_count < window) ? _count : window;
  if (n < 2) return;

  int16_t lo = _min, hi = _max;
  if (_autoscale) {
    lo = hi = sampleAt(n, 0);
    for (uint16_t i = 1; i < n; ++i) {
      const int16_t v = sampleAt(n, i);
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
  }
  if (hi <= lo) hi = lo + 1;
  const int32_t span = (int32_t)hi - lo;

  int16_t px = 0, py = 0;
  for (uint16_t i = 0; i < n; ++i) {
    const int16_t v = sampleAt(n, i);
    const int16_t x = ix + (int16_t)(((int32_t)i * (iw - 1)) / (window - 1));
    int32_t yy = iy + ih - 1 - ((int32_t)(v - lo) * (ih - 1)) / span;
    if (yy < iy) yy = iy;
    if (yy > iy + ih - 1) yy = iy + ih - 1;
    if (i > 0) c.line(px, py, x, (int16_t)yy, EGUI_WHITE);
    px = x; py = (int16_t)yy;
  }
}
