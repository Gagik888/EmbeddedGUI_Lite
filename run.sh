#!/bin/sh
# Хост-тесты без железа: эмулятор SSD1306 за интерфейсом Wire. Нужен только g++.
# Запуск из этой папки:  sh run.sh
S=../../src; E=../../examples
set -e
g++ -std=gnu++11 -Wall -Wextra -Imock -I$S core_test.cpp $S/*.cpp -o core_test && ./core_test | tail -12
for ex in 01_MinimalUno 02_Animation 03_Widgets; do
  g++ -std=gnu++11 -Wall -Wextra -Imock -I$S '-DEXAMPLE="'$PWD/$E/$ex/$ex.ino'"' '-DEXTRA_PER_FRAME=' run_example.cpp $S/*.cpp -o /tmp/ex_$ex
  /tmp/ex_$ex 30 | tail -1
done
