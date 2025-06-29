#pragma once
#include <QDialog>
#include <QMap>
#include <QString>

class QPushButton;
class QButtonGroup;
class QRadioButton;
class QLabel;
class QLineEdit;
class QFileDialog;

class WhisperModelSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit WhisperModelSettingsDialog(QWidget *parent = nullptr);
    QString selectedModel() const;

private slots:
    void onDownloadClicked(const QString &modelName);
    void onDeleteClicked(const QString &modelName);
    void onModelSelected();
    void updateModelStatus();
    void onCustomDownloadClicked();
    void onModelDirBrowseClicked();

private:
    void setupUi();
    void checkModelFiles();
    void downloadModel(const QString &modelName, const QString &url);
    void deleteModel(const QString &modelName);

    QMap<QString, QString> m_modelUrls; // modelName -> url
    QMap<QString, QPushButton*> m_downloadButtons;
    QMap<QString, QPushButton*> m_deleteButtons;
    QMap<QString, QLabel*> m_statusLabels;
    QButtonGroup *m_radioGroup;
    QString m_selectedModel;
    QLineEdit *m_customUrlEdit;
    QPushButton *m_customDownloadBtn;
    QLineEdit *m_modelDirEdit;
    QPushButton *m_modelDirBrowseBtn;
    QString m_modelDir;
}; 