# Simple Player

## Описание

Минимальный видеоплеер, построенный на базе Qt6:
- Использует стандартные компоненты Qt: `QMediaPlayer`, `QAudioOutput`, `QGraphicsView` и `QGraphicsVideoItem` для вывода видео.
- Субтитры отображаются поверх видео с помощью `QGraphicsTextItem`, что гарантирует overlay на всех платформах (macOS, Windows, Linux).
- Нет самописных декодеров, нет прямой работы с ffmpeg/ffprobe для воспроизведения (только для генерации субтитров Whisper).
- Весь UI и логика управления реализованы через Qt Widgets.

## Как работает overlay субтитров
- Видео выводится через `QGraphicsVideoItem` внутри `QGraphicsView`.
- Субтитры (SRT) парсятся и отображаются через `QGraphicsTextItem`, который всегда поверх видео.
- Масштабирование видео происходит автоматически при изменении размера окна или при загрузке нового видео.

## Сборка и запуск

1. Установите Qt6 (Core, Widgets, Multimedia, MultimediaWidgets, Concurrent).
2. В корне проекта выполните:
   ```sh
   ./run_simple_player.sh
   ```
   Этот скрипт пересоберёт проект и запустит минимальный видеоплеер.

## Скрипт для сборки и запуска
- См. файл `run_simple_player.sh` в корне репозитория.

## Возможности

- 🎵 **Аудио воспроизведение**: MP3, WAV, FLAC, AAC
- 🎬 **Видео воспроизведение**: MP4, AVI, MOV, MKV, WebM
- 📝 **Субтитры**: SRT формат с overlay поверх видео
- 🎤 **Генерация субтитров**: Whisper AI для автоматического создания субтитров
- 🎨 **Современный UI**: Темная тема, адаптивный дизайн
- ⌨️ **Горячие клавиши**: Полная поддержка клавиатурного управления
- 🖱️ **Drag & Drop**: Перетаскивание файлов в окно плеера

## Установка

### macOS
```bash
# Клонируйте репозиторий
git clone https://github.com/yourusername/simpleplayer.git
cd simpleplayer

# Установите зависимости
./scripts/install_deps_macos.sh

# Соберите проект
mkdir build
cd build
cmake ..
cmake --build .

# Запустите
./bin/simple_player
```

### Linux
```bash
# Установите Qt6
sudo apt install qt6-base-dev qt6-multimedia-dev

# Соберите проект
mkdir build
cd build
cmake ..
cmake --build .

# Запустите
./bin/simple_player
```

### Windows
```bash
# Установите Qt6 через Qt Installer
# Соберите проект через Qt Creator или командную строку
mkdir build
cd build
cmake ..
cmake --build .

# Запустите
./bin/simple_player.exe
```

## Использование

### Основные функции

1. **Открытие файлов**:
   - Кнопка "Open File" в интерфейсе
   - Горячая клавиша `Ctrl+O`
   - Перетаскивание файла в окно плеера

2. **Управление воспроизведением**:
   - ▶️ Play - воспроизведение
   - ⏸️ Pause - пауза
   - ⏹️ Stop - остановка

3. **Перемотка**:
   - Перетаскивание ползунка прогресса
   - Клик по видео для перемотки

4. **Громкость**:
   - Ползунок громкости
   - Горячие клавиши: ↑/↓

### Горячие клавиши

- `Space` - Play/Pause
- `S` - Stop
- `←/→` - Перемотка на 10 секунд
- `↑/↓` - Громкость
- `Cmd+F` - Переход в полноэкранный режим
- `Escape` - Выход из полноэкранного режима
- `Ctrl+O` - Открыть файл
- `Ctrl+Q` - Выход

## Поддерживаемые форматы

### Аудио
- MP3
- WAV
- FLAC
- AAC
- OGG
- M4A

### Видео
- MP4
- AVI
- MOV
- MKV
- WebM
- FLV
- WMV

## Архитектура

```
SimplePlayer/
├── src/                    # Исходный код
│   ├── simple_player_test.cpp # Точка входа
│   ├── core/              # Основная логика
│   │   ├── simplemediaplayer.cpp
│   │   └── WhisperModelSettingsDialog.cpp
│   └── ui/                # Пользовательский интерфейс
│       └── videowidget.cpp
├── include/               # Заголовочные файлы
├── build/                 # Папка сборки
├── docs/                  # Документация
├── scripts/               # Скрипты сборки
├── CMakeLists.txt         # Конфигурация CMake
└── README.md             # Этот файл
```

## Лицензия

MIT License - см. файл LICENSE для деталей.

## Вклад в проект

1. Форкните репозиторий
2. Создайте ветку для новой функции (`git checkout -b feature/amazing-feature`)
3. Зафиксируйте изменения (`git commit -m 'Add amazing feature'`)
4. Отправьте в ветку (`git push origin feature/amazing-feature`)
5. Откройте Pull Request

## Поддержка

Если у вас есть вопросы или проблемы:
- Создайте Issue в GitHub
- Опишите проблему подробно
- Приложите логи ошибок, если есть

## Благодарности

- Qt Framework за отличную библиотеку
- OpenAI Whisper за технологию распознавания речи
- Сообществу open source за вдохновение 