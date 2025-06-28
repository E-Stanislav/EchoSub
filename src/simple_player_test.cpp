#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include "core/simplemediaplayer.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    SimpleMediaPlayer player;
    player.show();
    
    // Если передан аргумент командной строки, открываем файл
    if (argc > 1) {
        QString filePath = QString::fromLocal8Bit(argv[1]);
        if (!player.openFile(filePath)) {
            QMessageBox::warning(&player, "Error", "Failed to open file: " + filePath);
        }
    }
    // Убираем автоматическое открытие диалога - теперь пользователь может использовать кнопку "Open File"
    
    return app.exec();
} 