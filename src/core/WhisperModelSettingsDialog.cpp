#include "WhisperModelSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>

// Функция для парсинга размера модели в байты
qint64 parseModelSize(const QString &sizeStr) {
    QRegularExpression re(R"((\d+(?:\.\d+)?)\s*(MB|GB))");
    QRegularExpressionMatch match = re.match(sizeStr);
    if (match.hasMatch()) {
        double value = match.captured(1).toDouble();
        QString unit = match.captured(2);
        if (unit == "GB") {
            return static_cast<qint64>(value * 1024 * 1024 * 1024);
        } else if (unit == "MB") {
            return static_cast<qint64>(value * 1024 * 1024);
        }
    }
    return 0;
}

// Функция для сортировки моделей по размеру
void sortModelsBySize(QList<ModelInfo> &models) {
    std::sort(models.begin(), models.end(), [](const ModelInfo &a, const ModelInfo &b) {
        return parseModelSize(a.size) < parseModelSize(b.size);
    });
}

WhisperModelSettingsDialog::WhisperModelSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    QSettings s;
    m_modelDir = s.value("whisper/model_dir", QDir::currentPath() + "/models/whisper").toString();
    
    // Модели с размерами (порядок не важен, будет отсортирован автоматически)
    m_models = {
        {"tiny",   "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin", "39 MB"},
        {"base",   "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin", "142 MB"},
        {"small",  "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin", "466 MB"},
        {"medium", "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin", "1.42 GB"},
        {"large",  "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3.bin", "3.82 GB"},
        {"large-v3-turbo",  "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo.bin", "1.62 GB"},
    };
    
    // Автоматическая сортировка по размеру от меньшего к большему
    sortModelsBySize(m_models);
    
    setupUi();
    checkModelFiles();
}

void WhisperModelSettingsDialog::setupUi()
{
    setWindowTitle("Настройки Whisper");
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // --- Ссылка на Hugging Face ---
    QLabel *linkLabel = new QLabel("<a href=\"https://huggingface.co/models?other=whisper&sort=downloads\">Список моделей Whisper на Hugging Face</a>", this);
    linkLabel->setTextFormat(Qt::RichText);
    linkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    linkLabel->setOpenExternalLinks(true);
    mainLayout->addWidget(linkLabel);

    // --- Каталог моделей ---
    QHBoxLayout *dirLayout = new QHBoxLayout();
    m_modelDirEdit = new QLineEdit(m_modelDir, this);
    m_modelDirBrowseBtn = new QPushButton("...", this);
    dirLayout->addWidget(new QLabel("Каталог моделей:", this));
    dirLayout->addWidget(m_modelDirEdit);
    dirLayout->addWidget(m_modelDirBrowseBtn);
    mainLayout->addLayout(dirLayout);
    connect(m_modelDirBrowseBtn, &QPushButton::clicked, this, &WhisperModelSettingsDialog::onModelDirBrowseClicked);

    // --- Пользовательская ссылка ---
    QHBoxLayout *customUrlLayout = new QHBoxLayout();
    m_customUrlEdit = new QLineEdit(this);
    m_customDownloadBtn = new QPushButton("Скачать по ссылке", this);
    customUrlLayout->addWidget(new QLabel("Ссылка на модель:", this));
    customUrlLayout->addWidget(m_customUrlEdit);
    customUrlLayout->addWidget(m_customDownloadBtn);
    mainLayout->addLayout(customUrlLayout);
    connect(m_customDownloadBtn, &QPushButton::clicked, this, &WhisperModelSettingsDialog::onCustomDownloadClicked);

    m_radioGroup = new QButtonGroup(this);
    int idx = 0;
    for (const auto &modelInfo : m_models) {
        QHBoxLayout *row = new QHBoxLayout();
        QString displayName = modelInfo.name + " (" + modelInfo.size + ")";
        QRadioButton *radio = new QRadioButton(displayName, this);
        m_radioGroup->addButton(radio, idx++);
        row->addWidget(radio);
        QLabel *status = new QLabel("", this);
        m_statusLabels[modelInfo.name] = status;
        row->addWidget(status);
        QPushButton *download = new QPushButton("Скачать", this);
        m_downloadButtons[modelInfo.name] = download;
        row->addWidget(download);
        QPushButton *deleteBtn = new QPushButton("Удалить", this);
        m_deleteButtons[modelInfo.name] = deleteBtn;
        row->addWidget(deleteBtn);
        mainLayout->addLayout(row);
        connect(download, &QPushButton::clicked, this, [this, modelInfo]() { onDownloadClicked(modelInfo.name); });
        connect(deleteBtn, &QPushButton::clicked, this, [this, modelInfo]() { onDeleteClicked(modelInfo.name); });
    }
    connect(m_radioGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, &WhisperModelSettingsDialog::onModelSelected);
    QPushButton *okBtn = new QPushButton("OK", this);
    connect(okBtn, &QPushButton::clicked, this, [this]() {
        // Сохраняем каталог моделей
        QSettings s;
        m_modelDir = m_modelDirEdit->text();
        s.setValue("whisper/model_dir", m_modelDir);
        accept();
    });
    mainLayout->addWidget(okBtn);
}

void WhisperModelSettingsDialog::checkModelFiles()
{
    QDir modelDir(m_modelDir);
    if (!modelDir.exists()) modelDir.mkpath(".");
    for (const auto &modelInfo : m_models) {
        QString filePath = modelDir.filePath("ggml-" + modelInfo.name + ".bin");
        if (QFile::exists(filePath)) {
            m_statusLabels[modelInfo.name]->setText("Скачано");
            m_downloadButtons[modelInfo.name]->setEnabled(false);
            m_deleteButtons[modelInfo.name]->setEnabled(true);
        } else {
            m_statusLabels[modelInfo.name]->setText("Не скачано");
            m_downloadButtons[modelInfo.name]->setEnabled(true);
            m_deleteButtons[modelInfo.name]->setEnabled(false);
        }
    }
    QSettings s;
    QString sel = s.value("whisper/selected_model", "base").toString();
    for (auto *btn : m_radioGroup->buttons()) {
        btn->setChecked(btn->text().contains(sel));
    }
}

void WhisperModelSettingsDialog::onCustomDownloadClicked()
{
    QString url = m_customUrlEdit->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите ссылку на модель");
        return;
    }
    QString fileName = url.section('/', -1);
    if (!fileName.endsWith(".bin")) {
        QMessageBox::warning(this, "Ошибка", "Ссылка должна указывать на .bin файл модели");
        return;
    }
    QDir modelDir(m_modelDirEdit->text());
    if (!modelDir.exists()) modelDir.mkpath(".");
    QString filePath = modelDir.filePath(fileName);
    downloadModel(fileName, url);
}

void WhisperModelSettingsDialog::onModelDirBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Выберите каталог моделей", m_modelDirEdit->text());
    if (!dir.isEmpty()) {
        m_modelDirEdit->setText(dir);
        m_modelDir = dir;
        QSettings s;
        s.setValue("whisper/model_dir", dir);
        checkModelFiles();
    }
}

void WhisperModelSettingsDialog::downloadModel(const QString &modelName, const QString &url)
{
    QDir modelDir(m_modelDirEdit ? m_modelDirEdit->text() : m_modelDir);
    QString filePath = modelDir.filePath("ggml-" + modelName + ".bin");
    QNetworkAccessManager *nam = new QNetworkAccessManager(this);
    QNetworkReply *reply = nam->get(QNetworkRequest(QUrl(url)));
    QProgressDialog *progress = new QProgressDialog("Скачивание " + modelName, "Отмена", 0, 100, this);
    progress->setWindowModality(Qt::WindowModal);
    connect(reply, &QNetworkReply::downloadProgress, this, [progress](qint64 rec, qint64 total) {
        if (total > 0) progress->setValue(int(100.0 * rec / total));
    });
    connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        progress->close();
        if (reply->error() == QNetworkReply::NoError) {
            QFile f(filePath);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(reply->readAll());
                f.close();
                QMessageBox::information(this, "Успех", "Модель " + modelName + " скачана!");
            }
        } else {
            QMessageBox::warning(this, "Ошибка", "Ошибка скачивания: " + reply->errorString());
        }
        reply->deleteLater();
        nam->deleteLater();
        updateModelStatus();
    });
    progress->show();
}

void WhisperModelSettingsDialog::onDownloadClicked(const QString &modelName)
{
    // Находим URL для указанной модели
    QString url;
    for (const auto &modelInfo : m_models) {
        if (modelInfo.name == modelName) {
            url = modelInfo.url;
            break;
        }
    }
    
    if (url.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Модель не найдена: " + modelName);
        return;
    }
    
    QDir modelDir(m_modelDirEdit ? m_modelDirEdit->text() : m_modelDir);
    QString filePath = modelDir.filePath("ggml-" + modelName + ".bin");
    QNetworkAccessManager *nam = new QNetworkAccessManager(this);
    QNetworkReply *reply = nam->get(QNetworkRequest(QUrl(url)));
    QProgressDialog *progress = new QProgressDialog("Скачивание " + modelName, "Отмена", 0, 100, this);
    progress->setWindowModality(Qt::WindowModal);
    connect(reply, &QNetworkReply::downloadProgress, this, [progress](qint64 rec, qint64 total) {
        if (total > 0) progress->setValue(int(100.0 * rec / total));
    });
    connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        progress->close();
        if (reply->error() == QNetworkReply::NoError) {
            QFile f(filePath);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(reply->readAll());
                f.close();
                QMessageBox::information(this, "Успех", "Модель " + modelName + " скачана!");
            }
        } else {
            QMessageBox::warning(this, "Ошибка", "Ошибка скачивания: " + reply->errorString());
        }
        reply->deleteLater();
        nam->deleteLater();
        updateModelStatus();
    });
    progress->show();
}

void WhisperModelSettingsDialog::onModelSelected()
{
    QAbstractButton *btn = m_radioGroup->checkedButton();
    if (btn) {
        m_selectedModel = btn->text().split(" (").first();
        QSettings s;
        s.setValue("whisper/selected_model", m_selectedModel);
    }
}

void WhisperModelSettingsDialog::updateModelStatus()
{
    checkModelFiles();
}

void WhisperModelSettingsDialog::onDeleteClicked(const QString &modelName)
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, 
        "Подтверждение удаления", 
        "Вы уверены, что хотите удалить модель '" + modelName + "'?",
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        deleteModel(modelName);
    }
}

void WhisperModelSettingsDialog::deleteModel(const QString &modelName)
{
    QDir modelDir(m_modelDirEdit ? m_modelDirEdit->text() : m_modelDir);
    QString filePath = modelDir.filePath("ggml-" + modelName + ".bin");
    
    if (QFile::exists(filePath)) {
        if (QFile::remove(filePath)) {
            QMessageBox::information(this, "Успех", "Модель '" + modelName + "' удалена!");
            updateModelStatus();
        } else {
            QMessageBox::warning(this, "Ошибка", "Не удалось удалить модель '" + modelName + "'");
        }
    } else {
        QMessageBox::warning(this, "Ошибка", "Файл модели '" + modelName + "' не найден");
    }
}

QString WhisperModelSettingsDialog::selectedModel() const
{
    return m_selectedModel;
} 