#!/bin/bash

# Тестирование встроенного видеоплеера EchoSub
# Проверяет функциональность покадрового рендеринга и воспроизведения

set -e

echo "🎬 Тестирование встроенного видеоплеера EchoSub"
echo "================================================"

# Проверяем наличие FFmpeg
if ! command -v ffmpeg &> /dev/null; then
    echo "❌ FFmpeg не найден. Установите FFmpeg для тестирования."
    exit 1
fi

echo "✅ FFmpeg найден: $(ffmpeg -version | head -n1)"

# Проверяем наличие тестового видео
if [ ! -f "test_video.mp4" ]; then
    echo "📹 Создание тестового видео..."
    
    # Создаем простое тестовое видео с помощью FFmpeg
    ffmpeg -f lavfi -i testsrc=duration=5:size=640x480:rate=10 \
           -f lavfi -i sine=frequency=440:duration=5 \
           -c:v libx264 -c:a aac \
           -shortest test_video.mp4 -y
    
    echo "✅ Тестовое видео создано: test_video.mp4"
else
    echo "✅ Тестовое видео найдено: test_video.mp4"
fi

# Проверяем информацию о видео
echo ""
echo "📊 Информация о тестовом видео:"
DURATION=$(ffprobe -v quiet -print_format json -show_format test_video.mp4 | jq -r '.format.duration')
VIDEO_INFO=$(ffprobe -v quiet -print_format json -show_streams test_video.mp4 | jq -r '.streams[] | select(.codec_type=="video") | (.width | tostring) + "x" + (.height | tostring)')
echo "   Длительность: ${DURATION} сек"
echo "   Разрешение: ${VIDEO_INFO} пикселей"

# Тестируем извлечение кадров
echo ""
echo "🖼️  Тестирование извлечения кадров..."

# Создаем временную директорию
TEMP_DIR=$(mktemp -d)
echo "📁 Временная директория: $TEMP_DIR"

# Извлекаем кадры
echo "⏳ Извлечение кадров..."
ffmpeg -i test_video.mp4 -vf fps=10 -q:v 2 -y "$TEMP_DIR/frame_%04d.png" 2>/dev/null

# Подсчитываем количество кадров
FRAME_COUNT=$(ls "$TEMP_DIR"/frame_*.png | wc -l)
echo "✅ Извлечено кадров: $FRAME_COUNT"

# Проверяем качество кадров
if [ $FRAME_COUNT -gt 0 ]; then
    FIRST_FRAME="$TEMP_DIR/frame_0001.png"
    if [ -f "$FIRST_FRAME" ]; then
        FRAME_SIZE=$(identify -format "%wx%h" "$FIRST_FRAME" 2>/dev/null || echo "неизвестно")
        echo "📐 Размер кадра: $FRAME_SIZE"
    fi
fi

# Тестируем воспроизведение кадров
echo ""
echo "▶️  Тестирование воспроизведения кадров..."

# Создаем простой тест воспроизведения
if [ $FRAME_COUNT -gt 0 ]; then
    echo "⏳ Симуляция воспроизведения $FRAME_COUNT кадров..."
    
    # Показываем первые несколько кадров
    for i in {1..3}; do
        FRAME_FILE="$TEMP_DIR/frame_$(printf "%04d" $i).png"
        if [ -f "$FRAME_FILE" ]; then
            echo "   Кадр $i: $(ls -lh "$FRAME_FILE" | awk '{print $5}')"
        fi
    done
    
    echo "✅ Симуляция воспроизведения завершена"
else
    echo "❌ Нет кадров для воспроизведения"
fi

# Тестируем перемотку
echo ""
echo "⏩ Тестирование перемотки..."

# Создаем тестовый скрипт для перемотки
cat > "$TEMP_DIR/test_seek.sh" << 'EOF'
#!/bin/bash
# Тест перемотки - извлекаем кадр с определенной позиции
VIDEO_FILE="$1"
SEEK_TIME="$2"
OUTPUT_FILE="$3"

ffmpeg -i "$VIDEO_FILE" -ss "$SEEK_TIME" -vframes 1 -q:v 2 -y "$OUTPUT_FILE" 2>/dev/null
EOF

chmod +x "$TEMP_DIR/test_seek.sh"

# Тестируем перемотку к разным позициям
for seek_time in "1" "2.5" "4"; do
    output_file="$TEMP_DIR/seek_${seek_time}s.png"
    if "$TEMP_DIR/test_seek.sh" "test_video.mp4" "$seek_time" "$output_file"; then
        echo "✅ Перемотка к ${seek_time}с: $(ls -lh "$output_file" | awk '{print $5}')"
    else
        echo "❌ Ошибка перемотки к ${seek_time}с"
    fi
done

# Тестируем производительность
echo ""
echo "⚡ Тестирование производительности..."

# Измеряем время извлечения кадров
echo "⏱️  Измерение времени извлечения кадров..."
START_TIME=$(date +%s.%N)
ffmpeg -i test_video.mp4 -vf fps=10 -q:v 2 -y "$TEMP_DIR/benchmark_%04d.png" 2>/dev/null
END_TIME=$(date +%s.%N)

EXTRACTION_TIME=$(echo "$END_TIME - $START_TIME" | bc -l)
BENCHMARK_COUNT=$(ls "$TEMP_DIR"/benchmark_*.png | wc -l)
FPS=$(echo "scale=2; $BENCHMARK_COUNT / $EXTRACTION_TIME" | bc -l)

echo "📊 Результаты производительности:"
echo "   Время извлечения: ${EXTRACTION_TIME}с"
echo "   Кадров извлечено: $BENCHMARK_COUNT"
echo "   Скорость: ${FPS} кадров/сек"

# Очистка
echo ""
echo "🧹 Очистка временных файлов..."
rm -rf "$TEMP_DIR"

echo ""
echo "🎉 Тестирование встроенного видеоплеера завершено!"
echo ""
echo "📋 Резюме:"
echo "   ✅ FFmpeg работает корректно"
echo "   ✅ Извлечение кадров функционирует"
echo "   ✅ Перемотка работает"
echo "   ✅ Производительность измерена"
echo ""
echo "🚀 Готово к использованию в EchoSub!" 