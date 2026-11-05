/*
 * Минимальный пример: Arduino Uno/Nano + OLED 128x64 (SSD1306, I2C).
 *
 * Подключение:  OLED GND -> GND, VCC -> 5V (или 3.3V), SDA -> A4, SCL -> A5.
 * Адрес по умолчанию 0x3C (если не работает - попробуйте 0x3D).
 * ОЗУ: ~1.1 КБ (буфер 1 КБ + служебные поля).
 */
#include <EmbeddedGUI.h>

EmbeddedGUI_OLED<128, 64> gui;          // объявлять глобально!

void setup() {
  if (!gui.begin()) {                   // Wire.begin() + инициализация дисплея
    for (;;) {}                         // дисплей не найден - проверьте провода/адрес
  }
}

void loop() {
  gui.clear();                                        // 1. очистить буфер
  gui.text(0, 0, F("EmbeddedGUI Lite"));              // 2. рисуем (F() держит строку во флеше)
  gui.hLine(0, 10, 128);
  gui.text(0, 20, F("Hello!"), 2);                    //    размер шрифта x2
  gui.text(0, 40, F("uptime, s: "));
  gui.number(72, 40, millis() / 1000);
  gui.rect(0, 52, 128, 12);
  gui.fillRect(2, 54, (millis() / 50) % 125, 8);      //    бегущая полоска
  gui.update();                                       // 3. отправить кадр на дисплей
}
