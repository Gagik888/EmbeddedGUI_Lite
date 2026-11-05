#include "ViewportManager.h"

// ===========================================================================
// Screen
// ===========================================================================
void Screen::add(UIWidget& w) {
  w._next = nullptr;
  if (!_head) _head = &w;
  else {
    UIWidget* t = _head;
    while (t->_next) t = t->_next;
    t->_next = &w;
  }
  _dirty = 1;
}

bool Screen::needsRedraw() const {
  if (_dirty) return true;
  for (UIWidget* w = _head; w; w = w->_next)
    if (w->isDirty()) return true;
  return false;
}

void Screen::setFocus(UIWidget* w) {
  if (_focus == w) return;
  if (_focus) _focus->setFocused(false);
  _focus = w;
  if (_focus) _focus->setFocused(true);
}

bool Screen::moveFocus(int8_t dir) {
  UIWidget *first = nullptr, *last = nullptr, *prev = nullptr, *next = nullptr;
  bool seen = false;
  for (UIWidget* w = _head; w; w = w->_next) {
    if (!w->isFocusable() || !w->isVisible()) continue;
    if (!first) first = w;
    last = w;
    if (w == _focus) seen = true;
    else if (!seen) prev = w;
    else if (!next) next = w;
  }
  UIWidget* target = (dir > 0) ? (next ? next : first) : (prev ? prev : last);
  if (!target) return false;
  setFocus(target);
  return true;
}

bool Screen::onEvent(InputEvent e) {
  if (_focus && _focus->onEvent(e)) return true;
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
  if (!_focus || !_focus->isFocusable()) { setFocus(nullptr); moveFocus(+1); }
  _dirty = 1;
  for (UIWidget* w = _head; w; w = w->_next) w->invalidate();
  onEnter();
}

void Screen::_update(uint32_t nowMs) {
  for (UIWidget* w = _head; w; w = w->_next) w->update(nowMs);
  onUpdate(nowMs);
}

void Screen::_draw(CanvasBuffer& c) {
  if (_autoClear) c.clear(EGUI_BLACK);
  onDraw(c);
  for (UIWidget* w = _head; w; w = w->_next) {
    if (w->isVisible()) w->draw(c);
    w->clearDirty();
  }
  _dirty = 0;
}

// ===========================================================================
// ViewportManager
// ===========================================================================
bool ViewportManager::push(Screen& s) {
  if (_depth == 0) { _stack[_depth++] = &s; s._enter(this); return true; }
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
      if (_depth < EGUI_MAX_SCREEN_DEPTH && s) { current()->_leave(); _stack[_depth++] = s; s->_enter(this); }
      break;
    case Pop:
      if (_depth > 1) { current()->_leave(); --_depth; current()->_enter(this); }
      break;
    case Replace:
      if (s) { current()->_leave(); _stack[_depth - 1] = s; s->_enter(this); }
      break;
    case Root:
      if (_depth > 1) { current()->_leave(); _depth = 1; _stack[0]->_enter(this); }
      break;
    default: break;
  }
}

void ViewportManager::update(uint32_t nowMs) {
  applyPending();
  if (Screen* s = current()) s->_update(nowMs);
}

void ViewportManager::draw(CanvasBuffer& c) { if (Screen* s = current()) s->_draw(c); }

bool ViewportManager::needsRedraw() const {
  Screen* s = current();
  return s && s->needsRedraw();
}

bool ViewportManager::dispatch(InputEvent e) {
  Screen* s = current();
  return s ? s->onEvent(e) : false;
}
