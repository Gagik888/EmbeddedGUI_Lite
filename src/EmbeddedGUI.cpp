#include "EmbeddedGUI.h"

static_assert((EGUI_EVENT_QUEUE_SIZE & (EGUI_EVENT_QUEUE_SIZE - 1)) == 0,
              "EGUI_EVENT_QUEUE_SIZE must be a power of two");
static const uint8_t EGUI_EV_MASK = EGUI_EVENT_QUEUE_SIZE - 1;

// ===========================================================================
// GfxBackend
// ===========================================================================
void GfxBackend::present(CanvasBuffer& cv, int16_t top, int16_t bottom) {
  const int16_t rows = bottom - top + 1;
  if (rows <= 0) return;
#if defined(EGUI_COLOR_RGB565)
  uint16_t* src = reinterpret_cast<uint16_t*>(cv.buffer()) + (size_t)top * cv.canvasWidth();
  _gfx.drawRGBBitmap(0, top, src, cv.canvasWidth(), rows);       // перегрузка для ОЗУ
#else
  uint8_t* src = cv.buffer() + (size_t)top * cv.bytesPerRow();
  _gfx.drawBitmap(0, top, src, cv.canvasWidth(), rows, _fg, _bg); // перегрузка для ОЗУ
#endif
  if (_flush) _flush(_ctx);
}

#if defined(EGUI_USE_U8G2) && defined(EGUI_COLOR_MONO)
void U8g2Backend::present(CanvasBuffer& cv, int16_t, int16_t) {
  _u8g2.clearBuffer();
  _u8g2.drawBitmap(0, 0, cv.bytesPerRow(), cv.canvasHeight(), cv.buffer());
  _u8g2.sendBuffer();
}
#endif

// ===========================================================================
// DebouncedButton
// ===========================================================================
bool DebouncedButton::pressed() {
  const bool level = digitalRead(_pin) == HIGH;
  const bool raw = _activeLow ? !level : level;     // true = кнопка нажата
  const uint32_t now = millis();
  if (raw != _lastRaw) { _lastRaw = raw; _changedMs = now; }
  if ((uint32_t)(now - _changedMs) >= _debounceMs && raw != _stable) {
    _stable = raw;
    if (_stable) return true;                       // фронт нажатия
  }
  return false;
}

// ===========================================================================
// EmbeddedGUI
// ===========================================================================
EmbeddedGUI::EmbeddedGUI(DisplayBackend& backend, CanvasBuffer& canvas)
  : _backend(backend), _canvas(canvas), _evHead(0), _evTail(0),
    _frameIntervalMs(1000 / EGUI_DEFAULT_FPS), _lastFrameMs(0),
    _forceRedraw(true), _started(false), _lastRenderUs(0), _frameCount(0) {}

void EmbeddedGUI::setFrameRate(uint8_t fps) {
  if (fps == 0) fps = 1;
  _frameIntervalMs = (uint16_t)(1000 / fps);
}

void EmbeddedGUI::begin(Screen& root, uint8_t fps) {
  _backend.begin();
  setFrameRate(fps);
  _vm.push(root);
  _forceRedraw = true;
  _started = true;
  _lastFrameMs = millis();
  renderFrame();
}

bool EmbeddedGUI::postEvent(InputEvent e) {
  const uint8_t next = (uint8_t)((_evHead + 1) & EGUI_EV_MASK);
  if (next == _evTail) return false;                // очередь полна - событие теряется
  _evBuf[_evHead] = (uint8_t)e;
  _evHead = next;
  return true;
}

bool EmbeddedGUI::popEvent(InputEvent& e) {
  if (_evTail == _evHead) return false;
  e = (InputEvent)_evBuf[_evTail];
  _evTail = (uint8_t)((_evTail + 1) & EGUI_EV_MASK);
  return true;
}

void EmbeddedGUI::update() {
  if (!_started) return;
  const uint32_t now = millis();

  InputEvent e;
  for (uint8_t n = 0; n < EGUI_MAX_EVENTS_PER_UPDATE && popEvent(e); ++n)
    _vm.dispatch(e);

  _vm.update(now);                                  // навигация + анимации

  if ((uint32_t)(now - _lastFrameMs) >= _frameIntervalMs &&
      (_forceRedraw || _vm.needsRedraw())) {
    _lastFrameMs = now;
    renderFrame();
  }
}

void EmbeddedGUI::renderFrame() {
  const uint32_t t0 = micros();
  _vm.draw(_canvas);                                // 1) собрать кадр в back buffer
  if (_canvas.hasDirty()) {
    _backend.present(_canvas, _canvas.dirtyTop(), _canvas.dirtyBottom()); // 2) одним проходом на дисплей
    _canvas.clearDirty();
  }
  _forceRedraw = false;
  _lastRenderUs = micros() - t0;
  ++_frameCount;
}
