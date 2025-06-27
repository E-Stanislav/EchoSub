#!/bin/bash

echo "=== Автоматическое тестирование медиаплеера EchoSub ==="

# Проверяем наличие тестовых файлов
echo "1. Проверка тестовых файлов..."

if [ ! -f "test_audio.mp3" ]; then
    echo "❌ test_audio.mp3 не найден. Запустите test_ffmpeg.sh сначала."
    exit 1
fi

if [ ! -f "test_video.mp4" ]; then
    echo "❌ test_video.mp4 не найден. Запустите test_ffmpeg.sh сначала."
    exit 1
fi

echo "✓ Тестовые файлы найдены"

# Проверяем наличие ffplay
echo "2. Проверка ffplay..."

if ! command -v ffplay &> /dev/null; then
    echo "❌ ffplay не найден. Установите FFmpeg с поддержкой ffplay."
    exit 1
fi

echo "✓ ffplay найден: $(which ffplay)"

# Проверяем наличие скомпилированного приложения
echo "3. Проверка скомпилированного приложения..."

if [ ! -f "echosub" ]; then
    echo "❌ echosub не найден. Скомпилируйте приложение: go build -o echosub ./cmd/echosub"
    exit 1
fi

echo "✓ Приложение найдено"

# Тестируем команды ffplay, которые использует наше приложение
echo "4. Тестирование команд ffplay..."

echo "Тестирование аудио команды..."
timeout 3s ffplay -nodisp -autoexit test_audio.mp3
if [ $? -eq 0 ] || [ $? -eq 124 ]; then
    echo "✓ Аудио команда работает"
else
    echo "❌ Аудио команда не работает"
    exit 1
fi

echo "Тестирование видео команды..."
timeout 3s ffplay -window_title "EchoSub - test_video" -autoexit test_video.mp4
if [ $? -eq 0 ] || [ $? -eq 124 ]; then
    echo "✓ Видео команда работает"
else
    echo "❌ Видео команда не работает"
    exit 1
fi

echo ""
echo "=== Все тесты прошли успешно! ==="
echo ""
echo "Теперь можно протестировать приложение:"
echo "1. Запустите: ./echosub"
echo "2. Перетащите test_audio.mp3 или test_video.mp4 в окно"
echo "3. Нажмите '▶ Воспроизвести'"
echo "4. Проверьте воспроизведение"
echo ""
echo "Ожидаемое поведение:"
echo "- Для аудио: должен воспроизводиться звук (без окна)"
echo "- Для видео: должно открыться окно ffplay с видео"
echo "- В логах терминала должны появиться команды ffplay" 