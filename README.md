# EchoSub Simple Player

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
- 🖱️ **Drag & Drop**: Перетаскивание файлов в окно приложения
- 🎛️ **Полное управление**: Play/Pause/Stop, перемотка, громкость
- 🎨 **Современный UI**: Красивый интерфейс на Qt 6
- ⚡ **Высокая производительность**: Нативная реализация на C++

## Требования

### Системные зависимости

#### macOS
```bash
# Установка Qt 6
brew install qt6

# Установка FFmpeg
brew install ffmpeg

# Установка CMake (если не установлен)
brew install cmake
```

#### Ubuntu/Debian
```bash
# Установка Qt 6
sudo apt update
sudo apt install qt6-base-dev qt6-multimedia-dev qt6-multimedia-widgets-dev

# Установка FFmpeg
sudo apt install ffmpeg libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev

# Установка CMake
sudo apt install cmake build-essential
```

#### Windows
1. Установите [Qt 6](https://www.qt.io/download)
2. Установите [FFmpeg](https://ffmpeg.org/download.html)
3. Установите [CMake](https://cmake.org/download/)
4. Установите [Visual Studio](https://visualstudio.microsoft.com/) или [MinGW](https://www.mingw-w64.org/)

## Сборка

### 1. Клонирование репозитория
```bash
git clone https://github.com/yourusername/echosub.git
cd echosub
```

### 2. Создание папки для сборки
```bash
mkdir build
cd build
```

### 3. Конфигурация CMake
```bash
cmake ..
```

### 4. Сборка
```bash
cmake --build .
```

### 5. Запуск
```bash
./bin/echosub
```

## Использование

### Основные функции

1. **Открытие файла**: 
   - Меню File → Open
   - Кнопка "Open" на панели инструментов
   - Drag & Drop файла в окно

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
EchoSub/
├── src/                    # Исходный код
│   ├── main.cpp           # Точка входа
│   ├── core/              # Основная логика
│   │   ├── mediaplayer.cpp
│   │   ├── audiodecoder.cpp
│   │   └── videodecoder.cpp
│   ├── ui/                # Пользовательский интерфейс
│   │   ├── mainwindow.cpp
│   │   ├── videowidget.cpp
│   │   └── controlspanel.cpp
│   └── media/             # Медиа обработка
│       └── ffmpegwrapper.cpp
├── include/               # Заголовочные файлы
├── build/                 # Папка сборки
├── docs/                  # Документация
├── scripts/               # Скрипты сборки
├── CMakeLists.txt         # Конфигурация CMake
└── README.md             # Этот файл
```

## Разработка

### Структура классов

- **MediaPlayer**: Основной класс плеера
- **AudioDecoder**: Декодирование аудио
- **VideoDecoder**: Декодирование видео
- **FFmpegWrapper**: Обертка для FFmpeg API
- **MainWindow**: Главное окно приложения
- **VideoWidget**: Виджет отображения видео
- **ControlsPanel**: Панель управления

### Добавление новых форматов

1. Добавьте поддержку в `FFmpegWrapper`
2. Обновите список форматов в `MediaPlayer::isValidMediaFile()`
3. Протестируйте с новым форматом

## Лицензия

MIT License - см. файл LICENSE для деталей.

## Вклад в проект

1. Fork репозитория
2. Создайте ветку для новой функции
3. Внесите изменения
4. Создайте Pull Request

## Поддержка

- Создайте Issue для багов
- Обсуждения в Discussions
- Документация в папке docs/ 