#!/bin/bash

# Скрипт для извлечения аудио из видео файлов для Whisper
# Использование: ./extract_audio.sh input_video.mp4 [output_audio.wav]

if [ $# -eq 0 ]; then
    echo "Использование: $0 input_video.mp4 [output_audio.wav]"
    exit 1
fi

INPUT_FILE="$1"
OUTPUT_FILE="${2:-${1%.*}.wav}"

# Проверяем, что входной файл существует
if [ ! -f "$INPUT_FILE" ]; then
    echo "Ошибка: файл '$INPUT_FILE' не найден"
    exit 1
fi

# Извлекаем аудио с помощью ffmpeg
echo "Извлечение аудио из '$INPUT_FILE' в '$OUTPUT_FILE'..."

if command -v ffmpeg &> /dev/null; then
    ffmpeg -i "$INPUT_FILE" -vn -acodec pcm_s16le -ar 16000 -ac 1 "$OUTPUT_FILE" -y
    if [ $? -eq 0 ]; then
        echo "✅ Аудио успешно извлечено: $OUTPUT_FILE"
    else
        echo "❌ Ошибка при извлечении аудио"
        exit 1
    fi
else
    echo "❌ ffmpeg не найден. Установите ffmpeg для извлечения аудио."
    echo "   brew install ffmpeg"
    exit 1
fi 