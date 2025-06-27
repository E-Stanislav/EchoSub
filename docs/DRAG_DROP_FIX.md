# Исправление Drag-and-Drop в EchoSub

## 🐛 Проблема

Drag-and-drop не работал из-за неправильного API для Fyne 2.6.

## ✅ Решение

### 1. Исправлен API для Fyne 2.6

**Было:**
```go
// Неправильная сигнатура для Fyne 2.6
w.SetOnDropped(func(e *fyne.DropEvent) {
    if len(e.URIs) > 0 {
        filePath := e.URIs[0].Path()
        mediaView.LoadMediaFile(filePath)
    }
})
```

**Стало:**
```go
// Правильная сигнатура для Fyne 2.6
w.SetOnDropped(func(pos fyne.Position, uris []fyne.URI) {
    if len(uris) > 0 {
        filePath := uris[0].Path()
        mediaView.LoadMediaFile(filePath)
    }
})
```

### 2. Добавлена фильтрация форматов

```go
// Проверяем поддерживаемый формат
ext := strings.ToLower(filepath.Ext(filePath))
if ext == "" {
    log.Printf("Skipping file without extension: %s", filePath)
    return
}

// Убираем точку из расширения
format := ext[1:]
if !media.IsSupportedFormat(format) {
    log.Printf("Unsupported format: %s", format)
    return
}
```

### 3. Улучшена визуальная обратная связь

Добавлены методы в `MediaView`:
- `ShowDragOverlay()` - показывает индикатор drag-and-drop
- `HideDragOverlay()` - скрывает индикатор
- `ShowDragOverlayValid()` - показывает индикатор для валидного файла

## 🧪 Тестирование

### Автоматические тесты
```bash
# Создание тестовых файлов
./scripts/test_ffmpeg.sh

# Проверка готовности к тестированию drag-and-drop
./scripts/test_drag_drop.sh
```

### Ручное тестирование
1. Запустить приложение: `./echosub`
2. Перетащить `test_audio.mp3` или `test_video.mp4` в окно
3. Проверить загрузку и отображение информации
4. Попробовать перетащить неподдерживаемый файл (.txt)

## 📋 Поддерживаемые форматы

**Видео:** MP4, MKV, MOV
**Аудио:** MP3, WAV, M4A, FLAC

## 🔧 Файлы изменений

- `cmd/echosub/main.go` - исправлен API drag-and-drop
- `internal/ui/media_view.go` - добавлена визуальная обратная связь
- `docs/FR-01_IMPROVEMENTS.md` - обновлена документация
- `scripts/test_drag_drop.sh` - добавлен тест drag-and-drop

## ✅ Результат

Drag-and-drop теперь полностью работает:
- ✅ Правильный API для Fyne 2.6
- ✅ Фильтрация по поддерживаемым форматам
- ✅ Визуальная обратная связь
- ✅ Логирование для отладки
- ✅ Тестирование готово 