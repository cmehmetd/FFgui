// FFgui — Qt + C++ ile yazılmış FFmpeg arayüzü.
// Copyright (C) 2026 FFgui contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMainWindow>
#include <QProcess>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QProgressBar;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class QTabWidget;
class QGroupBox;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void browseInput();
    void browseOutput();
    void updateCommandPreview();
    void startConversion();
    void stopConversion();
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onInputChanged();
    void onDistroChanged(int index);
    void onTopTabChanged(int index);
    void installWindowsFfmpeg();
    void installLinuxFfmpeg();
    void onInstallFinished(int exitCode, QProcess::ExitStatus status);
    void refreshFfmpegStatus();

private:
    QString ffmpegBinary() const { return m_ffmpegPath; }
    QString ffprobeBinary() const { return m_ffprobePath; }
    QStringList buildArguments() const;
    QString commandPreviewText() const;
    void setRunning(bool running);
    void setInstalling(bool installing);
    QString distroDefaultCommand(const QString &distroKey) const;
    double durationToSeconds(const QString &d) const;
    void probeInputFile(const QString &path);

    QString m_ffmpegPath = QStringLiteral("ffmpeg");
    QString m_ffprobePath = QStringLiteral("ffprobe");
    double m_totalSeconds = 0.0;

    // Üst sekmeler: yan yana Kurulum ile Anasayfa
    QTabWidget *m_topTabs = nullptr;
    QWidget *m_setupPage = nullptr;
    QWidget *m_ioPage = nullptr;
    QTabWidget *m_optionsTabs = nullptr;
    QWidget *m_bottomWidget = nullptr;
    QGroupBox *m_winGroup = nullptr;
    QGroupBox *m_linuxGroup = nullptr;
    QLabel *m_ffmpegStatusLabel = nullptr;
    QLineEdit *m_winCommandEdit = nullptr;
    QPushButton *m_winInstallButton = nullptr;
    QComboBox *m_distroCombo = nullptr;
    QLineEdit *m_linuxCommandEdit = nullptr;
    QPushButton *m_linuxInstallButton = nullptr;

    // Girdi / çıktı (üzerine yazma seçeneği yok — orijinal korunur)
    QLineEdit *m_inputEdit = nullptr;
    QLineEdit *m_outputEdit = nullptr;

    // Video
    QComboBox *m_videoCodecCombo = nullptr;
    QComboBox *m_videoPresetCombo = nullptr;
    QComboBox *m_videoBitrateCombo = nullptr;
    QComboBox *m_videoCrfCombo = nullptr;
    QComboBox *m_resolutionCombo = nullptr;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QComboBox *m_fpsCombo = nullptr;
    QCheckBox *m_faststartCheck = nullptr;

    // Ses
    QComboBox *m_audioCodecCombo = nullptr;
    QComboBox *m_audioBitrateCombo = nullptr;
    QComboBox *m_audioRateCombo = nullptr;
    QComboBox *m_audioChannelsCombo = nullptr;

    // Kesme
    QLineEdit *m_seekEdit = nullptr;
    QLineEdit *m_durationEdit = nullptr;

    // Ek
    QLineEdit *m_extraArgsEdit = nullptr;
    QPlainTextEdit *m_commandPreview = nullptr;

    // Çalıştırma (çıktı günlüğü ve durum yazısı yok — yalnızca ilerleme çubuğu)
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QString m_errorBuffer;

    QProcess *m_process = nullptr;
    QProcess *m_installProcess = nullptr;
    QString m_installOutput;
};
