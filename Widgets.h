/**
 * @file Widgets.h
 * @brief Лёгкие виджеты: Label, Button, ProgressBar, DynamicGraph.
 *
 *   UIWidget (абстрактный)
 *    |- Label         - текст (внутренний буфер EGUI_LABEL_CAPACITY байт)
 *    |- Button        - кнопка с callback'ом
 *    |- ProgressBar   - полоса прогресса с плавной анимацией (только целые числа)
 *    `- DynamicGraph  - график реального времени (кольцевой буфер)
 *
 * Правила: виджеты создаёт пользователь (глобально / членами Screen); куча не
 * используется. Экран хранит их в связном списке через поле _next (2 байта на виджет).
 * delay() не нужен: анимации считаются по millis() в update(nowMs).
 */
#ifndef EMBEDDEDGUI_WIDGETS_H
#define EMBEDDEDGUI_WIDGETS_H

#include "EmbeddedGUI_Config.h"
#include "CanvasBuffer.h"

/// События ввода (абстракция над кнопками/энкодером).
enum class InputEvent : uint8_t { None = 0, Up, Down, Left, Right, Select, Back };

/// Callback без std::function: функция + пользовательский контекст.
typedef void (*EGUI_Callback)(void* ctx);

class Screen;

class UIWidget {
public:
  UIWidget(int16_t x, int16_t y, int16_t w, int16_t h)
    : _next(nullptr), _x(x), _y(y), _w(w), _h(h), _visible(1), _focused(0), _dirty(1) {}

  virtual void update(uint32_t nowMs) { (void)nowMs; }   ///< Логика анимаций. НЕ блокировать!
  virtual void draw(CanvasBuffer& canvas) = 0;
  virtual bool onEvent(InputEvent e) { (void)e; return false; }  ///< true = событие съедено
  virtual bool isFocusable() const { return false; }

  void setPosition(int16_t x, int16_t y) { _x = x; _y = y; invalidate(); }
  void setSize(int16_t w, int16_t h)     { _w = w; _h = h; invalidate(); }
  void setVisible(bool v) { if ((bool)_visible != v) { _visible = v; invalidate(); } }
  void setFocused(bool f) { if ((bool)_focused != f) { _focused = f; invalidate(); } }
  int16_t x() const { return _x; }
  int16_t y() const { return _y; }
  int16_t width()  const { return _w; }
  int16_t height() const { return _h; }
  bool isVisible() const { return _visible; }
  bool isFocused() const { return _focused; }

  void invalidate()    { _dirty = 1; }
  bool isDirty() const { return _dirty; }
  void clearDirty()    { _dirty = 0; }

protected:
  ~UIWidget() {}
  UIWidget* _next;
  int16_t _x, _y, _w, _h;
  uint8_t _visible : 1, _focused : 1, _dirty : 1;
private:
  friend class Screen;
};

// ===========================================================================
class Label : public UIWidget {
public:
  enum Align : uint8_t { Left, Center, Right };

  /// @param w ширина области (для выравнивания); высота = 8*textSize
  Label(int16_t x, int16_t y, int16_t w, const char* text = "", uint8_t textSize = 1);
  Label(int16_t x, int16_t y, int16_t w, const __FlashStringHelper* text, uint8_t textSize = 1);

  void setText(const char* text);                       ///< Копирует (обрезает до CAPACITY-1)
  void setText(const __FlashStringHelper* text);        ///< Копирует из флеша: F("...")
  void setInt(long value);
  void setFloat(float value, uint8_t decimals = 1);     ///< Без printf, работает на AVR
  void setAlign(Align a)      { if (_align != a) { _align = a; invalidate(); } }
  void setTextSize(uint8_t s) { _size = s ? s : 1; _h = 8 * _size; invalidate(); }
  void setColor(EGUI_Color c) { if (_color != c) { _color = c; invalidate(); } }
  const char* text() const    { return _buf; }

  void draw(CanvasBuffer& canvas) override;

private:
  void assign(const char* s, bool pgm);
  char _buf[EGUI_LABEL_CAPACITY];
  uint8_t _size;
  Align _align;
  EGUI_Color _color;
};

// ===========================================================================
class Button : public UIWidget {
public:
  /// Строка должна жить дольше кнопки (литерал или F("...") подходят).
  Button(int16_t x, int16_t y, int16_t w, int16_t h, const char* text);
  Button(int16_t x, int16_t y, int16_t w, int16_t h, const __FlashStringHelper* text);

  void setText(const char* text)                 { _text = text; _pgm = 0; invalidate(); }
  void setText(const __FlashStringHelper* text)  { _text = (const char*)text; _pgm = 1; invalidate(); }
  void setOnClick(EGUI_Callback cb, void* ctx = nullptr) { _cb = cb; _ctx = ctx; }
  void click() { onEvent(InputEvent::Select); }  ///< Программное нажатие

  bool isFocusable() const override { return true; }
  bool onEvent(InputEvent e) override;           ///< Select => click
  void update(uint32_t nowMs) override;
  void draw(CanvasBuffer& canvas) override;

private:
  const char* _text;
  EGUI_Callback _cb;
  void* _ctx;
  uint32_t _pressedUntil;
  uint8_t _pressed : 1, _pgm : 1;
};

// ===========================================================================
class ProgressBar : public UIWidget {
public:
  ProgressBar(int16_t x, int16_t y, int16_t w, int16_t h);

  void setRange(int16_t minV, int16_t maxV);     ///< По умолчанию 0..100
  void setValue(int16_t v);
  int16_t value() const { return _target; }
  void setSmooth(bool on) { _smooth = on; }      ///< Плавное движение к значению (по умолч. вкл.)
  void showPercent(bool on) { _percent = on; invalidate(); }  ///< Нужно h >= 11

  void update(uint32_t nowMs) override;
  void draw(CanvasBuffer& canvas) override;

private:
  int16_t  _min, _max, _target;
  uint16_t _target16, _shown16;                  // доля 0..65535
  uint32_t _last;
  uint8_t  _smooth : 1, _percent : 1, _started : 1;
};

// ===========================================================================
class DynamicGraph : public UIWidget {
public:
  /// storage - внешний int16_t[capacity]; проще использовать DynamicGraphT<N>.
  DynamicGraph(int16_t x, int16_t y, int16_t w, int16_t h, int16_t* storage, uint16_t capacity);

  void pushSample(int16_t v);                    ///< O(1)
  void clear();
  void setRange(int16_t minV, int16_t maxV) { _min = minV; _max = maxV; _autoscale = 0; invalidate(); }
  void setAutoScale(bool on)                { _autoscale = on; invalidate(); }
  void showGrid(bool on)                    { _grid = on; invalidate(); }
  uint16_t count() const { return _count; }

  void draw(CanvasBuffer& canvas) override;

private:
  int16_t sampleAt(uint16_t n, uint16_t i) const;   ///< i-й из последних n (0 = самый старый)
  int16_t* _data;
  uint16_t _cap, _count, _head;
  int16_t _min, _max;
  uint8_t _autoscale : 1, _grid : 1;
};

/// DynamicGraph со своим статическим массивом на N отсчётов (2*N байт).
template <uint16_t N>
class DynamicGraphT : public DynamicGraph {
public:
  DynamicGraphT(int16_t x, int16_t y, int16_t w, int16_t h)
    : DynamicGraph(x, y, w, h, _samples, N) {}
private:
  int16_t _samples[N];
};

#endif // EMBEDDEDGUI_WIDGETS_H
