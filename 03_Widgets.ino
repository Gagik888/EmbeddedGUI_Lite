/*
 * Виджеты и экраны: Label, Button, ProgressBar, DynamicGraph + навигация.
 *
 * Кнопки (на землю, внутренняя подтяжка): D2 = вниз/фокус, D3 = выбрать, D4 = назад.
 * Экран "Главный" -> кнопка "Настройки" -> экран "Настройки" (Back/кнопка возвращает).
 */
#include <EmbeddedGUI.h>

EmbeddedGUI_OLED<128, 64> gui;

DebouncedButton btnNext(2), btnOk(3), btnBack(4);

// ---------------------------------------------------------------------------
class SettingsScreen : public Screen {
public:
  SettingsScreen()
    : title(0, 0, 128, F("Settings"), 1),
      info(0, 16, 128, F("Contrast:"), 1),
      value(0, 28, 128, "", 2),
      plus(0, 48, 40, 16, F("+")),
      minus(44, 48, 40, 16, F("-")),
      back(88, 48, 40, 16, F("Back")) {
    title.setAlign(Label::Center);
    add(title); add(info); add(value); add(plus); add(minus); add(back);
    value.setInt(contrast);
  }
  void changeContrast(int16_t d) {
    contrast = constrain(contrast + d, 0, 255);
    value.setInt(contrast);
    gui.setContrast((uint8_t)contrast);
  }
  Label title, info, value;
  Button plus, minus, back;
  int16_t contrast = 207;
} settings;

// ---------------------------------------------------------------------------
class MainScreen : public Screen {
public:
  MainScreen()
    : title(0, 0, 128, F("Dashboard"), 1),
      reading(0, 12, 70, "", 2),
      graph(72, 10, 56, 24),
      bar(0, 38, 128, 11),
      open(0, 52, 128, 12, F("Settings")) {
    title.setAlign(Label::Left);
    add(title); add(reading); add(graph); add(bar); add(open);
    bar.showPercent(true);
  }
  void onUpdate(uint32_t now) override {          // вызывается каждый цикл, без блокировок
    if (now - lastSample >= 200) {
      lastSample = now;
      const int16_t v = analogRead(A0);           // подставьте свой датчик
      graph.pushSample(v);
      reading.setFloat(v * (5.0f / 1023.0f), 2);  // вольты, 2 знака
      bar.setValue((int32_t)v * 100 / 1023);
    }
  }
  Label title, reading;
  DynamicGraphT<56> graph;                        // 56 отсчётов = 112 байт ОЗУ
  ProgressBar bar;
  Button open;
  uint32_t lastSample = 0;
} mainScreen;

// ---------------------------------------------------------------------------
static void openSettings(void*) { gui.push(settings); }
static void goBack(void*)        { gui.pop(); }
static void contrastUp(void*)    { settings.changeContrast(+32); }
static void contrastDown(void*)  { settings.changeContrast(-32); }

void setup() {
  btnNext.begin(); btnOk.begin(); btnBack.begin();
  if (!gui.begin()) for (;;) {}

  mainScreen.open.setOnClick(openSettings);
  settings.back.setOnClick(goBack);
  settings.plus.setOnClick(contrastUp);
  settings.minus.setOnClick(contrastDown);

  gui.setScreen(mainScreen);
}

void loop() {
  if (btnNext.pressed()) gui.handleEvent(InputEvent::Down);
  if (btnOk.pressed())   gui.handleEvent(InputEvent::Select);
  if (btnBack.pressed()) gui.handleEvent(InputEvent::Back);
  gui.update();
}
