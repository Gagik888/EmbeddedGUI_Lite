/**
 * @file CanvasBuffer.h
 * @brief Внеэкранный кадровый буфер (back buffer) для двойной буферизации.
 *
 * Принцип: все виджеты рисуют в CanvasBuffer (ОЗУ), а на реальный дисплей кадр
 * отправляется ОДНИМ махом методом DisplayBackend::present(). Пользователь
 * никогда не видит "полусобранный" кадр -> нет мерцания.
 *
 * CanvasBuffer наследует Adafruit_GFX, поэтому доступны ВСЕ примитивы GFX:
 * drawLine, drawRect, fillCircle, print, setFont, getTextBounds и т.д.
 *
 * Память НЕ выделяется динамически: буфер передаётся извне либо живёт внутри
 * шаблона StaticCanvas<W,H>.
 *
 * Формат буфера:
 *  - MONO  : построчно, MSB-first, (W+7)/8 байт на строку (формат drawBitmap).
 *  - RGB565: построчно, uint16_t на пиксель.
 *
 * Отслеживание "грязных" строк: canvas помнит диапазон изменённых строк,
 * backend отправляет только его (экономия времени шины на SPI/ST7735).
 */
#ifndef EMBEDDEDGUI_CANVASBUFFER_H
#define EMBEDDEDGUI_CANVASBUFFER_H

#include <Adafruit_GFX.h>
#include "EmbeddedGUI_Config.h"

class CanvasBuffer : public Adafruit_GFX {
public:
  /// Сколько байт нужно под буфер WxH в текущем цветовом режиме.
  static constexpr size_t requiredBytes(uint16_t w, uint16_t h) {
#if defined(EGUI_COLOR_RGB565)
    return (size_t)w * h * 2;
#else
    return (size_t)((w + 7) / 8) * h;
#endif
  }

  /**
   * @param w,h    размеры в пикселях
   * @param memory внешний буфер >= requiredBytes(w,h), выровненный по 2 байта
   *               (для RGB565). Время жизни - не короче времени жизни canvas.
   */
  CanvasBuffer(uint16_t w, uint16_t h, uint8_t* memory);

  // --- Adafruit_GFX interface ---
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void fillScreen(uint16_t color) override;

  // --- Доступ к данным ---
  uint16_t getPixel(int16_t x, int16_t y) const;
  uint8_t* buffer() const { return _mem; }
  size_t   bufferSize() const { return requiredBytes(_w, _h); }
  uint16_t bytesPerRow() const;               ///< Байт на строку
  uint16_t canvasWidth()  const { return _w; }
  uint16_t canvasHeight() const { return _h; }

  // --- Dirty rows ---
  void    markAllDirty();
  void    clearDirty();
  bool    hasDirty() const { return _dirtyBottom >= _dirtyTop; }
  int16_t dirtyTop() const { return _dirtyTop; }
  int16_t dirtyBottom() const { return _dirtyBottom; }

private:
  void markRow(int16_t y) {
    if (y < _dirtyTop) _dirtyTop = y;
    if (y > _dirtyBottom) _dirtyBottom = y;
  }

  uint16_t _w, _h;
  uint8_t* _mem;
  int16_t  _dirtyTop, _dirtyBottom;
};

/**
 * @brief Canvas со встроенным статическим буфером.
 *
 * Объявляйте ГЛОБАЛЬНО (или static), чтобы буфер лежал в .bss, а не на стеке:
 *   StaticCanvas<128, 64> canvas;
 */
template <uint16_t W, uint16_t H>
class StaticCanvas : public CanvasBuffer {
public:
  StaticCanvas() : CanvasBuffer(W, H, _storage) {}
private:
  alignas(4) uint8_t _storage[CanvasBuffer::requiredBytes(W, H)];
};

#endif // EMBEDDEDGUI_CANVASBUFFER_H
