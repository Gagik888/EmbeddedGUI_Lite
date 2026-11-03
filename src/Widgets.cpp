#include "Widgets.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#if defined(EGUI_COLOR_RGB565)
  #define EGUI_DEFAULT_ACCENT EGUI_CYAN
#else
  #define EGUI_DEFAULT_ACCENT EGUI_WHITE
#endif

Theme UIWidget::_theme = { EGUI_BLACK, EGUI_WHITE, EGUI_DEFAULT_ACCENT, EGUI_GRAY };

// Вспомогательная функция: ширина/высота текста с учётом текущего шрифта.
static void measure(CanvasBuffer& c, const char* s, uint8_t size, uint16_t& w, uint16_t& h) {
  int16_t x1, y1;
  c.setTextSize(size);
  c.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
}

// ===========================================================================
// Label
// ===========================================================================
Label::Label(int16_t x, int16_t y, int16_t w, const char* text, uint8_t textSize)
  : UIWidget(x, y, w, 8 * (textSize ? textSize : 1)),
    _size(textSize ? textSize : 1), _align(Left), _customColor(false), _color(EGUI_WHITE) {
  _buf[0] = '\0';
  setText(text);
}

void Label::setText(const char* text) {
  if (!text) text = "";
  if (strlen(text) < EGUI_LABEL_CAPACITY && strcmp(_buf, text) == 0) return;  // не изменилось
  strncpy(_buf, text, EGUI_LABEL_CAPACITY - 1);
  _buf[EGUI_LABEL_CAPACITY - 1] = '\0';
  invalidate();
}

void Label::setTextF(const char* fmt, ...) {
  char tmp[EGUI_LABEL_CAPACITY];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  setText(tmp);
}

void Label::setInt(long value) { setTextF("%ld", value); }

void Label::setFloat(float v, uint8_t decimals) {
  if (decimals > 4) decimals = 4;
  bool neg = v < 0;
  if (neg) v = -v;
  uint32_t scale = 1;
  for (uint8_t i = 0; i < decimals; ++i) scale *= 10;
  uint32_t scaled = (uint32_t)(v * scale + 0.5f);
  if (scaled == 0) neg = false;                    // избегаем "-0.0"
  char tmp[EGUI_LABEL_CAPACITY];
  if (decimals)
    snprintf(tmp, sizeof(tmp), "%s%lu.%0*lu", neg ? "-" : "",
             (unsigned long)(scaled / scale), (int)decimals, (unsigned long)(scaled % scale));
  else
    snprintf(tmp, sizeof(tmp), "%s%lu", neg ? "-" : "", (unsigned long)scaled);
  setText(tmp);
}

void Label::draw(CanvasBuffer& c) {
  uint16_t tw, th;
  measure(c, _buf, _size, tw, th);
  int16_t ox = 0;
  if (_align == Center)     ox = (_w - (int16_t)tw) / 2;
  else if (_align == Right) ox = _w - (int16_t)tw;
  c.setTextWrap(false);
  c.setTextColor(_customColor ? _color : theme().foreground);
  c.setCursor(_x + ox, _y);
  c.print(_buf);
}

// ===========================================================================
// Button
// ===========================================================================
Button::Button(int16_t x, int16_t y, int16_t w, int16_t h, const char* text)
  : UIWidget(x, y, w, h), _text(text), _cb(nullptr), _ctx(nullptr),
    _pressed(false), _pressedUntil(0) {}

bool Button::onEvent(InputEvent e) {
  if (e != InputEvent::Select) return false;
  _pressed = true;
  _pressedUntil = millis() + EGUI_BUTTON_FLASH_MS;
  invalidate();
  if (_cb) _cb(_ctx);          // callback может вызвать push()/pop(): они отложенные, это безопасно
  return true;
}

void Button::update(uint32_t nowMs) {
  if (_pressed && (int32_t)(nowMs - _pressedUntil) >= 0) {
    _pressed = false;
    invalidate();
  }
}

void Button::draw(CanvasBuffer& c) {
  const Theme& t = theme();
  const int16_t r = 3;
  if (_pressed) {
    // Нажата: заливка, инвертированный текст
    c.fillRoundRect(_x, _y, _w, _h, r, t.foreground);
  } else {
    c.drawRoundRect(_x, _y, _w, _h, r, _focused ? t.accent : t.foreground);
    if (_focused)               // фокус = двойная рамка (виден и на монохроме)
      c.drawRoundRect(_x + 1, _y + 1, _w - 2, _h - 2, r > 0 ? r - 1 : 0, t.accent);
  }
  uint16_t tw, th;
  measure(c, _text ? _text : "", 1, tw, th);
  c.setTextWrap(false);
  c.setTextColor(_pressed ? t.background : t.foreground);
  c.setCursor(_x + (_w - (int16_t)tw) / 2, _y + (_h - 8) / 2);
  if (_text) c.print(_text);
}

// ===========================================================================
// ProgressBar
// ===========================================================================
ProgressBar::ProgressBar(int16_t x, int16_t y, int16_t w, int16_t h)
  : UIWidget(x, y, w, h), _min(0), _max(100), _target(0), _shown(0), _speed(1.5f),
    _animated(true), _percent(false), _started(false), _last(0) {}

void ProgressBar::setRange(int32_t minV, int32_t maxV) {
  if (maxV <= minV) maxV = minV + 1;
  _min = minV; _max = maxV;
  setValue(_target);
}

void ProgressBar::setValue(int32_t v) {
  if (v < _min) v = _min;
  if (v > _max) v = _max;
  if (v == _target) return;
  _target = v;
  if (!_animated) { _shown = (float)v; invalidate(); }
}

void ProgressBar::update(uint32_t nowMs) {
  if (!_started) { _started = true; _last = nowMs; }
  float dt = (nowMs - _last) / 1000.0f;
  _last = nowMs;
  if (dt > 0.25f) dt = 0.25f;                      // защита от больших пауз
  const float target = (float)_target;
  if (_shown == target) return;
  if (!_animated) { _shown = target; invalidate(); return; }
  const float step = _speed * (float)(_max - _min) * dt;
  const float diff = target - _shown;
  if (fabsf(diff) <= step) _shown = target;
  else _shown += (diff > 0 ? step : -step);
  invalidate();
}

void ProgressBar::draw(CanvasBuffer& c) {
  const Theme& t = theme();
  c.drawRect(_x, _y, _w, _h, t.foreground);
  const int16_t innerW = _w - 4;
  int16_t fw = 0;
  if (innerW > 0) fw = (int16_t)(((_shown - _min) / (float)(_max - _min)) * innerW);
  if (fw > 0 && _h > 4) c.fillRect(_x + 2, _y + 2, fw, _h - 4, t.accent);

  if (_percent) {
    char tmp[6];
    int pct = (int)(((_shown - _min) * 100.0f) / (float)(_max - _min) + 0.5f);
    snprintf(tmp, sizeof(tmp), "%d%%", pct);
    uint16_t tw, th;
    measure(c, tmp, 1, tw, th);
    int16_t tx = _x + (_w - (int16_t)tw) / 2;
    // Если центр закрашен - текст рисуем цветом фона, иначе цветом переднего плана
    c.setTextColor((fw > innerW / 2) ? t.background : t.foreground);
    c.setCursor(tx, _y + (_h - 8) / 2);
    c.print(tmp);
  }
}

// ===========================================================================
// DynamicGraph
// ===========================================================================
DynamicGraph::DynamicGraph(int16_t x, int16_t y, int16_t w, int16_t h, int16_t* storage, uint16_t capacity)
  : UIWidget(x, y, w, h), _data(storage), _cap(capacity ? capacity : 1), _count(0), _head(0),
    _min(0), _max(100), _autoscale(true), _grid(true) {}

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
  const Theme& t = theme();
  c.drawRect(_x, _y, _w, _h, t.foreground);
  const int16_t iw = _w - 2, ih = _h - 2;
  if (iw < 2 || ih < 2) return;
  const int16_t ix = _x + 1, iy = _y + 1;

  if (_grid) {                                     // пунктирная сетка
    for (uint8_t i = 1; i < 4; ++i) {
      int16_t gy = iy + (ih * i) / 4;
      for (int16_t gx = ix; gx < ix + iw; gx += 4) c.drawPixel(gx, gy, t.dim);
    }
  }

  const uint16_t window = (_cap < (uint16_t)iw) ? _cap : (uint16_t)iw;
  const uint16_t n = (_count < window) ? _count : window;
  if (n < 2) return;

  int16_t lo = _min, hi = _max;
  if (_autoscale) {
    lo = hi = sampleAt(n, 0);
    for (uint16_t i = 1; i < n; ++i) {
      int16_t v = sampleAt(n, i);
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
  }
  if (hi <= lo) { hi = lo + 1; }
  const int32_t span = (int32_t)hi - lo;

  int16_t px = 0, py = 0;
  for (uint16_t i = 0; i < n; ++i) {
    int16_t v = sampleAt(n, i);
    int16_t x = ix + (int16_t)(((int32_t)i * (iw - 1)) / (window - 1));
    int32_t yy = iy + ih - 1 - ((int32_t)(v - lo) * (ih - 1)) / span;
    if (yy < iy) yy = iy;
    if (yy > iy + ih - 1) yy = iy + ih - 1;
    if (i > 0) c.drawLine(px, py, x, (int16_t)yy, t.accent);
    px = x; py = (int16_t)yy;
  }
}
