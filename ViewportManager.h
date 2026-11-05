/**
 * @file ViewportManager.h
 * @brief Экраны (Screen) и стек переходов между ними (ViewportManager).
 *
 *   Главный -> Настройки -> График        push()/pop()
 *
 * Screen хранит виджеты в связном списке (поле _next самих виджетов): ни массивов
 * фиксированного размера, ни лимита на число виджетов. Один виджет - один экран.
 *
 * Навигация ОТЛОЖЕННАЯ: push()/pop()/replace() применяются в начале ближайшего
 * update(), поэтому их безопасно вызывать прямо из callback'а кнопки.
 * Запрос один: если за цикл вызвать несколько - выполнится последний.
 */
#ifndef EMBEDDEDGUI_VIEWPORTMANAGER_H
#define EMBEDDEDGUI_VIEWPORTMANAGER_H

#include "Widgets.h"

class ViewportManager;

class Screen {
public:
  Screen() : _head(nullptr), _focus(nullptr), _vm(nullptr), _dirty(1), _autoClear(1) {}

  /// Добавить виджет (порядок добавления = порядок отрисовки и обхода фокуса).
  void add(UIWidget& w);

  void invalidate()           { _dirty = 1; }
  bool needsRedraw() const;
  void setAutoClear(bool on)  { _autoClear = on; }   ///< false - не очищать фон перед кадром
  ViewportManager* manager() const { return _vm; }

  // ---- Хуки (переопределяйте в наследниках) ----
  virtual void onEnter() {}                              ///< Экран стал активным
  virtual void onExit()  {}                              ///< Экран уходит
  virtual void onUpdate(uint32_t nowMs) { (void)nowMs; } ///< Каждый цикл, не блокировать!
  virtual void onDraw(CanvasBuffer& c) { (void)c; }      ///< Фон, под виджетами
  /// По умолчанию: событие -> виджет в фокусе; Up/Left, Down/Right -> смена фокуса;
  /// Back -> возврат на предыдущий экран.
  virtual bool onEvent(InputEvent e);

  bool moveFocus(int8_t dir);                            ///< +1 / -1 с заворачиванием
  UIWidget* focused() const { return _focus; }

protected:
  ~Screen() {}

private:
  friend class ViewportManager;
  void _enter(ViewportManager* vm);
  void _leave() { onExit(); }
  void _update(uint32_t nowMs);
  void _draw(CanvasBuffer& c);
  void setFocus(UIWidget* w);

  UIWidget* _head;
  UIWidget* _focus;
  ViewportManager* _vm;
  uint8_t _dirty : 1, _autoClear : 1;
};

class ViewportManager {
public:
  ViewportManager() : _pendingScreen(nullptr), _depth(0), _pending(None) {}

  bool push(Screen& s);          ///< В пустой стек - сразу (корневой экран)
  bool pop();                    ///< false, если остался только корневой
  bool replace(Screen& s);
  void popToRoot();

  Screen* current() const { return _depth ? _stack[_depth - 1] : nullptr; }
  uint8_t depth() const   { return _depth; }
  bool    active() const  { return _depth > 0; }

  void update(uint32_t nowMs);
  void draw(CanvasBuffer& c);
  bool needsRedraw() const;
  bool dispatch(InputEvent e);

private:
  enum Pending : uint8_t { None, Push, Pop, Replace, Root };
  void applyPending();

  Screen* _stack[EGUI_MAX_SCREEN_DEPTH];
  Screen* _pendingScreen;
  uint8_t _depth;
  Pending _pending;
};

#endif
