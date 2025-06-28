#!/bin/bash

# Скрипт для запуска simple_player
# Использование: ./run_simple_player.sh [путь_к_видео_файлу]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLAYER_PATH="$SCRIPT_DIR/build_simple/bin/simple_player.app/Contents/MacOS/simple_player"

if [ ! -f "$PLAYER_PATH" ]; then
    echo "Ошибка: simple_player не найден в $PLAYER_PATH"
    echo "Убедитесь, что приложение собрано в build_simple/"
    exit 1
fi

if [ $# -eq 0 ]; then
    # Запуск без аргументов
    "$PLAYER_PATH"
else
    # Запуск с файлом
    "$PLAYER_PATH" "$1"
fi 