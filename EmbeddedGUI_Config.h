/**
 * @file EmbeddedGUI_Config.h
 * @brief Параметры времени компиляции EmbeddedGUI_Lite v2.
 *
 * ВАЖНО: Arduino IDE компилирует .cpp библиотеки отдельно от скетча, поэтому
 * #define в скетче НЕ влияют на библиотеку (и могут сломать раскладку классов).
 * Меняйте значения здесь или передавайте флагами компилятора
 * (PlatformIO: build_flags = -DEGUI_LABEL_CAPACITY=32).
 */
#ifndef EMBEDDEDGUI_CONFIG_H
#define EMBEDDEDGUI_CONFIG_H

#include <stdint.h>
#include <stddef.h>

#define EGUI_VERSION "2.0.0"

#ifndef EGUI_MAX_SCREEN_DEPTH
  #define EGUI_MAX_SCREEN_DEPTH 4      ///< Глубина стека экранов (2 байта на уровень)
#endif
#ifndef EGUI_LABEL_CAPACITY
  #define EGUI_LABEL_CAPACITY 20       ///< Буфер текста Label, байт (с '\0')
#endif
#ifndef EGUI_BUTTON_FLASH_MS
  #define EGUI_BUTTON_FLASH_MS 120     ///< Длительность визуального "нажатия" кнопки
#endif
#ifndef EGUI_PROGRESS_SMOOTH_MS
  #define EGUI_PROGRESS_SMOOTH_MS 150  ///< Постоянная времени плавной анимации ProgressBar
#endif
#ifndef EGUI_I2C_CLOCK
  #define EGUI_I2C_CLOCK 400000UL      ///< Частота I2C (SSD1306 держит 400 кГц)
#endif
#ifndef EGUI_I2C_CHUNK
  // Сколько байт данных за одну I2C-транзакцию (+1 управляющий байт).
  // AVR: буфер Wire = 32 байта -> 30 данных + 1 control. Остальные: консервативно 16.
  #if defined(__AVR__)
    #define EGUI_I2C_CHUNK 30
  #else
    #define EGUI_I2C_CHUNK 16
  #endif
#endif
#define EGUI_NUM_BUF 16                ///< Буфер для форматирования чисел

/// Цвет: 0 - погашен, 1 - горит, 2 - инверсия (XOR) пикселей под фигурой.
typedef uint8_t EGUI_Color;
enum : EGUI_Color { EGUI_BLACK = 0, EGUI_WHITE = 1, EGUI_INVERT = 2 };

/// Контроллер дисплея.
enum EGUI_Chip : uint8_t { EGUI_SSD1306 = 0, EGUI_SH1106 = 1 };

#endif // EMBEDDEDGUI_CONFIG_H
