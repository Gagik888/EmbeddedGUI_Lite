/*
 * Плавная анимация без delay(): отскакивающий мяч, бегущий квадрат (инверсия),
 * анимированный ProgressBar и счётчик кадров в секунду.
 * Положение объектов считается от millis(), поэтому скорость не зависит от FPS.
 */
#include <EmbeddedGUI.h>

EmbeddedGUI_OLED<128, 64> gui;
ProgressBar bar(0, 52, 128, 12);

// «Пинг-понг»: 0 -> range -> 0 за 2*periodMs
static int16_t pingPong(uint32_t t, int16_t range, uint16_t periodMs) {
  uint16_t p = (uint16_t)(t % (2UL * periodMs));
  if (p > periodMs) p = 2 * periodMs - p;
  return (int16_t)((uint32_t)p * range / periodMs);
}

uint32_t fpsStamp = 0;
uint16_t fpsCount = 0, fps = 0;

void setup() {
  if (!gui.begin()) for (;;) {}
  bar.showPercent(true);
}

void loop() {
  const uint32_t t = millis();

  gui.clear();

  // Мяч: X и Y с разными периодами - получается «диагональный» отскок
  const int16_t bx = 4 + pingPong(t, 120, 1700);
  const int16_t by = 14 + pingPong(t, 24, 600);
  gui.fillCircle(bx, by, 4);

  // Квадрат с инверсией: виден и на чёрном, и поверх мяча/линии
  const int16_t sx = pingPong(t, 112, 2500);
  gui.hLine(0, 40, 128);
  gui.fillRect(sx, 36, 16, 8, EGUI_INVERT);

  // ProgressBar сам плавно доезжает до нового значения; цель меняем раз в 1.5 с
  bar.setValue(((t / 1500) % 4) * 33);        // 0, 33, 66, 99
  gui.draw(bar);

  // FPS
  gui.text(0, 0, F("FPS:"));
  gui.number(28, 0, fps);

  gui.update();

  ++fpsCount;
  if (t - fpsStamp >= 1000) { fps = fpsCount; fpsCount = 0; fpsStamp = t; }
}
