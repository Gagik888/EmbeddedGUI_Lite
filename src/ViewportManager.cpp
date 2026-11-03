#include "ViewportManager.h"

// ===========================================================================
// Screen
// ===========================================================================
bool Screen::add(UIWidget& w) {
  if (_count >= EGUI_MAX_WIDGETS) return false;
  _items[_count++] = &w;
  _dirty = true;
  return true;
}

bool Screen::needsRedraw() const {
  if (_dirty) return true;
  for (uint8_t i = 0; i < _count; ++i)
    if (_items[i]->isDirty()) return true;
  return false;
}

void Screen::setFocusIndex(int8_t idx) {
  if (_focus == idx) return;
  if (_focus >= 0) _items[_focus]->setFocused(false);
  _focus = idx;
  if (_focus >= 0) _items[_focus]->setFocused(true);
}

bool Screen::moveFocus(int8_t dir) {
  if (_count == 0) return false;
  int8_t start = (_focus < 0) ? (dir > 0 ? -1 : 0) : _focus;
  for (uint8_t step = 0; step < _count; ++step) {
    start = (int8_t)((start + dir + _count) % _count);
    if (_items[start]->isFocusable()) {
      setFocusIndex(start);
      return true;
    }
  }
  return false;
}

bool Screen::onEvent(InputEvent e) {
  UIWidget* f = focused();
  if (f && f->onEvent(e)) return true;
  switch (e) {
    case InputEvent::Down:
    case InputEvent::Right: return moveFocus(+1);
    case InputEvent::Up:
    case InputEvent::Left:  return moveFocus(-1);
    case InputEvent::Back:
      if (_vm && _vm->depth() > 1) return _vm->pop();
      return false;
    default: return false;
  }
}

void Screen::_enter(ViewportManager* vm) {
  _vm = vm;
  // Если фокуса нет (или виджет пропал) - ставим на первый фокусируемый
  if (_focus < 0 || !_items[_focus]->isFocusable()) {
    _focus = -1;
    moveFocus(+1);
  }
  _dirty = true;
  for (uint8_t i = 0; i < _count; ++i) _items[i]->invalidate();
  onEnter();
}

void Screen::_leave() { onExit(); }

void Screen::_update(uint32_t nowMs) {
  for (uint8_t i = 0; i < _count; ++i) _items[i]->update(nowMs);
  onUpdate(nowMs);
}

void Screen::_draw(CanvasBuffer& c) {
  if (_autoClear) c.fillScreen(UIWidget::theme().background);
  onDrawBackground(c);
  for (uint8_t i = 0; i < _count; ++i)
    if (_items[i]->isVisible()) _items[i]->draw(c);
  onDrawOverlay(c);
  _dirty = false;
  for (uint8_t i = 0; i < _count; ++i) _items[i]->clearDirty();
}

// ===========================================================================
// ViewportManager
// ===========================================================================
bool ViewportManager::push(Screen& s) {
  if (_depth == 0) {                          // корневой экран - сразу
    _stack[_depth++] = &s;
    s._enter(this);
    return true;
  }
  if (_depth >= EGUI_MAX_SCREEN_DEPTH || current() == &s) return false;
  _pending = Push; _pendingScreen = &s;
  return true;
}

bool ViewportManager::pop() {
  if (_depth <= 1) return false;
  _pending = Pop; _pendingScreen = nullptr;
  return true;
}

bool ViewportManager::replace(Screen& s) {
  if (_depth == 0) return push(s);
  _pending = Replace; _pendingScreen = &s;
  return true;
}

void ViewportManager::popToRoot() {
  if (_depth > 1) { _pending = Root; _pendingScreen = nullptr; }
}

void ViewportManager::applyPending() {
  const Pending p = _pending;
  Screen* s = _pendingScreen;
  _pending = None; _pendingScreen = nullptr;
  if (p == None || _depth == 0) return;

  switch (p) {
    case Push:
      if (_depth < EGUI_MAX_SCREEN_DEPTH && s) {
        current()->_leave();
        _stack[_depth++] = s;
        s->_enter(this);
      }
      break;
    case Pop:
      if (_depth > 1) {
        current()->_leave();
        --_depth;
        current()->_enter(this);
      }
      break;
    case Replace:
      if (s) {
        current()->_leave();
        _stack[_depth - 1] = s;
        s->_enter(this);
      }
      break;
    case Root:
      if (_depth > 1) {
        current()->_leave();
        _depth = 1;
        _stack[0]->_enter(this);
      }
      break;
    default: break;
  }
}

void ViewportManager::update(uint32_t nowMs) {
  applyPending();
  if (Screen* s = current()) s->_update(nowMs);
}

void ViewportManager::draw(CanvasBuffer& c) {
  if (Screen* s = current()) s->_draw(c);
}

bool ViewportManager::needsRedraw() const {
  Screen* s = current();
  return s && s->needsRedraw();
}

bool ViewportManager::dispatch(InputEvent e) {
  Screen* s = current();
  return s ? s->onEvent(e) : false;
}
