/**
 * @file CanvasBuffer.h
 * @brief Единый кадровый буфер в «страничном» формате SSD1306 + примитивы рисования.
 *
 * Буфер ОДИН и лежит в том же формате, который понимает дисплей:
 *   байт = 8 вертикальных пикселей, индекс = (y >> 3) * width + x, бит = y & 7 (LSB сверху).
 * Поэтому на дисплей он уходит напрямую, без конвертации и без второй копии.
 *   128x64 -> ровно 1024 байта. Динамической памяти нет.
 *
 * Отслеживание «грязных» страниц: отправляются только изменённые страницы (8 строк).
 * Зависимостей от Adafruit_GFX/U8g2 нет. Шрифт 5x7 (ASCII) лежит во флеше.
 */
#ifndef EMBEDDEDGUI_CANVASBUFFER_H
#define EMBEDDEDGUI_CANVASBUFFER_H

#include <Arduino.h>
#include "EmbeddedGUI_Config.h"

class CanvasBuffer {
public:
  /// Сколько байт нужно под буфер WxH.
  static constexpr size_t requiredBytes(uint16_t w, uint16_t h) {
    return (size_t)w * ((h + 7) / 8);
  }

  /// @param memory внешний буфер >= requiredBytes(w,h). Обычно его даёт EmbeddedGUI_OLED.
  CanvasBuffer(uint16_t w, uint16_t h, uint8_t* memory);

  // ---- Информация ----
  uint16_t width()  const { return _w; }
  uint16_t height() const { return _h; }
  uint8_t  pages()  const { return (uint8_t)((_h + 7) >> 3); }
  uint8_t* buffer() const { return _mem; }
  size_t   bufferSize() const { return requiredBytes(_w, _h); }

  // ---- Базовые операции (всё с отсечением по границам экрана) ----
  void clear(EGUI_Color c = EGUI_BLACK);
  void pixel(int16_t x, int16_t y, EGUI_Color c = EGUI_WHITE);
  bool getPixel(int16_t x, int16_t y) const;

  // ---- Примитивы ----
  void hLine(int16_t x, int16_t y, int16_t w, EGUI_Color c = EGUI_WHITE);
  void vLine(int16_t x, int16_t y, int16_t h, EGUI_Color c = EGUI_WHITE);
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, EGUI_Color c = EGUI_WHITE);
  void rect(int16_t x, int16_t y, int16_t w, int16_t h, EGUI_Color c = EGUI_WHITE);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, EGUI_Color c = EGUI_WHITE);
  void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, EGUI_Color c = EGUI_WHITE);
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, EGUI_Color c = EGUI_WHITE);
  void circle(int16_t cx, int16_t cy, int16_t r, EGUI_Color c = EGUI_WHITE);
  void fillCircle(int16_t cx, int16_t cy, int16_t r, EGUI_Color c = EGUI_WHITE);

  /// Битмап из флеша (PROGMEM): построчно, MSB первым, (w+7)/8 байт на строку.
  /// Рисуются только единичные биты (нули прозрачны).
  void bitmap(int16_t x, int16_t y, const uint8_t* bmp, int16_t w, int16_t h,
              EGUI_Color c = EGUI_WHITE);

  // ---- Текст: шрифт 5x7, ячейка 6x8 * size. '\n' - новая строка. ----
  /// @return X-координата сразу после последнего символа (удобно для «склейки» вывода)
  int16_t text(int16_t x, int16_t y, const char* s, uint8_t size = 1, EGUI_Color c = EGUI_WHITE);
  int16_t text(int16_t x, int16_t y, const __FlashStringHelper* s, uint8_t size = 1, EGUI_Color c = EGUI_WHITE);
  int16_t number(int16_t x, int16_t y, long v, uint8_t size = 1, EGUI_Color c = EGUI_WHITE);
  int16_t numberF(int16_t x, int16_t y, float v, uint8_t decimals = 1, uint8_t size = 1, EGUI_Color c = EGUI_WHITE);

  /// Ширина первой строки текста в пикселях.
  static uint16_t textWidth(const char* s, uint8_t size = 1);
  static uint16_t textWidth(const __FlashStringHelper* s, uint8_t size = 1);

  /// Форматирование без printf (экономит ~1.5 КБ флеша). buf >= EGUI_NUM_BUF. @return длина
  static uint8_t formatNumber(char* buf, long v);
  static uint8_t formatDecimal(char* buf, float v, uint8_t decimals);

  // ---- «Грязные» страницы ----
  void    markAllDirty()  { _dirtyMin = 0; _dirtyMax = (uint8_t)(pages() - 1); }
  void    clearDirty()    { _dirtyMin = 0xFF; _dirtyMax = 0; }
  bool    hasDirty() const { return _dirtyMin <= _dirtyMax; }
  uint8_t dirtyFirstPage() const { return _dirtyMin; }
  uint8_t dirtyLastPage()  const { return _dirtyMax; }

private:
  void markPages(uint8_t p0, uint8_t p1) {
    if (p0 < _dirtyMin) _dirtyMin = p0;
    if (p1 > _dirtyMax) _dirtyMax = p1;
  }
  void columnMask(int16_t x, int16_t y, uint8_t mask, EGUI_Color c);
  void glyph(int16_t x, int16_t y, char ch, uint8_t size, EGUI_Color c);
  int16_t drawChars(int16_t x, int16_t y, const char* s, bool pgm, uint8_t size, EGUI_Color c);
  void cornerArcs(int16_t cx, int16_t cy, int16_t r, uint8_t corners, EGUI_Color c);

  uint16_t _w, _h;
  uint8_t* _mem;
  uint8_t  _dirtyMin, _dirtyMax;
};

#endif // EMBEDDEDGUI_CANVASBUFFER_H
