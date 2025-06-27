package media

import (
	"fmt"
	"os/exec"
	"strings"
)

// FFmpegError представляет ошибку FFmpeg с дополнительной информацией
type FFmpegError struct {
	Command   string
	ExitCode  int
	Stderr    string
	Operation string
	Original  error
}

// Error возвращает строковое представление ошибки
func (e *FFmpegError) Error() string {
	if e.Original != nil {
		return fmt.Sprintf("FFmpeg %s failed: %v (exit code: %d)", e.Operation, e.Original, e.ExitCode)
	}
	return fmt.Sprintf("FFmpeg %s failed (exit code: %d): %s", e.Operation, e.ExitCode, e.Stderr)
}

// Unwrap возвращает оригинальную ошибку
func (e *FFmpegError) Unwrap() error {
	return e.Original
}

// IsFFmpegError проверяет, является ли ошибка FFmpegError
func IsFFmpegError(err error) bool {
	_, ok := err.(*FFmpegError)
	return ok
}

// NewFFmpegError создает новую ошибку FFmpeg
func NewFFmpegError(operation string, cmd *exec.Cmd, err error) *FFmpegError {
	var stderr string
	if cmd.Stderr != nil {
		// В реальной реализации нужно захватить stderr
		stderr = "stderr not captured"
	}

	var exitCode int
	if exitErr, ok := err.(*exec.ExitError); ok {
		exitCode = exitErr.ExitCode()
	}

	return &FFmpegError{
		Command:   strings.Join(cmd.Args, " "),
		ExitCode:  exitCode,
		Stderr:    stderr,
		Operation: operation,
		Original:  err,
	}
}

// CommonErrorMessages содержит сообщения для частых ошибок FFmpeg
var CommonErrorMessages = map[string]string{
	"Invalid data found when processing input": "Неподдерживаемый формат файла или поврежденный файл",
	"No such file or directory":                "Файл не найден",
	"Permission denied":                        "Нет прав доступа к файлу",
	"Invalid argument":                         "Неправильные параметры команды",
	"Operation not permitted":                  "Операция не разрешена",
	"Device or resource busy":                  "Устройство или ресурс занят",
}

// GetUserFriendlyMessage возвращает понятное пользователю сообщение об ошибке
func GetUserFriendlyMessage(err error) string {
	if ffmpegErr, ok := err.(*FFmpegError); ok {
		// Проверяем известные ошибки
		for pattern, message := range CommonErrorMessages {
			if strings.Contains(ffmpegErr.Stderr, pattern) {
				return message
			}
		}

		// Общие сообщения по кодам ошибок
		switch ffmpegErr.ExitCode {
		case 1:
			return "Ошибка обработки файла. Проверьте, что файл не поврежден и поддерживается."
		case 2:
			return "Ошибка в параметрах команды."
		case 126:
			return "FFmpeg не может быть выполнен. Проверьте права доступа."
		case 127:
			return "FFmpeg не найден в системе. Установите FFmpeg."
		default:
			return fmt.Sprintf("Ошибка FFmpeg (код %d): %s", ffmpegErr.ExitCode, ffmpegErr.Stderr)
		}
	}

	return err.Error()
}

// ValidateFFmpegInstallation проверяет установку FFmpeg и возвращает детальную информацию
func ValidateFFmpegInstallation() error {
	// Проверяем ffmpeg
	ffmpegPath, err := exec.LookPath("ffmpeg")
	if err != nil {
		return fmt.Errorf("ffmpeg not found in PATH: %w", err)
	}

	// Проверяем ffprobe
	_, err = exec.LookPath("ffprobe")
	if err != nil {
		return fmt.Errorf("ffprobe not found in PATH: %w", err)
	}

	// Проверяем версию ffmpeg
	cmd := exec.Command(ffmpegPath, "-version")
	output, err := cmd.Output()
	if err != nil {
		return fmt.Errorf("failed to get ffmpeg version: %w", err)
	}

	// Извлекаем версию из вывода
	lines := strings.Split(string(output), "\n")
	if len(lines) > 0 {
		versionLine := lines[0]
		if strings.Contains(versionLine, "ffmpeg version") {
			// Версия найдена, все в порядке
			return nil
		}
	}

	return fmt.Errorf("unable to determine ffmpeg version")
}
