/**
 * @file ViewportManager.h
 * @brief Экраны (Screen) и менеджер переходов между ними (стек).
 *
 *   Главный экран -> Настройки -> График
 *   push()          push()
 *   <-- pop() <--   pop()
 *
 * Screen - контейнер виджетов + жизненный цикл (onEnter/onExit/onUpdate).
 * ViewportManager - стек Screen* фиксированной глубины (EGUI_MAX_SCREEN_DEPTH).
 *
 * Навигация ОТЛОЖЕННАЯ: push()/pop()/replace() только запоминают запрос, а
 * применяются в начале ближайшего update(). Поэтому их безопасно вызывать
 * прямо из callback'а кнопки (экран не "исчезнет" посреди обработки события).
 * Запрос один: если за один цикл вызвать несколько, выполнится последний.
 */
#ifndef EMBEDDEDGUI_VIEWPORTMANAGER_H
#define EMBEDDEDGUI_VIEWPORTMANAGER_H

#include "Widgets.h"

class ViewportManager;

// ===========================================================================
// Screen
// ===========================================================================
class Screen {
public:
  Screen() : _count(0), _focus(-1), _dirty(true), _autoClear(true), _vm(nullptr) {}
  virtual ~Screen() {}

  /// Добавить виджет (хранится ссылка, порядок добавления = порядок отрисовки
  /// и порядок обхода фокуса). @return false, если превышен EGUI_MAX_WIDGETS.
  bool add(UIWidget& w);

  void invalidate()            { _dirty = true; }
  bool needsRedraw() const;
  void setAutoClear(bool on)   { _autoClear = on; }   ///< false - не очищать фон перед кадром
  ViewportManager* manager() const { return _vm; }

  // ---- Хуки жизненного цикла (переопределяйте в наследниках) ----
  virtual void onEnter() {}                            ///< Экран стал активным
  virtual void onExit()  {}                            ///< Экран уходит (push поверх или pop)
  virtual void onUpdate(uint32_t nowMs) { (void)nowMs; } ///< Каждый цикл, не блокировать!
  virtual void onDrawBackground(CanvasBuffer& c) { (void)c; } ///< Под виджетами
  virtual void onDrawOverlay(CanvasBuffer& c)    { (void)c; } ///< Поверх виджетов
  /// По умолчанию: событие -> виджет в фокусе; Up/Down(Left/Right) -> смена фокуса;
  /// Back -> pop(), если есть куда вернуться.
  virtual bool onEvent(InputEvent e);

  bool moveFocus(int8_t dir);                          ///< +1 / -1, с заворачиванием
  UIWidget* focused() const { return (_focus >= 0) ? _items[_focus] : nullptr; }

private:
  friend class ViewportManager;
  void _enter(ViewportManager* vm);
  void _leave();
  void _update(uint32_t nowMs);
  void _draw(CanvasBuffer& c);
  void setFocusIndex(int8_t idx);

  UIWidget* _items[EGUI_MAX_WIDGETS];
  uint8_t _count;
  int8_t  _focus;
  bool    _dirty, _autoClear;
  ViewportManager* _vm;
};

// ===========================================================================
// ViewportManager
// ===========================================================================
class ViewportManager {
public:
  ViewportManager() : _depth(0), _pending(None), _pendingScreen(nullptr) {}

  /// Первый push в пустой стек применяется сразу (это "корневой" экран).
  bool push(Screen& s);
  bool pop();                         ///< false, если остался только корневой экран
  bool replace(Screen& s);            ///< Заменить верхний экран
  void popToRoot();

  Screen*  current() const { return _depth ? _stack[_depth - 1] : nullptr; }
  uint8_t  depth() const   { return _depth; }

  // Вызываются фасадом EmbeddedGUI
  void update(uint32_t nowMs);
  void draw(CanvasBuffer& c);
  bool needsRedraw() const;
  bool dispatch(InputEvent e);

private:
  enum Pending : uint8_t { None, Push, Pop, Replace, Root };
  void applyPending();

  Screen* _stack[EGUI_MAX_SCREEN_DEPTH];
  uint8_t _depth;
  Pending _pending;
  Screen* _pendingScreen;
};

#endif // EMBEDDEDGUI_VIEWPORTMANAGER_H
