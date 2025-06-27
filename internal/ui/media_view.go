package ui

import (
	"fmt"
	"image"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/layout"
	"fyne.io/fyne/v2/widget"

	"github.com/echosub/echosub/internal/media"
)

// MediaView отображает медиа-файл с превью и тайм-линией
type MediaView struct {
	mediaInfo   *media.MediaInfo
	ffmpeg      *media.FFmpegProcessor
	player      *media.MediaPlayer
	videoPlayer *media.InlineVideoPlayer

	// UI компоненты
	container           *fyne.Container
	previewImage        *canvas.Image
	videoImage          *canvas.Image // для покадрового видео
	waveformImage       *canvas.Image
	infoLabel           *widget.Label
	loadingSpinner      *widget.ProgressBar
	waveformPlaceholder *widget.Label
	dragOverlay         *widget.Label // Индикатор drag-and-drop

	// Контролы плеера
	playButton     *widget.Button
	stopButton     *widget.Button
	progressSlider *widget.Slider
	timeLabel      *widget.Label
	playerControls *fyne.Container

	// Кнопки оптимизации видео
	optimizeFaststartButton *widget.Button
	optimizeKeyframesButton *widget.Button
	optimizeHwButton        *widget.Button
	optimizationControls    *fyne.Container

	// Callbacks
	onMediaLoaded func(*media.MediaFile)
	onError       func(error)
}

// NewMediaView создает новый вид медиа-файла
func NewMediaView(ffmpeg *media.FFmpegProcessor) *MediaView {
	mv := &MediaView{
		ffmpeg: ffmpeg,
	}

	// Создаем медиаплеер для аудио
	if ffmpeg != nil {
		mv.player = media.NewMediaPlayer(ffmpeg)
		mv.player.SetCallbacks(
			mv.onPlayStateChanged,
			mv.onPositionChanged,
			mv.onPlayerError,
		)
	}

	// Создаем встроенный видеоплеер
	mv.videoPlayer = media.NewInlineVideoPlayer()
	mv.videoPlayer.SetCallbacks(
		mv.onVideoFrameReady,
		mv.onVideoError,
		mv.onVideoPlayStateChanged,
		mv.onVideoProgressChanged,
	)

	mv.createUI()
	return mv
}

// createUI создает пользовательский интерфейс
func (mv *MediaView) createUI() {
	// Создаем компоненты
	mv.previewImage = &canvas.Image{}
	mv.previewImage.FillMode = canvas.ImageFillContain
	mv.previewImage.SetMinSize(fyne.NewSize(400, 300))

	mv.videoImage = &canvas.Image{} // для покадрового видео
	mv.videoImage.FillMode = canvas.ImageFillContain
	mv.videoImage.SetMinSize(fyne.NewSize(400, 300))

	mv.waveformImage = &canvas.Image{}
	mv.waveformImage.FillMode = canvas.ImageFillStretch
	mv.waveformImage.SetMinSize(fyne.NewSize(800, 100))

	mv.infoLabel = widget.NewLabel("Перетащите медиа-файл или нажмите 'Открыть файл'")
	mv.infoLabel.Alignment = fyne.TextAlignCenter

	mv.loadingSpinner = widget.NewProgressBar()
	mv.loadingSpinner.Hide()

	mv.waveformPlaceholder = widget.NewLabel("Загрузка волнограммы...")
	mv.waveformPlaceholder.Alignment = fyne.TextAlignCenter

	mv.dragOverlay = widget.NewLabel("Перетащите файл сюда")
	mv.dragOverlay.Alignment = fyne.TextAlignCenter

	// Создаем контролы плеера
	mv.createPlayerControls()

	// Создаем контролы оптимизации
	mv.createOptimizationControls()

	// Создаем контейнер
	mv.container = container.NewVBox(
		container.NewHBox(
			layout.NewSpacer(),
			mv.videoImage, // всегда videoImage (будет пустым для аудио)
			layout.NewSpacer(),
		),
		mv.infoLabel,
		mv.loadingSpinner,
		mv.playerControls,       // Контролы плеера
		mv.optimizationControls, // Контролы оптимизации
		container.NewHBox(
			layout.NewSpacer(),
			mv.waveformPlaceholder,
			layout.NewSpacer(),
		),
		mv.dragOverlay,
	)
}

// GetContent возвращает контейнер для отображения
func (mv *MediaView) GetContent() fyne.CanvasObject {
	return mv.container
}

// LoadMediaFile загружает медиа-файл
func (mv *MediaView) LoadMediaFile(filePath string) {
	// Показываем спиннер загрузки
	fyne.Do(func() {
		mv.loadingSpinner.Show()
		mv.loadingSpinner.SetValue(0)
		mv.infoLabel.SetText("Загрузка файла...")
	})

	// Загружаем в горутине, чтобы не блокировать UI
	go mv.loadMediaFileAsync(filePath)
}

// loadMediaFileAsync асинхронно загружает медиа-файл
func (mv *MediaView) loadMediaFileAsync(filePath string) {
	// Проверяем наличие FFmpeg
	if mv.ffmpeg == nil {
		mv.handleError(fmt.Errorf("FFmpeg не доступен"))
		return
	}

	// Загружаем базовую информацию
	mediaFile, err := mv.ffmpeg.LoadMediaFile(filePath)
	if err != nil {
		mv.handleError(err)
		return
	}

	// ВАЖНО: присваиваем mediaInfo для корректной работы контролов
	mv.mediaInfo = &media.MediaInfo{File: mediaFile}

	// Загружаем файл в плеер
	if mv.player != nil {
		if err := mv.player.LoadMedia(mediaFile); err != nil {
			mv.handleError(fmt.Errorf("failed to load media in player: %w", err))
			return
		}
	}

	// Загружаем видео в встроенный видеоплеер, если это видео файл
	if mediaFile.HasVideo && mv.videoPlayer != nil {
		fyne.Do(func() {
			mv.loadingSpinner.SetValue(0.2)
			mv.infoLabel.SetText("Загрузка видео в плеер...")
		})

		if err := mv.videoPlayer.LoadVideo(mediaFile); err != nil {
			// Логируем ошибку, но продолжаем работу
			fmt.Printf("Warning: failed to load video in inline player: %v\n", err)
		}
	}

	// Обновляем UI с базовой информацией
	mv.updateBasicInfo(mediaFile)

	// Показываем контролы плеера
	fyne.Do(func() {
		mv.playerControls.Show()
		mv.playButton.Enable()
		mv.stopButton.Enable()
		mv.progressSlider.Enable()

		// Обновляем время
		info := mv.player.GetPlaybackInfo()
		mv.timeLabel.SetText(info.TimeString)
	})

	var wg sync.WaitGroup

	if mediaFile.HasVideo {
		wg.Add(1)
		go func() {
			defer wg.Done()
			fyne.Do(func() {
				mv.loadingSpinner.SetValue(0.3)
				mv.infoLabel.SetText("Извлечение превью...")
			})

			posterPath, err := mv.ffmpeg.ExtractPosterFrame(mediaFile)
			if err == nil {
				mv.updatePreview(posterPath)
			} else {
				fmt.Printf("Warning: failed to extract poster frame: %v\n", err)
			}
		}()
	}

	if mediaFile.HasAudio {
		wg.Add(1)
		go func() {
			defer wg.Done()
			fyne.Do(func() {
				mv.loadingSpinner.SetValue(0.6)
				mv.infoLabel.SetText("Генерация волнограммы...")
			})

			waveformPath, err := mv.ffmpeg.GenerateWaveform(mediaFile)
			if err == nil {
				mv.updateWaveform(waveformPath)
			} else {
				fmt.Printf("Warning: failed to generate waveform: %v\n", err)
			}
		}()
	}

	go func() {
		wg.Wait()
		fyne.Do(func() {
			mv.loadingSpinner.SetValue(1.0)
			mv.loadingSpinner.Hide()
		})
		mv.updateFinalInfo(mediaFile)

		if mv.onMediaLoaded != nil {
			mv.onMediaLoaded(mediaFile)
		}
	}()
}

// updateBasicInfo обновляет базовую информацию о файле
func (mv *MediaView) updateBasicInfo(mediaFile *media.MediaFile) {
	info := fmt.Sprintf("Файл: %s\nФормат: %s\nРазмер: %s",
		mediaFile.Name,
		strings.ToUpper(mediaFile.Format),
		formatFileSize(mediaFile.Size))

	if mediaFile.Duration > 0 {
		info += fmt.Sprintf("\nДлительность: %s", formatDuration(mediaFile.Duration))
	}

	fyne.Do(func() {
		mv.infoLabel.SetText(info)
	})
}

// updatePreview обновляет превью
func (mv *MediaView) updatePreview(imagePath string) {
	fyne.Do(func() {
		// Показываем превью только если нет видео
		if mv.mediaInfo == nil || mv.mediaInfo.File == nil || !mv.mediaInfo.File.HasVideo {
			mv.previewImage.File = imagePath
			mv.previewImage.Refresh()
		}
	})
}

// updateWaveform обновляет волнограмму
func (mv *MediaView) updateWaveform(imagePath string) {
	fyne.Do(func() {
		// Скрываем placeholder
		mv.waveformPlaceholder.Hide()

		// Показываем реальную волнограмму
		mv.waveformImage.File = imagePath
		mv.waveformImage.Show()
		mv.waveformImage.Refresh()

		// Заменяем placeholder на реальную волнограмму в контейнере
		// (в реальной реализации нужно обновить контейнер)
	})
}

// updateFinalInfo обновляет финальную информацию
func (mv *MediaView) updateFinalInfo(mediaFile *media.MediaFile) {
	info := fmt.Sprintf("✓ %s\nФормат: %s • Размер: %s",
		mediaFile.Name,
		strings.ToUpper(mediaFile.Format),
		formatFileSize(mediaFile.Size))

	if mediaFile.Duration > 0 {
		info += fmt.Sprintf(" • Длительность: %s", formatDuration(mediaFile.Duration))
	}

	if mediaFile.HasVideo {
		info += fmt.Sprintf("\nВидео: %s (%dx%d)", mediaFile.VideoCodec, mediaFile.Width, mediaFile.Height)
	}

	if mediaFile.HasAudio {
		info += fmt.Sprintf("\nАудио: %s (%d Hz, %d каналов)",
			mediaFile.AudioCodec, mediaFile.SampleRate, mediaFile.Channels)
	}

	fyne.Do(func() {
		mv.infoLabel.SetText(info)

		// Показываем кнопки оптимизации только для видео файлов
		if mediaFile.HasVideo {
			mv.optimizationControls.Show()
			mv.optimizeFaststartButton.Enable()
			mv.optimizeKeyframesButton.Enable()
			mv.optimizeHwButton.Enable()
		} else {
			mv.optimizationControls.Hide()
		}
	})
}

// handleError обрабатывает ошибки
func (mv *MediaView) handleError(err error) {
	fyne.Do(func() {
		mv.loadingSpinner.Hide()

		// Используем улучшенные сообщения об ошибках
		userMessage := media.GetUserFriendlyMessage(err)
		mv.infoLabel.SetText(fmt.Sprintf("Ошибка: %s", userMessage))
	})

	if mv.onError != nil {
		mv.onError(err)
	}
}

// SetCallbacks устанавливает callbacks
func (mv *MediaView) SetCallbacks(onMediaLoaded func(*media.MediaFile), onError func(error)) {
	mv.onMediaLoaded = onMediaLoaded
	mv.onError = onError
}

// ShowDragOverlay показывает индикатор drag-and-drop
func (mv *MediaView) ShowDragOverlay() {
	fyne.Do(func() {
		mv.dragOverlay.Show()
		mv.dragOverlay.SetText("Перетащите файл сюда")
	})
}

// HideDragOverlay скрывает индикатор drag-and-drop
func (mv *MediaView) HideDragOverlay() {
	fyne.Do(func() {
		mv.dragOverlay.Hide()
	})
}

// ShowDragOverlayValid показывает индикатор для валидного файла
func (mv *MediaView) ShowDragOverlayValid() {
	fyne.Do(func() {
		mv.dragOverlay.Show()
		mv.dragOverlay.SetText("✓ Отпустите для загрузки")
	})
}

// onPlayStateChanged обрабатывает изменение состояния воспроизведения
func (mv *MediaView) onPlayStateChanged(isPlaying bool) {
	fyne.Do(func() {
		if isPlaying {
			mv.playButton.SetText("⏸ Пауза")
		} else {
			mv.playButton.SetText("▶ Воспроизвести")
		}
	})
}

// onPositionChanged обрабатывает изменение позиции воспроизведения
func (mv *MediaView) onPositionChanged(position time.Duration) {
	fyne.Do(func() {
		info := mv.player.GetPlaybackInfo()
		mv.timeLabel.SetText(info.TimeString)
		mv.progressSlider.SetValue(info.Progress)
	})
}

// onPlayerError обрабатывает ошибки плеера
func (mv *MediaView) onPlayerError(err error) {
	fyne.Do(func() {
		mv.infoLabel.SetText(fmt.Sprintf("Ошибка воспроизведения: %s", err.Error()))
	})
}

// onVideoFrameReady обрабатывает готовность видеокадра
func (mv *MediaView) onVideoFrameReady(frame *image.Image) {
	fyne.Do(func() {
		if img, ok := (*frame).(*image.RGBA); ok {
			mv.videoImage.Image = img
			mv.videoImage.Refresh()
		}
	})
}

// onVideoError обрабатывает ошибки видеоплеера
func (mv *MediaView) onVideoError(err error) {
	fyne.Do(func() {
		mv.infoLabel.SetText(fmt.Sprintf("Ошибка видео: %s", err.Error()))
	})
}

// onVideoPlayStateChanged обрабатывает изменение состояния видеоплеера
func (mv *MediaView) onVideoPlayStateChanged(isPlaying bool) {
	fyne.Do(func() {
		if isPlaying {
			mv.playButton.SetText("⏸ Пауза")
		} else {
			mv.playButton.SetText("▶ Воспроизвести")
		}
	})
}

// onVideoProgressChanged обрабатывает изменение прогресса видеоплеера
func (mv *MediaView) onVideoProgressChanged(progress float64) {
	fyne.Do(func() {
		mv.progressSlider.SetValue(progress)
	})
}

// Вспомогательные функции

func formatFileSize(size int64) string {
	const unit = 1024
	if size < unit {
		return fmt.Sprintf("%d B", size)
	}
	div, exp := int64(unit), 0
	for n := size / unit; n >= unit; n /= unit {
		div *= unit
		exp++
	}
	return fmt.Sprintf("%.1f %cB", float64(size)/float64(div), "KMGTPE"[exp])
}

func formatDuration(d time.Duration) string {
	hours := int(d.Hours())
	minutes := int(d.Minutes()) % 60
	seconds := int(d.Seconds()) % 60

	if hours > 0 {
		return fmt.Sprintf("%02d:%02d:%02d", hours, minutes, seconds)
	}
	return fmt.Sprintf("%02d:%02d", minutes, seconds)
}

// createPlayerControls создает контролы медиаплеера
func (mv *MediaView) createPlayerControls() {
	// Кнопка Play/Pause
	mv.playButton = widget.NewButton("▶ Воспроизвести", func() {
		// Используем встроенный видеоплеер для видео, обычный для аудио
		if mv.videoPlayer != nil && mv.mediaInfo != nil && mv.mediaInfo.File.HasVideo {
			if mv.videoPlayer.IsPlaying() {
				mv.videoPlayer.Pause()
			} else {
				mv.videoPlayer.Play()
			}
		} else if mv.player != nil {
			if mv.player.IsPlaying() {
				mv.player.Pause()
			} else {
				mv.player.Play()
			}
		}
	})
	mv.playButton.Disable() // Изначально отключена

	// Кнопка Stop
	mv.stopButton = widget.NewButton("⏹ Стоп", func() {
		if mv.videoPlayer != nil && mv.mediaInfo != nil && mv.mediaInfo.File.HasVideo {
			mv.videoPlayer.Stop()
		} else if mv.player != nil {
			mv.player.Stop()
		}
	})
	mv.stopButton.Disable() // Изначально отключена

	// Прогресс-бар
	mv.progressSlider = widget.NewSlider(0, 100)
	mv.progressSlider.OnChanged = func(value float64) {
		// Используем соответствующий плеер для перемотки
		if mv.videoPlayer != nil && mv.mediaInfo != nil && mv.mediaInfo.File.HasVideo {
			duration := mv.videoPlayer.GetDuration()
			position := time.Duration(float64(duration) * value / 100)
			// Запускаем перемотку в горутине, чтобы не блокировать UI
			go func() {
				if err := mv.videoPlayer.Seek(position); err != nil && mv.onError != nil {
					mv.onError(fmt.Errorf("seek failed: %w", err))
				}
			}()
		} else if mv.player != nil {
			duration := mv.player.GetDuration()
			position := time.Duration(float64(duration) * value / 100)
			mv.player.Seek(position)
		}
	}
	mv.progressSlider.Disable() // Изначально отключен

	// Метка времени
	mv.timeLabel = widget.NewLabel("00:00 / 00:00")
	mv.timeLabel.Alignment = fyne.TextAlignCenter

	// Контейнер с контролами
	mv.playerControls = container.NewVBox(
		container.NewHBox(
			mv.playButton,
			mv.stopButton,
			layout.NewSpacer(),
			mv.timeLabel,
		),
		mv.progressSlider,
	)

	// Изначально скрываем контролы
	mv.playerControls.Hide()
}

// createOptimizationControls создает контролы оптимизации видео
func (mv *MediaView) createOptimizationControls() {
	// Кнопка оптимизации Faststart
	mv.optimizeFaststartButton = widget.NewButton("🚀 Faststart", func() {
		mv.optimizeVideoFaststart()
	})
	mv.optimizeFaststartButton.Disable()

	// Кнопка оптимизации Keyframes
	mv.optimizeKeyframesButton = widget.NewButton("🎯 Keyframes", func() {
		mv.optimizeVideoKeyframes()
	})
	mv.optimizeKeyframesButton.Disable()

	// Кнопка оптимизации HW
	mv.optimizeHwButton = widget.NewButton("⚡ HW", func() {
		mv.optimizeVideoHardware()
	})
	mv.optimizeHwButton.Disable()

	// Контейнер с контролами оптимизации
	mv.optimizationControls = container.NewHBox(
		widget.NewLabel("Оптимизация:"),
		mv.optimizeFaststartButton,
		mv.optimizeKeyframesButton,
		mv.optimizeHwButton,
		layout.NewSpacer(),
	)

	// Изначально скрываем контролы оптимизации
	mv.optimizationControls.Hide()
}

// optimizeVideoFaststart оптимизирует видео для быстрого старта
func (mv *MediaView) optimizeVideoFaststart() {
	if mv.mediaInfo == nil || mv.mediaInfo.File == nil {
		return
	}

	// Показываем прогресс
	fyne.Do(func() {
		mv.loadingSpinner.Show()
		mv.loadingSpinner.SetValue(0)
		mv.infoLabel.SetText("Оптимизация для быстрого старта...")
		mv.optimizeFaststartButton.Disable()
	})

	go func() {
		// Создаем временный файл для результата
		tempDir := filepath.Dir(mv.mediaInfo.File.Path)
		outputPath := filepath.Join(tempDir, "optimized_"+filepath.Base(mv.mediaInfo.File.Path))

		// Выполняем оптимизацию
		err := mv.ffmpeg.OptimizeVideoForFastStart(mv.mediaInfo.File.Path, outputPath)

		fyne.Do(func() {
			mv.loadingSpinner.Hide()
			mv.optimizeFaststartButton.Enable()

			if err != nil {
				mv.infoLabel.SetText(fmt.Sprintf("Ошибка оптимизации: %s", err.Error()))
				if mv.onError != nil {
					mv.onError(err)
				}
			} else {
				mv.infoLabel.SetText("✅ Видео оптимизировано для быстрого старта")
				// Обновляем путь к файлу
				mv.mediaInfo.File.Path = outputPath
			}
		})
	}()
}

// optimizeVideoKeyframes оптимизирует видео с частыми ключевыми кадрами
func (mv *MediaView) optimizeVideoKeyframes() {
	if mv.mediaInfo == nil || mv.mediaInfo.File == nil {
		return
	}

	// Показываем прогресс
	fyne.Do(func() {
		mv.loadingSpinner.Show()
		mv.loadingSpinner.SetValue(0)
		mv.infoLabel.SetText("Перекодирование с частыми ключевыми кадрами...")
		mv.optimizeKeyframesButton.Disable()
	})

	go func() {
		// Создаем временный файл для результата
		tempDir := filepath.Dir(mv.mediaInfo.File.Path)
		outputPath := filepath.Join(tempDir, "keyframes_"+filepath.Base(mv.mediaInfo.File.Path))

		// Выполняем оптимизацию
		err := mv.ffmpeg.ReencodeWithFrequentKeyframes(mv.mediaInfo.File.Path, outputPath)

		fyne.Do(func() {
			mv.loadingSpinner.Hide()
			mv.optimizeKeyframesButton.Enable()

			if err != nil {
				mv.infoLabel.SetText(fmt.Sprintf("Ошибка перекодирования: %s", err.Error()))
				if mv.onError != nil {
					mv.onError(err)
				}
			} else {
				mv.infoLabel.SetText("✅ Видео перекодировано с частыми ключевыми кадрами")
				// Обновляем путь к файлу
				mv.mediaInfo.File.Path = outputPath
			}
		})
	}()
}

// optimizeVideoHardware оптимизирует видео с аппаратным ускорением
func (mv *MediaView) optimizeVideoHardware() {
	if mv.mediaInfo == nil || mv.mediaInfo.File == nil {
		return
	}

	// Показываем прогресс
	fyne.Do(func() {
		mv.loadingSpinner.Show()
		mv.loadingSpinner.SetValue(0)
		mv.infoLabel.SetText("Оптимизация с аппаратным ускорением...")
		mv.optimizeHwButton.Disable()
	})

	go func() {
		// Создаем временный файл для результата
		tempDir := filepath.Dir(mv.mediaInfo.File.Path)
		outputPath := filepath.Join(tempDir, "hw_"+filepath.Base(mv.mediaInfo.File.Path))

		// Выполняем оптимизацию
		err := mv.ffmpeg.OptimizeVideoWithHardwareAcceleration(mv.mediaInfo.File.Path, outputPath)

		fyne.Do(func() {
			mv.loadingSpinner.Hide()
			mv.optimizeHwButton.Enable()

			if err != nil {
				mv.infoLabel.SetText(fmt.Sprintf("Ошибка аппаратной оптимизации: %s", err.Error()))
				if mv.onError != nil {
					mv.onError(err)
				}
			} else {
				mv.infoLabel.SetText("✅ Видео оптимизировано с аппаратным ускорением")
				// Обновляем путь к файлу
				mv.mediaInfo.File.Path = outputPath
			}
		})
	}()
}
