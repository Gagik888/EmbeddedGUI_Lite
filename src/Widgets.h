/**
 * @file Widgets.h
 * @brief Базовые ООП-компоненты интерфейса.
 *
 *   UIWidget (abstract)
 *    |- Label         - текст (внутренний статический буфер)
 *    |- Button        - кнопка с callback'ом
 *    |- ProgressBar   - полоса прогресса с плавной анимацией
 *    `- DynamicGraph  - график реального времени (кольцевой буфер)
 *
 * Правила:
 *  - Виджеты НЕ владеют памятью экрана: их создаёт пользователь (глобально
 *    или как члены Screen), а Screen хранит лишь ссылки.
 *  - Никаких delay(): анимации считаются по millis() в update(nowMs).
 *  - Изменение состояния => виджет ставит флаг dirty => Screen перерисуется.
 */
#ifndef EMBEDDEDGUI_WIDGETS_H
#define EMBEDDEDGUI_WIDGETS_H

#include "EmbeddedGUI_Config.h"
#include "CanvasBuffer.h"

/// События ввода (абстракция над кнопками/энкодером/тачем).
enum class InputEvent : uint8_t { None = 0, Up, Down, Left, Right, Select, Back };

/// Callback без std::function (никакой кучи): функция + пользовательский контекст.
typedef void (*EGUI_Callback)(void* ctx);

/// Цветовая тема, общая для всех виджетов.
struct Theme {
  EGUI_Color background;
  EGUI_Color foreground;
  EGUI_Color accent;      ///< Заливка прогресса, линия графика, рамка фокуса
  EGUI_Color dim;         ///< Сетка графика и пр.
};

// ===========================================================================
// UIWidget
// ===========================================================================
class UIWidget {
public:
  UIWidget(int16_t x, int16_t y, int16_t w, int16_t h)
    : _x(x), _y(y), _w(w), _h(h), _visible(true), _focused(false), _dirty(true) {}
  virtual ~UIWidget() {}

  /// Вызывается каждый цикл. Здесь живёт логика анимаций. НЕ блокировать!
  virtual void update(uint32_t nowMs) { (void)nowMs; }
  /// Отрисовка в back buffer. Вызывается только когда экран перерисовывается.
  virtual void draw(CanvasBuffer& canvas) = 0;
  /// @return true, если событие "съедено" виджетом.
  virtual bool onEvent(InputEvent e) { (void)e; return false; }
  virtual bool isFocusable() const { return false; }

  // Геометрия / состояние
  void setPosition(int16_t x, int16_t y) { _x = x; _y = y; invalidate(); }
  void setSize(int16_t w, int16_t h)     { _w = w; _h = h; invalidate(); }
  void setVisible(bool v)  { if (_visible != v) { _visible = v; invalidate(); } }
  void setFocused(bool f)  { if (_focused != f) { _focused = f; invalidate(); } }
  int16_t x() const { return _x; }
  int16_t y() const { return _y; }
  int16_t width()  const { return _w; }
  int16_t height() const { return _h; }
  bool isVisible() const { return _visible; }
  bool isFocused() const { return _focused; }

  // Dirty-флаг
  void invalidate()      { _dirty = true; }
  bool isDirty() const   { return _dirty; }
  void clearDirty()      { _dirty = false; }

  /// Глобальная тема (можно менять в setup()).
  static Theme& theme() { return _theme; }

protected:
  int16_t _x, _y, _w, _h;
  bool _visible, _focused, _dirty;
private:
  static Theme _theme;
};

// ===========================================================================
// Label
// ===========================================================================
class Label : public UIWidget {
public:
  enum Align : uint8_t { Left, Center, Right };

  /// @param w ширина области (нужна для выравнивания); высота = 8*textSize
  Label(int16_t x, int16_t y, int16_t w, const char* text = "", uint8_t textSize = 1);

  void setText(const char* text);                    ///< Копирует (обрезает до EGUI_LABEL_CAPACITY-1)
  void setTextF(const char* fmt, ...);               ///< printf-стиль (на AVR нет %f - см. setFloat)
  void setInt(long value);
  void setFloat(float value, uint8_t decimals = 1);  ///< Работает и на AVR, decimals <= 4
  void setAlign(Align a)      { if (_align != a) { _align = a; invalidate(); } }
  void setTextSize(uint8_t s) { _size = s ? s : 1; _h = 8 * _size; invalidate(); }
  void setColor(EGUI_Color c) { _color = c; _customColor = true; invalidate(); }
  const char* text() const    { return _buf; }

  void draw(CanvasBuffer& canvas) override;

private:
  char _buf[EGUI_LABEL_CAPACITY];
  uint8_t _size;
  Align _align;
  bool _customColor;
  EGUI_Color _color;
};

// ===========================================================================
// Button
// ===========================================================================
class Button : public UIWidget {
public:
  /// @param text указатель на строку, которая должна жить дольше кнопки (литерал/PROGMEM нельзя)
  Button(int16_t x, int16_t y, int16_t w, int16_t h, const char* text);

  void setText(const char* text) { _text = text; invalidate(); }
  void setOnClick(EGUI_Callback cb, void* ctx = nullptr) { _cb = cb; _ctx = ctx; }

  bool isFocusable() const override { return true; }
  bool onEvent(InputEvent e) override;     ///< Select => click
  void update(uint32_t nowMs) override;    ///< Неблокирующий "отпуск" кнопки
  void draw(CanvasBuffer& canvas) override;

private:
  const char* _text;
  EGUI_Callback _cb;
  void* _ctx;
  bool _pressed;
  uint32_t _pressedUntil;
};

// ===========================================================================
// ProgressBar
// ===========================================================================
class ProgressBar : public UIWidget {
public:
  ProgressBar(int16_t x, int16_t y, int16_t w, int16_t h);

  void setRange(int32_t minV, int32_t maxV);          ///< По умолчанию 0..100
  void setValue(int32_t v);
  int32_t value() const { return _target; }
  /// Плавная анимация: speed = доля диапазона в секунду (1.0 => за 1 с на весь диапазон).
  void setAnimated(bool on, float speed = 1.5f) { _animated = on; _speed = speed; }
  void showPercent(bool on) { _percent = on; invalidate(); }  ///< Требует h >= 11 для text size 1

  void update(uint32_t nowMs) override;
  void draw(CanvasBuffer& canvas) override;

private:
  int32_t _min, _max, _target;
  float _shown, _speed;
  bool _animated, _percent, _started;
  uint32_t _last;
};

// ===========================================================================
// DynamicGraph
// ===========================================================================
class DynamicGraph : public UIWidget {
public:
  /**
   * @param storage  внешний массив int16_t[capacity] (кольцевой буфер)
   * @param capacity число хранимых отсчётов
   * Удобнее использовать шаблон DynamicGraphT<N>, который содержит массив сам.
   */
  DynamicGraph(int16_t x, int16_t y, int16_t w, int16_t h, int16_t* storage, uint16_t capacity);

  void pushSample(int16_t v);                         ///< O(1), без блокировок
  void clear();
  void setRange(int16_t minV, int16_t maxV)  { _min = minV; _max = maxV; _autoscale = false; invalidate(); }
  void setAutoScale(bool on)                 { _autoscale = on; invalidate(); }
  void showGrid(bool on)                     { _grid = on; invalidate(); }
  uint16_t count() const { return _count; }

  void draw(CanvasBuffer& canvas) override;

private:
  int16_t sampleAt(uint16_t n, uint16_t i) const;     ///< i-й из последних n (0 = самый старый)

  int16_t* _data;
  uint16_t _cap, _count, _head;
  int16_t _min, _max;
  bool _autoscale, _grid;
};

/// DynamicGraph с собственным статическим массивом на N отсчётов.
template <uint16_t N>
class DynamicGraphT : public DynamicGraph {
public:
  DynamicGraphT(int16_t x, int16_t y, int16_t w, int16_t h)
    : DynamicGraph(x, y, w, h, _samples, N) {}
private:
  int16_t _samples[N];
};

#endif // EMBEDDEDGUI_WIDGETS_H
