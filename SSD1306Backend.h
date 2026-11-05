/**
 * @file SSD1306Backend.h
 * @brief Драйвер SSD1306 / SH1106 по I2C поверх штатного Wire (любая плата Arduino-API).
 *
 * Работает в режиме страничной адресации: для каждой грязной страницы отправляется
 * «выбери страницу/колонку» и затем width байт данных порциями EGUI_I2C_CHUNK
 * прямо из буфера холста. Собственного буфера у драйвера нет.
 */
#ifndef EMBEDDEDGUI_SSD1306BACKEND_H
#define EMBEDDEDGUI_SSD1306BACKEND_H

#include <Wire.h>
#include "DisplayBackend.h"

class SSD1306Backend : public DisplayBackend {
public:
  SSD1306Backend(uint8_t width, uint8_t height, uint8_t i2cAddr = 0x3C,
                 EGUI_Chip chip = EGUI_SSD1306, TwoWire& wire = Wire)
    : _wire(&wire), _w(width), _h(height), _addr(i2cAddr), _chip(chip),
      _colOffset(chip == EGUI_SH1106 ? 2 : 0), _manageWire(true) {}

  bool begin() override;
  void present(const uint8_t* buffer, uint8_t firstPage, uint8_t lastPage) override;
  void setContrast(uint8_t v) override;
  void setPower(bool on) override;
  void setInverted(bool on) override;
  void setFlipped(bool on) override;

  /// false - не вызывать Wire.begin() (вы настроили шину сами, например Wire.begin(SDA,SCL) на ESP32).
  void setManageWire(bool on) { _manageWire = on; }
  /// Смещение колонок (SH1106 = 2 по умолчанию; если картинка съехала после flip - попробуйте 0).
  void setColumnOffset(uint8_t off) { _colOffset = off; }

private:
  void cmd(uint8_t c);
  void cmd2(uint8_t c, uint8_t v);

  TwoWire* _wire;
  uint8_t _w, _h, _addr;
  EGUI_Chip _chip;
  uint8_t _colOffset;
  bool _manageWire;
};

#endif
