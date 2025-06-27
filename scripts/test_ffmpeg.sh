#!/bin/bash

# Скрипт для тестирования FFmpeg функциональности EchoSub

echo "=== Тестирование FFmpeg для EchoSub ==="

# Проверяем наличие FFmpeg
echo "1. Проверка установки FFmpeg..."
if command -v ffmpeg &> /dev/null; then
    echo "✓ FFmpeg найден: $(ffmpeg -version | head -n1)"
else
    echo "✗ FFmpeg не найден"
    exit 1
fi

# Проверяем наличие ffprobe
echo "2. Проверка установки ffprobe..."
if command -v ffprobe &> /dev/null; then
    echo "✓ ffprobe найден: $(ffprobe -version | head -n1)"
else
    echo "✗ ffprobe не найден"
    exit 1
fi

# Создаем тестовый аудио файл
echo "3. Создание тестового аудио файла..."
ffmpeg -f lavfi -i "sine=frequency=1000:duration=5" -c:a mp3 test_audio.mp3 -y 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✓ Тестовый аудио файл создан: test_audio.mp3"
else
    echo "✗ Ошибка создания тестового аудио файла"
    exit 1
fi

# Создаем тестовый видео файл
echo "4. Создание тестового видео файла..."
ffmpeg -f lavfi -i "testsrc=duration=5:size=320x240:rate=1" -f lavfi -i "sine=frequency=1000:duration=5" -c:v libx264 -c:a aac test_video.mp4 -y 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✓ Тестовый видео файл создан: test_video.mp4"
else
    echo "✗ Ошибка создания тестового видео файла"
    exit 1
fi

# Тестируем извлечение метаданных
echo "5. Тестирование извлечения метаданных..."
ffprobe -v quiet -print_format json -show_format -show_streams test_audio.mp3 > /dev/null
if [ $? -eq 0 ]; then
    echo "✓ Извлечение метаданных работает"
else
    echo "✗ Ошибка извлечения метаданных"
    exit 1
fi

# Тестируем генерацию волнограммы
echo "6. Тестирование генерации волнограммы..."
ffmpeg -i test_audio.mp3 -filter_complex "showwavespic=s=1200x200:colors=gray" -frames:v 1 test_waveform.png -y 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✓ Генерация волнограммы работает"
    ls -la test_waveform.png
else
    echo "✗ Ошибка генерации волнограммы"
    exit 1
fi

# Тестируем извлечение первого кадра
echo "7. Тестирование извлечения первого кадра..."
ffmpeg -i test_video.mp4 -vframes 1 -q:v 2 test_poster.jpg -y 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✓ Извлечение первого кадра работает"
    ls -la test_poster.jpg
else
    echo "✗ Ошибка извлечения первого кадра"
    exit 1
fi

echo ""
echo "=== Все тесты прошли успешно! ==="
echo "Тестовые файлы созданы:"
echo "- test_audio.mp3 (аудио)"
echo "- test_video.mp4 (видео)"
echo "- test_waveform.png (волнограмма)"
echo "- test_poster.jpg (первый кадр)"
echo ""
echo "Теперь можно протестировать EchoSub с этими файлами." 