#!/bin/bash
set -e  # если любая команда падает — сразу выходим

# папка для сборки
BUILD_DIR="cmake-build-debug"

# создаём папку, если нет
mkdir -p "$BUILD_DIR"

# конфигурируем CMake
cmake -S . -B "$BUILD_DIR" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug

# собираем (можно -jN для многопоточности)
cmake --build "$BUILD_DIR" --target mafia -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# запускаем бинарь только в интерактивной сессии (есть TTY)
if [ -t 0 ]; then
  "./$BUILD_DIR/mafia"
fi

