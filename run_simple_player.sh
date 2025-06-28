#!/bin/bash
set -e

# Перейти в корень проекта
cd "$(dirname "$0")"

# Создать build_simple, если не существует
mkdir -p build_simple
cd build_simple

# Генерировать CMake и собрать
cmake ..
make -j$(nproc || sysctl -n hw.ncpu)

# Запустить simple_player
./bin/simple_player.app/Contents/MacOS/simple_player & 