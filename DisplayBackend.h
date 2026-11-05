/**
 * @file DisplayBackend.h
 * @brief Минимальный интерфейс «вывода кадра» на физический дисплей.
 *
 * Бэкенд получает указатель на ЕДИНЫЙ буфер холста (страничный формат) и диапазон
 * страниц, которые нужно отправить. Ничего не копируется и не выделяется.
 * Свой бэкенд (SPI, SH1106-SPI, ...) = наследник с begin() и present().
 */
#ifndef EMBEDDEDGUI_DISPLAYBACKEND_H
#define EMBEDDEDGUI_DISPLAYBACKEND_H

#include "EmbeddedGUI_Config.h"

class DisplayBackend {
public:
  /// Инициализация железа. @return false, если дисплей не отвечает.
  virtual bool begin() = 0;
  /// Отправить страницы [first..last] из буфера (страница = width байт подряд).
  virtual void present(const uint8_t* buffer, uint8_t firstPage, uint8_t lastPage) = 0;

  // Необязательные возможности (по умолчанию - ничего не делают).
  virtual void setContrast(uint8_t) {}
  virtual void setPower(bool) {}
  virtual void setInverted(bool) {}
  virtual void setFlipped(bool) {}

protected:
  ~DisplayBackend() {}   // объекты статические, удаление через базовый класс не нужно
};

#endif
