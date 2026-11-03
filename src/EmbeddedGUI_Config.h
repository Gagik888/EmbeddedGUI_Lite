/**
 * @file EmbeddedGUI_Config.h
 * @brief Compile-time configuration of EmbeddedGUI_Lite.
 *
 * ВАЖНО: Arduino IDE компилирует .cpp библиотеки отдельно от скетча, поэтому
 * #define в скетче НЕ влияют на библиотеку. Меняйте значения здесь
 * или передавайте их флагами компилятора (PlatformIO: build_flags = -DEGUI_COLOR_RGB565).
 */
#ifndef EMBEDDEDGUI_CONFIG_H
#define EMBEDDEDGUI_CONFIG_H

#include <stdint.h>
#include <stddef.h>

// ---------------------------------------------------------------------------
// Режим цвета: ровно ОДИН из двух. По умолчанию - монохромный (SSD1306).
//   EGUI_COLOR_MONO   : 1 бит/пиксель  (128x64  = 1 КБ)
//   EGUI_COLOR_RGB565 : 16 бит/пиксель (128x128 = 32 КБ, нужен ESP32/RP2040/STM32)
// ---------------------------------------------------------------------------
// #define EGUI_COLOR_RGB565
#if defined(EGUI_COLOR_MONO) && defined(EGUI_COLOR_RGB565)
  #error "Define only one of EGUI_COLOR_MONO / EGUI_COLOR_RGB565"
#endif
#if !defined(EGUI_COLOR_MONO) && !defined(EGUI_COLOR_RGB565)
  #define EGUI_COLOR_MONO
#endif

// Раскомментируйте, чтобы включить U8g2-бэкенд (только монохромный режим).
// #define EGUI_USE_U8G2

// ---------------------------------------------------------------------------
// Лимиты статической памяти (ничего не выделяется динамически).
// ---------------------------------------------------------------------------
#ifndef EGUI_MAX_WIDGETS
  #define EGUI_MAX_WIDGETS 8          ///< Макс. виджетов на один Screen
#endif
#ifndef EGUI_MAX_SCREEN_DEPTH
  #define EGUI_MAX_SCREEN_DEPTH 4     ///< Глубина стека экранов
#endif
#ifndef EGUI_LABEL_CAPACITY
  #define EGUI_LABEL_CAPACITY 24      ///< Размер буфера текста Label (с '\0')
#endif
#ifndef EGUI_EVENT_QUEUE_SIZE
  #define EGUI_EVENT_QUEUE_SIZE 8     ///< Очередь событий ввода. СТЕПЕНЬ ДВОЙКИ!
#endif
#ifndef EGUI_MAX_EVENTS_PER_UPDATE
  #define EGUI_MAX_EVENTS_PER_UPDATE 4 ///< Сколько событий обрабатывать за один update()
#endif
#ifndef EGUI_BUTTON_FLASH_MS
  #define EGUI_BUTTON_FLASH_MS 120    ///< Длительность визуального "нажатия" кнопки
#endif
#ifndef EGUI_DEFAULT_FPS
  #define EGUI_DEFAULT_FPS 30         ///< Максимальная частота перерисовки по умолчанию
#endif

// ---------------------------------------------------------------------------
// Цвета
// ---------------------------------------------------------------------------
typedef uint16_t EGUI_Color;

#if defined(EGUI_COLOR_RGB565)
  static const EGUI_Color EGUI_BLACK  = 0x0000;
  static const EGUI_Color EGUI_WHITE  = 0xFFFF;
  static const EGUI_Color EGUI_RED    = 0xF800;
  static const EGUI_Color EGUI_GREEN  = 0x07E0;
  static const EGUI_Color EGUI_BLUE   = 0x001F;
  static const EGUI_Color EGUI_YELLOW = 0xFFE0;
  static const EGUI_Color EGUI_CYAN   = 0x07FF;
  static const EGUI_Color EGUI_GRAY   = 0x8410;
#else
  // В монохромном режиме любой "не чёрный" цвет = 1.
  static const EGUI_Color EGUI_BLACK  = 0;
  static const EGUI_Color EGUI_WHITE  = 1;
  static const EGUI_Color EGUI_RED    = 1;
  static const EGUI_Color EGUI_GREEN  = 1;
  static const EGUI_Color EGUI_BLUE   = 1;
  static const EGUI_Color EGUI_YELLOW = 1;
  static const EGUI_Color EGUI_CYAN   = 1;
  static const EGUI_Color EGUI_GRAY   = 1;
#endif

#endif // EMBEDDEDGUI_CONFIG_H
