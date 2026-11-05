#include "SSD1306Backend.h"

void SSD1306Backend::cmd(uint8_t c) {
  _wire->beginTransmission(_addr);
  _wire->write((uint8_t)0x00);               // control: далее идут команды
  _wire->write(c);
  _wire->endTransmission();
}

void SSD1306Backend::cmd2(uint8_t c, uint8_t v) {
  _wire->beginTransmission(_addr);
  _wire->write((uint8_t)0x00);
  _wire->write(c);
  _wire->write(v);
  _wire->endTransmission();
}

bool SSD1306Backend::begin() {
  if (_manageWire) _wire->begin();
  _wire->setClock(EGUI_I2C_CLOCK);

  _wire->beginTransmission(_addr);           // дисплей на шине?
  if (_wire->endTransmission() != 0) return false;

  const bool tall = _h > 32;
  cmd(0xAE);                                 // display off
  cmd2(0xD5, 0x80);                          // clock divide / osc
  cmd2(0xA8, (uint8_t)(_h - 1));             // multiplex
  cmd2(0xD3, 0x00);                          // display offset
  cmd(0x40);                                 // start line 0
  if (_chip == EGUI_SH1106) cmd2(0xAD, 0x8B);          // внутренний DC-DC SH1106
  else { cmd2(0x8D, 0x14); cmd2(0x20, 0x02); }         // charge pump + страничная адресация
  setFlipped(false);                         // 0xA1 + 0xC8
  cmd2(0xDA, tall ? 0x12 : 0x02);            // COM pins
  cmd2(0x81, tall ? 0xCF : 0x8F);            // contrast
  cmd2(0xD9, 0xF1);                          // pre-charge
  cmd2(0xDB, 0x40);                          // VCOMH
  cmd(0xA4);                                 // follow RAM
  cmd(0xA6);                                 // normal (не инверсия)
  cmd(0x2E);                                 // scroll off
  cmd(0xAF);                                 // display on
  return true;
}

void SSD1306Backend::present(const uint8_t* buffer, uint8_t firstPage, uint8_t lastPage) {
  for (uint8_t p = firstPage; p <= lastPage; ++p) {
    // выбрать страницу и стартовую колонку одной транзакцией
    _wire->beginTransmission(_addr);
    _wire->write((uint8_t)0x00);
    _wire->write((uint8_t)(0xB0 | (p & 0x0F)));
    _wire->write((uint8_t)(_colOffset & 0x0F));
    _wire->write((uint8_t)(0x10 | (_colOffset >> 4)));
    _wire->endTransmission();

    const uint8_t* src = buffer + (size_t)p * _w;
    uint8_t left = _w;
    while (left) {
      const uint8_t n = (left > EGUI_I2C_CHUNK) ? (uint8_t)EGUI_I2C_CHUNK : left;
      _wire->beginTransmission(_addr);
      _wire->write((uint8_t)0x40);           // control: далее идут данные
      _wire->write(src, (size_t)n);
      _wire->endTransmission();
      src += n; left -= n;
    }
  }
}

void SSD1306Backend::setContrast(uint8_t v) { cmd2(0x81, v); }
void SSD1306Backend::setPower(bool on)      { cmd(on ? 0xAF : 0xAE); }
void SSD1306Backend::setInverted(bool on)   { cmd(on ? 0xA7 : 0xA6); }
void SSD1306Backend::setFlipped(bool on) {
  cmd(on ? 0xA0 : 0xA1);                     // зеркало по X
  cmd(on ? 0xC0 : 0xC8);                     // зеркало по Y
}
