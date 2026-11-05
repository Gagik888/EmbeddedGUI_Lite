#include "EmbeddedGUI.h"
#include <string.h>

EmbeddedGUI::EmbeddedGUI(uint16_t w, uint16_t h, uint8_t* memory, DisplayBackend& backend)
  : CanvasBuffer(w, h, memory), _backend(backend),
    _lastFlushUs(0), _flushCount(0), _started(false) {}

bool EmbeddedGUI::begin() {
  memset(buffer(), 0, bufferSize());
  const bool ok = _backend.begin();
  markAllDirty();                         // первый update() зальёт пустой буфер и сотрёт «мусор» GDDRAM
  _started = true;
  if (ok) update();
  return ok;
}

void EmbeddedGUI::draw(UIWidget& w) {
  w.update(millis());
  if (w.isVisible()) w.draw(*this);
  w.clearDirty();
}

void EmbeddedGUI::update() {
  if (!_started) return;
  if (_vm.active()) {                     // режим экранов
    _vm.update(millis());                 // навигация + анимации
    if (_vm.needsRedraw()) _vm.draw(*this);
  }
  if (!hasDirty()) return;                // ничего не менялось - шина молчит
  const uint32_t t0 = micros();
  _backend.present(buffer(), dirtyFirstPage(), dirtyLastPage());
  clearDirty();
  _lastFlushUs = micros() - t0;
  ++_flushCount;
}
