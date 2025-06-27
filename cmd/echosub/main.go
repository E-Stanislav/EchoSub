package main

import (
	"log"
	"os"
	"path/filepath"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/layout"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"

	"github.com/echosub/echosub/internal/media"
	"github.com/echosub/echosub/internal/ui"
)

func main() {
	// Инициализация приложения с уникальным ID
	a := app.NewWithID("com.echosub.app")
	a.SetIcon(theme.DocumentIcon())

	// Создание главного окна
	w := a.NewWindow("EchoSub - Professional Subtitle Editor")
	w.Resize(fyne.NewSize(1200, 800))
	// w.CenterOnScreen() // Удалено: может вызывать panic на macOS/Fyne. Центрирование окна не требуется.

	// Инициализация FFmpeg процессора
	ffmpeg, err := media.NewFFmpegProcessor()
	if err != nil {
		log.Printf("Warning: FFmpeg not available: %v", err)
		// Продолжаем без FFmpeg для демонстрации UI
	}

	// Создание MediaView (передаем nil если FFmpeg недоступен)
	mediaView := ui.NewMediaView(ffmpeg)

	// Создание тулбара
	toolbar := createToolbar(w, mediaView)

	// Создание главного контейнера
	mainContainer := container.NewBorder(
		toolbar,                // top
		nil,                    // bottom
		nil,                    // left
		nil,                    // right
		mediaView.GetContent(), // content
	)

	// Настройка drag-and-drop
	setupDragAndDrop(w, mediaView)

	// Установка содержимого окна
	w.SetContent(mainContainer)

	// Показ окна
	w.ShowAndRun()

	// Очистка при выходе
	if ffmpeg != nil {
		ffmpeg.Cleanup()
	}
}

// setupDragAndDrop настраивает drag-and-drop для окна
func setupDragAndDrop(w fyne.Window, mediaView *ui.MediaView) {
	// В Fyne 2.6+ используется SetOnDropped с правильной сигнатурой
	w.SetOnDropped(func(pos fyne.Position, uris []fyne.URI) {
		if len(uris) > 0 {
			filePath := uris[0].Path()
			log.Printf("Dropped file: %s", filePath)

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

			// Скрываем overlay и загружаем файл
			mediaView.HideDragOverlay()
			mediaView.LoadMediaFile(filePath)
		}
	})
}

// createToolbar создает тулбар с кнопками
func createToolbar(w fyne.Window, mediaView *ui.MediaView) fyne.CanvasObject {
	openButton := widget.NewButtonWithIcon("Открыть файл", theme.FolderOpenIcon(), func() {
		dialog.ShowFileOpen(func(reader fyne.URIReadCloser, err error) {
			if err != nil {
				log.Printf("Error opening file: %v", err)
				return
			}
			if reader == nil {
				return // Пользователь отменил
			}
			defer reader.Close()

			filePath := reader.URI().Path()
			mediaView.LoadMediaFile(filePath)
		}, w)
	})

	// Фильтр для поддерживаемых форматов
	openButton.Importance = widget.HighImportance

	toolbar := container.NewHBox(
		openButton,
		widget.NewSeparator(),
		widget.NewLabel("Поддерживаемые форматы: MP4, MKV, MOV, MP3, WAV, M4A, FLAC"),
		layout.NewSpacer(),
	)

	return toolbar
}

// init инициализирует логирование и обработку ошибок
func init() {
	// Настройка логирования
	log.SetFlags(log.LstdFlags | log.Lshortfile)

	// Обработка паники
	defer func() {
		if r := recover(); r != nil {
			log.Printf("Fatal error: %v", r)
			os.Exit(1)
		}
	}()
}
