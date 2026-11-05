/**
 * @file EmbeddedGUI.h
 * @brief Фасад библиотеки EmbeddedGUI_Lite v2 (подключайте только этот файл).
 *
 *   EmbeddedGUI_OLED<128,64> gui;    // 1 объект = буфер 1 КБ + драйвер дисплея
 *   gui.begin();
 *   gui.clear(); gui.text(0,0,F("Hi")); gui.update();
 *
 * update() - ЕДИНСТВЕННЫЙ метод вывода:
 *   - режим «рисую сам»: вы рисуете примитивы/виджеты (gui.draw(w)), update() шлёт кадр;
 *   - режим «экраны»: после gui.setScreen(s) update() сам обрабатывает анимации,
 *     перерисовывает экран при изменениях и шлёт кадр.
 * Если ничего не менялось - по шине не уходит ни байта.
 */
#ifndef EMBEDDEDGUI_H
#define EMBEDDEDGUI_H

#include <Arduino.h>
#include "EmbeddedGUI_Config.h"
#include "CanvasBuffer.h"
#include "DisplayBackend.h"
#include "SSD1306Backend.h"
#include "Widgets.h"
#include "ViewportManager.h"

// ===========================================================================
// EmbeddedGUI - ядро (холст + бэкенд + экраны). Обычно используйте EmbeddedGUI_OLED.
// ===========================================================================
class EmbeddedGUI : public CanvasBuffer {
public:
  EmbeddedGUI(uint16_t w, uint16_t h, uint8_t* memory, DisplayBackend& backend);

  /// Инициализация дисплея, очистка буфера и экрана. @return false - дисплей не найден.
  bool begin();

  /// Отправить на дисплей изменённые страницы (в режиме экранов - сначала обновить и перерисовать).
  /// Вызывать в loop(); delay() не нужен.
  void update();

  /// Принудительно отправить весь буфер при следующем update().
  void invalidate() { markAllDirty(); }

  // ---- Виджеты в режиме «рисую сам» ----
  /// update(millis()) + draw(). Для анимированных виджетов вызывайте каждый кадр.
  void draw(UIWidget& w);

  // ---- Экраны ----
  bool setScreen(Screen& s) { return _vm.replace(s); }   ///< Корневой экран / замена текущего
  bool push(Screen& s)      { return _vm.push(s); }
  bool pop()                { return _vm.pop(); }
  void popToRoot()          { _vm.popToRoot(); }
  Screen* screen() const    { return _vm.current(); }
  /// Подать событие ввода активному экрану (вызывать из loop(), не из прерываний).
  bool handleEvent(InputEvent e) { return _vm.dispatch(e); }

  // ---- Управление дисплеем ----
  void setContrast(uint8_t v)  { _backend.setContrast(v); }
  void setPower(bool on)       { _backend.setPower(on); }
  void setInverted(bool on)    { _backend.setInverted(on); }
  void setFlipped(bool on)     { _backend.setFlipped(on); }

  // ---- Диагностика ----
  uint32_t lastFlushMicros() const { return _lastFlushUs; }  ///< Время последней отправки кадра
  uint32_t flushCount() const      { return _flushCount; }

private:
  DisplayBackend& _backend;
  ViewportManager _vm;
  uint32_t _lastFlushUs, _flushCount;
  bool _started;
};

// ===========================================================================
// EmbeddedGUI_OLED<W,H> - «всё в одном»: буфер W*H/8 байт + драйвер SSD1306/SH1106 (I2C).
// Объявляйте ГЛОБАЛЬНО: буфер окажется в .bss, а не на стеке.
// ===========================================================================
template <uint16_t W, uint16_t H>
class EmbeddedGUI_OLED : public EmbeddedGUI {
  static_assert(W >= 8 && W <= 128, "OLED width must be 8..128");
  static_assert(H >= 8 && H <= 64 && (H % 8) == 0, "OLED height must be 8..64, multiple of 8");
public:
  explicit EmbeddedGUI_OLED(uint8_t i2cAddr = 0x3C, EGUI_Chip chip = EGUI_SSD1306, TwoWire& wire = Wire)
    : EmbeddedGUI(W, H, _mem, _drv), _drv((uint8_t)W, (uint8_t)H, i2cAddr, chip, wire) {}

  /// @param initWire false - не вызывать Wire.begin() (если настроили шину сами: ESP32 Wire.begin(SDA,SCL))
  bool begin(bool initWire = true) { _drv.setManageWire(initWire); return EmbeddedGUI::begin(); }

  SSD1306Backend& driver() { return _drv; }   ///< Доступ к тонким настройкам (смещение колонок и т.п.)

private:
  SSD1306Backend _drv;
  uint8_t _mem[CanvasBuffer::requiredBytes(W, H)];
};

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
  bool pressed() {
    const bool level = digitalRead(_pin) == HIGH;
    const bool raw = _activeLow ? !level : level;
    const uint32_t now = millis();
    if (raw != _lastRaw) { _lastRaw = raw; _changedMs = now; }
    if ((uint32_t)(now - _changedMs) >= _debounceMs && raw != _stable) {
      _stable = raw;
      if (_stable) return true;
    }
    return false;
  }
private:
  uint8_t _pin;
  bool _activeLow;
  uint16_t _debounceMs;
  bool _lastRaw, _stable;
  uint32_t _changedMs;
};

#endif // EMBEDDEDGUI_H
