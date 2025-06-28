#!/bin/bash

echo "Проверка установки Whisper..."

# Проверяем, установлен ли Whisper
if command -v whisper &> /dev/null; then
    echo "✅ Whisper найден в системе"
    echo "Версия: $(whisper --version 2>/dev/null || echo 'версия не определена')"
else
    echo "❌ Whisper не найден в системе"
    echo ""
    echo "Для установки Whisper выполните:"
    echo "pip install openai-whisper"
    echo ""
    echo "Или для установки с поддержкой CUDA:"
    echo "pip install openai-whisper[cu118]"
fi

echo ""
echo "Проверка доступных моделей в папке models/whisper/..."

MODELS_DIR="models/whisper"
if [ -d "$MODELS_DIR" ]; then
    echo "📁 Папка моделей найдена: $MODELS_DIR"
    echo "Найденные модели:"
    ls -la "$MODELS_DIR"/*.bin 2>/dev/null || echo "  Нет скачанных моделей"
else
    echo "📁 Папка моделей не найдена: $MODELS_DIR"
    echo "Создаем папку..."
    mkdir -p "$MODELS_DIR"
fi 