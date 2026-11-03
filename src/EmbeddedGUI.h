/**
 * @file EmbeddedGUI.h
 * @brief Фасад библиотеки EmbeddedGUI_Lite.
 *
 *   loop() -> gui.update()
 *               |- разбор очереди событий ввода -> Screen::onEvent
 *               |- ViewportManager::update  (навигация + анимации, БЕЗ задержек)
 *               `- если пора и что-то изменилось:
 *                     Screen::_draw -> CanvasBuffer (back buffer)
 *                     DisplayBackend::present -> дисплей (один проход, без мерцания)
 *
 * Бэкенды дисплея:
 *   GfxBackend    - любой Adafruit_GFX-дисплей (SSD1306, ST7735, ST7789, ...)
 *   U8g2Backend   - U8g2 (включается #define EGUI_USE_U8G2 в EmbeddedGUI_Config.h)
 * Свой бэкенд = наследник DisplayBackend с одним методом present().
 */
#ifndef EMBEDDEDGUI_H
#define EMBEDDEDGUI_H

#include <Arduino.h>
#include "EmbeddedGUI_Config.h"
#include "CanvasBuffer.h"
#include "Widgets.h"
#include "ViewportManager.h"

// ===========================================================================
// DisplayBackend
// ===========================================================================
class DisplayBackend {
public:
  virtual ~DisplayBackend() {}
  /// Инициализация, если нужна (по умолчанию ничего; display.begin() вызывает пользователь).
  virtual bool begin() { return true; }
  /// Отправить строки [top..bottom] буфера на дисплей.
  virtual void present(CanvasBuffer& canvas, int16_t top, int16_t bottom) = 0;
};

/// Колбэк "завершить кадр" (для SSD1306 это display.display()).
typedef void (*EGUI_FlushHook)(void* ctx);

/**
 * @brief Бэкенд для любого дисплея на базе Adafruit_GFX.
 *
 * Использует drawBitmap (mono) / drawRGBBitmap (RGB565) версии для данных в ОЗУ
 * (не PROGMEM - корректно и на AVR).
 *  - SSD1306: передайте flush = функция, вызывающая oled.display().
 *  - ST7735/ST7789: flush не нужен (рисуют сразу на панель).
 */
class GfxBackend : public DisplayBackend {
public:
  GfxBackend(Adafruit_GFX& gfx, EGUI_FlushHook flush = nullptr, void* flushCtx = nullptr,
             EGUI_Color fg = EGUI_WHITE, EGUI_Color bg = EGUI_BLACK)
    : _gfx(gfx), _flush(flush), _ctx(flushCtx), _fg(fg), _bg(bg) {}

  void present(CanvasBuffer& cv, int16_t top, int16_t bottom) override;

private:
  Adafruit_GFX& _gfx;
  EGUI_FlushHook _flush;
  void* _ctx;
  EGUI_Color _fg, _bg;     // используются только в MONO-режиме
};

#if defined(EGUI_USE_U8G2) && defined(EGUI_COLOR_MONO)
#include <U8g2lib.h>
/// Бэкенд U8g2 (full-buffer режим, например U8G2_SSD1306_128X64_NONAME_F_HW_I2C).
class U8g2Backend : public DisplayBackend {
public:
  explicit U8g2Backend(U8G2& u8g2) : _u8g2(u8g2) {}
  void present(CanvasBuffer& cv, int16_t top, int16_t bottom) override;
private:
  U8G2& _u8g2;
};
#endif

// ===========================================================================
// DebouncedButton - неблокирующий антидребезг физической кнопки
// ===========================================================================
class DebouncedButton {
public:
  DebouncedButton(uint8_t pin, bool activeLow = true, uint16_t debounceMs = 30)
    : _pin(pin), _activeLow(activeLow), _debounceMs(debounceMs),
      _lastRaw(false), _stable(false), _changedMs(0) {}
  void begin() { pinMode(_pin, _activeLow ? INPUT_PULLUP : INPUT); }
  /// @return true ОДИН раз в момент нажатия. Вызывать в loop().
  bool pressed();
private:
  uint8_t _pin;
  bool _activeLow;
  uint16_t _debounceMs;
  bool _lastRaw, _stable;
  uint32_t _changedMs;
};

// ===========================================================================
// EmbeddedGUI
// ===========================================================================
class EmbeddedGUI {
public:
  EmbeddedGUI(DisplayBackend& backend, CanvasBuffer& canvas);

  /// Запустить GUI с корневым экраном. Делает первый кадр.
  void begin(Screen& root, uint8_t fps = EGUI_DEFAULT_FPS);

  /// Главный "тик". Вызывать в loop() как можно чаще. Не содержит delay().
  void update();

  /// Положить событие в очередь (SPSC: допустимо вызывать из ISR одного источника).
  bool postEvent(InputEvent e);

  void setFrameRate(uint8_t fps);          ///< Верхний предел частоты перерисовки
  void invalidate() { _forceRedraw = true; }

  ViewportManager& viewport() { return _vm; }
  CanvasBuffer&    canvas()   { return _canvas; }

  // Диагностика
  uint32_t lastRenderMicros() const { return _lastRenderUs; }
  uint32_t frameCount() const       { return _frameCount; }

private:
  bool popEvent(InputEvent& e);
  void renderFrame();

  DisplayBackend& _backend;
  CanvasBuffer&   _canvas;
  ViewportManager _vm;

  volatile uint8_t _evBuf[EGUI_EVENT_QUEUE_SIZE];
  volatile uint8_t _evHead, _evTail;

  uint16_t _frameIntervalMs;
  uint32_t _lastFrameMs;
  bool     _forceRedraw, _started;
  uint32_t _lastRenderUs, _frameCount;
};

/*
 * ===========================================================================
 * Краткий пример (полный - examples/BasicDemo/BasicDemo.ino)
 * ===========================================================================
 *
 *   Adafruit_SSD1306 oled(128, 64, &Wire, -1);
 *   StaticCanvas<128, 64> canvas;
 *   static void flushOled(void*) { oled.display(); }
 *   GfxBackend backend(oled, flushOled);
 *   EmbeddedGUI gui(backend, canvas);
 *
 *   class MyScreen : public Screen {
 *   public:
 *     MyScreen() : title(0, 0, 128, "Hello", 2), ok(30, 40, 68, 16, "OK") {
 *       title.setAlign(Label::Center);
 *       add(title); add(ok);
 *     }
 *     Label title; Button ok;
 *   } screen;
 *
 *   void setup() {
 *     Wire.begin();
 *     oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
 *     gui.begin(screen);
 *   }
 *   void loop() { gui.update(); }          // никаких delay()!
 */

#endif // EMBEDDEDGUI_H
