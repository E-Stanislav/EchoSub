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

WhisperModelSettingsDialog::WhisperModelSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    // Примерные ссылки на модели (можно заменить на актуальные)
    m_modelUrls = {
        {"tiny",   "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin"},
        {"base",   "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin"},
        {"small",  "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.bin"},
        {"medium", "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.bin"},
        {"large",  "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large.bin"}
    };
    setupUi();
    checkModelFiles();
}

void WhisperModelSettingsDialog::setupUi()
{
    setWindowTitle("Настройки Whisper");
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    m_radioGroup = new QButtonGroup(this);
    int idx = 0;
    for (const auto &model : m_modelUrls.keys()) {
        QHBoxLayout *row = new QHBoxLayout();
        QRadioButton *radio = new QRadioButton(model, this);
        m_radioGroup->addButton(radio, idx++);
        row->addWidget(radio);
        QLabel *status = new QLabel("", this);
        m_statusLabels[model] = status;
        row->addWidget(status);
        QPushButton *download = new QPushButton("Скачать", this);
        m_downloadButtons[model] = download;
        row->addWidget(download);
        QPushButton *deleteBtn = new QPushButton("Удалить", this);
        m_deleteButtons[model] = deleteBtn;
        row->addWidget(deleteBtn);
        mainLayout->addLayout(row);
        connect(download, &QPushButton::clicked, this, [this, model]() { onDownloadClicked(model); });
        connect(deleteBtn, &QPushButton::clicked, this, [this, model]() { onDeleteClicked(model); });
    }
    connect(m_radioGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, &WhisperModelSettingsDialog::onModelSelected);
    QPushButton *okBtn = new QPushButton("OK", this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(okBtn);
}

void WhisperModelSettingsDialog::checkModelFiles()
{
    QDir modelDir(QDir::currentPath() + "/models/whisper");
    if (!modelDir.exists()) modelDir.mkpath(".");
    for (const auto &model : m_modelUrls.keys()) {
        QString filePath = modelDir.filePath("ggml-" + model + ".bin");
        if (QFile::exists(filePath)) {
            m_statusLabels[model]->setText("Скачано");
            m_downloadButtons[model]->setEnabled(false);
            m_deleteButtons[model]->setEnabled(true);
        } else {
            m_statusLabels[model]->setText("Не скачано");
            m_downloadButtons[model]->setEnabled(true);
            m_deleteButtons[model]->setEnabled(false);
        }
    }
    // Восстанавливаем выбранную модель
    QSettings s;
    QString sel = s.value("whisper/selected_model", "base").toString();
    for (auto *btn : m_radioGroup->buttons()) {
        btn->setChecked(btn->text() == sel);
    }
}

void WhisperModelSettingsDialog::onDownloadClicked(const QString &modelName)
{
    QString url = m_modelUrls[modelName];
    QDir modelDir(QDir::currentPath() + "/models/whisper");
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
        m_selectedModel = btn->text();
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
    QDir modelDir(QDir::currentPath() + "/models/whisper");
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