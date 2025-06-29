#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDir>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QMessageBox>
#include <QIcon>

#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application info
    app.setApplicationName("EchoSub");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("EchoSub");
    app.setOrganizationDomain("echosub.com");

    // Set application icon
    QIcon appIcon(":/icons/app_image.png");
    app.setWindowIcon(appIcon);

    // Set application style
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // Command line parser
    QCommandLineParser parser;
    parser.setApplicationDescription("EchoSub - Modern Media Player");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption fileOption(QStringList() << "f" << "file",
                                 "Open media file", "file");
    parser.addOption(fileOption);
    
    parser.process(app);
    
    // Create and show main window
    MainWindow window;
    window.setWindowIcon(appIcon);
    window.show();
    
    // Open file if specified
    if (parser.isSet(fileOption)) {
        QString filePath = parser.value(fileOption);
        if (QFile::exists(filePath)) {
            window.loadFile(filePath);
        } else {
            QMessageBox::warning(&window, "File Not Found", 
                               QString("File not found: %1").arg(filePath));
        }
    }
    
    return app.exec();
} 