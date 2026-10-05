// FFgui — Qt + C++ ile yazılmış FFmpeg arayüzü.
// Copyright (C) 2026 FFgui contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>

// NOT: Bu uygulamada bilerek özel tema / stil kodu YOKTUR.
// setStyleSheet(), setStyle(), özel QPalette kullanılmaz.
// Böylece arayüz, sistemdeki Qt temasıyla (açık/koyu, Fusion, Breeze,
// Adwaita, Windows/macOS stili vb.) tamamen uyumlu çalışır.

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("FFgui"));
    // Pencere başlığı simgesi (görev çubuğu da bunu kullanır).
    setWindowIcon(QIcon(QStringLiteral(":/icons/FFgui.png")));
    // Responsive pencere: sabit boyut yok, küçük ekranda kaydırma + esneme var.
    setMinimumSize(480, 560);
    resize(720, 600);

    // ffmpeg / ffprobe yolunu çöz (PATH içinde varsa tam yolu kullan)
    const QString ffmpegFound = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (!ffmpegFound.isEmpty())
        m_ffmpegPath = ffmpegFound;
    const QString ffprobeFound = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if (!ffprobeFound.isEmpty())
        m_ffprobePath = ffprobeFound;

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyRead, this, &MainWindow::onProcessOutput);
    connect(m_process, &QProcess::finished, this, &MainWindow::onProcessFinished);

    m_installProcess = new QProcess(this);
    m_installProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_installProcess, &QProcess::finished, this, &MainWindow::onInstallFinished);

    // NOT: Bilerek menü çubuğu yok. Tüm işlemler ana pencereden yapılır.

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(9, 9, 9, 9);

    // Kaydırılabilir içerik: dar/alçak pencerede seçenekler kayar,
    // alt taraftaki ilerleme çubuğu + butonlar her zaman görünür.
    auto *scroll = new QScrollArea(central);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *content = new QWidget(scroll);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    scroll->setWidget(content);
    mainLayout->addWidget(scroll, 1);

    // ---- Üst sekmeler: Kurulum ile Girdi/Çıktı yan yana ----
    m_topTabs = new QTabWidget(content);
    m_topTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    contentLayout->addWidget(m_topTabs);

    // Kurulum sayfası: Windows ve Linux bölümleri tek sayfada, ayrı sekme yok.
    m_setupPage = new QWidget(m_topTabs);
    auto *setupLayout = new QVBoxLayout(m_setupPage);
    m_ffmpegStatusLabel = new QLabel(m_setupPage);
    m_ffmpegStatusLabel->setWordWrap(true);
    setupLayout->addWidget(m_ffmpegStatusLabel);

    m_winGroup = new QGroupBox(tr("Windows"), m_setupPage);
    auto *winLayout = new QVBoxLayout(m_winGroup);
    auto *winCmdRow = new QHBoxLayout;
    m_winCommandEdit = new QLineEdit(m_winGroup);
    m_winCommandEdit->setText(QStringLiteral("winget install ffmpeg"));
    m_winCommandEdit->setReadOnly(true);
    m_winCommandEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_winCommandEdit->setMinimumWidth(120);
    m_winInstallButton = new QPushButton(tr("Yükle"), m_winGroup);
    winCmdRow->addWidget(m_winCommandEdit, 1);
    winCmdRow->addWidget(m_winInstallButton);
    winLayout->addLayout(winCmdRow);
    setupLayout->addWidget(m_winGroup);

    m_linuxGroup = new QGroupBox(tr("Linux"), m_setupPage);
    auto *linuxLayout = new QVBoxLayout(m_linuxGroup);
    auto *distroRow = new QHBoxLayout;
    distroRow->addWidget(new QLabel(tr("Dağıtım:"), m_linuxGroup));
    m_distroCombo = new QComboBox(m_linuxGroup);
    m_distroCombo->addItem(tr("Debian"), QStringLiteral("debian"));
    m_distroCombo->addItem(tr("Fedora"), QStringLiteral("fedora"));
    m_distroCombo->addItem(tr("Arch Linux"), QStringLiteral("arch"));
    m_distroCombo->addItem(tr("Diğer…"), QStringLiteral("other"));
    m_distroCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_distroCombo->setToolTip(tr("Komut şablonunu belirler; Diğer seçilirse komutu kendin yaz"));
    distroRow->addWidget(m_distroCombo, 1);
    linuxLayout->addLayout(distroRow);
    auto *linuxCmdRow = new QHBoxLayout;
    m_linuxCommandEdit = new QLineEdit(m_linuxGroup);
    m_linuxCommandEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_linuxCommandEdit->setMinimumWidth(120);
    m_linuxCommandEdit->setToolTip(tr("Diğer dağıtımlar için komutu buraya kendin yaz"));
    m_linuxInstallButton = new QPushButton(tr("Yükle"), m_linuxGroup);
    linuxCmdRow->addWidget(m_linuxCommandEdit, 1);
    linuxCmdRow->addWidget(m_linuxInstallButton);
    linuxLayout->addLayout(linuxCmdRow);
    setupLayout->addWidget(m_linuxGroup);
    setupLayout->addStretch(1);

    // Kullanılan sisteme göre diğer bölüm tıklanamaz.
#ifdef Q_OS_WIN
    m_linuxGroup->setEnabled(false);
    m_linuxGroup->setToolTip(tr("Yalnızca Linux üzerinde kullanılabilir"));
#else
    m_winGroup->setEnabled(false);
    m_winGroup->setToolTip(tr("Yalnızca Windows üzerinde kullanılabilir"));
#endif
    m_topTabs->addTab(m_setupPage, tr("Kurulum"));

    connect(m_winInstallButton, &QPushButton::clicked, this, &MainWindow::installWindowsFfmpeg);
    connect(m_linuxInstallButton, &QPushButton::clicked, this, &MainWindow::installLinuxFfmpeg);
    connect(m_distroCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onDistroChanged);
    onDistroChanged(m_distroCombo->currentIndex());

    // ---- Anasayfa, Kurulum sekmesiyle yan yana ----
    m_ioPage = new QWidget(m_topTabs);
    auto *ioLayout = new QFormLayout(m_ioPage);

    auto *inputRow = new QHBoxLayout;
    m_inputEdit = new QLineEdit(m_ioPage);
    m_inputEdit->setPlaceholderText(tr("Girdi video dosyası…"));
    m_inputEdit->setClearButtonEnabled(true);
    m_inputEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_inputEdit->setMinimumWidth(120);
    auto *inputBrowse = new QPushButton(tr("Gözat…"), m_ioPage);
    inputBrowse->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    inputRow->addWidget(m_inputEdit, 1);
    inputRow->addWidget(inputBrowse);
    ioLayout->addRow(tr("Girdi:"), inputRow);

    auto *outputRow = new QHBoxLayout;
    m_outputEdit = new QLineEdit(m_ioPage);
    m_outputEdit->setPlaceholderText(tr("Çıktı dosyası…"));
    m_outputEdit->setToolTip(tr("Örn. /tmp/cikti.mp4"));
    m_outputEdit->setClearButtonEnabled(true);
    m_outputEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_outputEdit->setMinimumWidth(120);
    auto *outputBrowse = new QPushButton(tr("Farklı Kaydet…"), m_ioPage);
    outputBrowse->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    outputRow->addWidget(m_outputEdit, 1);
    outputRow->addWidget(outputBrowse);
    ioLayout->addRow(tr("Çıktı:"), outputRow);

    // NOT: Bilerek "üzerine yaz" seçeneği yok. Orijinal video dosyasını
    // korumak için ffmpeg her zaman "-n" (asla üzerine yazma) ile çalışır
    // ve çıktı = girdi ise işlem başlamadan engellenir (bkz. startConversion).

    // Anasayfa en başta, Kurulum ikinci sekme.
    m_topTabs->insertTab(0, m_ioPage, tr("Anasayfa"));

    connect(inputBrowse, &QPushButton::clicked, this, &MainWindow::browseInput);
    connect(outputBrowse, &QPushButton::clicked, this, &MainWindow::browseOutput);
    connect(m_inputEdit, &QLineEdit::textChanged, this, &MainWindow::onInputChanged);

    // ---- Seçenek sekmeleri ----
    m_optionsTabs = new QTabWidget(content);
    auto *tabs = m_optionsTabs;
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    tabs->setDocumentMode(false);
    contentLayout->addWidget(tabs, 1);

    // Video sekmesi
    auto *videoTab = new QWidget(tabs);
    auto *videoForm = new QFormLayout(videoTab);
    m_videoCodecCombo = new QComboBox(videoTab);
    m_videoCodecCombo->addItem(tr("H.264"), QStringLiteral("libx264"));
    m_videoCodecCombo->setItemData(0, QStringLiteral("libx264"), Qt::ToolTipRole);
    m_videoCodecCombo->addItem(tr("H.265 / HEVC"), QStringLiteral("libx265"));
    m_videoCodecCombo->setItemData(1, QStringLiteral("libx265"), Qt::ToolTipRole);
    m_videoCodecCombo->addItem(tr("VP9"), QStringLiteral("libvpx-vp9"));
    m_videoCodecCombo->setItemData(2, QStringLiteral("libvpx-vp9"), Qt::ToolTipRole);
    m_videoCodecCombo->addItem(tr("AV1"), QStringLiteral("libaom-av1"));
    m_videoCodecCombo->setItemData(3, QStringLiteral("libaom-av1"), Qt::ToolTipRole);
    m_videoCodecCombo->addItem(tr("MPEG-4"), QStringLiteral("mpeg4"));
    m_videoCodecCombo->setItemData(4, QStringLiteral("mpeg4"), Qt::ToolTipRole);
    m_videoCodecCombo->addItem(tr("Kopyala"), QStringLiteral("copy"));
    m_videoCodecCombo->setItemData(5, tr("Yeniden kodlama yapmadan aktar"), Qt::ToolTipRole);
    m_videoCodecCombo->setToolTip(tr("Video codec"));
    videoForm->addRow(tr("Video codec:"), m_videoCodecCombo);

    m_videoPresetCombo = new QComboBox(videoTab);
    m_videoPresetCombo->addItems({QStringLiteral("ultrafast"), QStringLiteral("superfast"),
                                  QStringLiteral("veryfast"), QStringLiteral("faster"),
                                  QStringLiteral("fast"), QStringLiteral("medium"),
                                  QStringLiteral("slow"), QStringLiteral("slower"),
                                  QStringLiteral("veryslow")});
    m_videoPresetCombo->setCurrentText(QStringLiteral("medium"));
    m_videoPresetCombo->setToolTip(tr("Yalnızca x264 ve x265 için geçerli"));
    videoForm->addRow(tr("Preset:"), m_videoPresetCombo);

    m_videoCrfCombo = new QComboBox(videoTab);
    m_videoCrfCombo->addItem(tr("Otomatik kalite"), QStringLiteral(""));
    m_videoCrfCombo->setItemData(0, tr("CRF 23"), Qt::ToolTipRole);
    for (int crf : {18, 20, 23, 26, 28, 32}) {
        m_videoCrfCombo->addItem(tr("CRF %1").arg(crf), QString::number(crf));
        if (crf == 23)
            m_videoCrfCombo->setCurrentIndex(m_videoCrfCombo->count() - 1);
    }
    videoForm->addRow(tr("Kalite:"), m_videoCrfCombo);
    m_videoCrfCombo->setToolTip(tr("CRF değeri; küçük sayı daha yüksek kalite demektir"));

    m_videoBitrateCombo = new QComboBox(videoTab);
    m_videoBitrateCombo->setEditable(true);
    m_videoBitrateCombo->addItem(tr("Otomatik"), QStringLiteral(""));
    m_videoBitrateCombo->addItem(QStringLiteral("500k"), QStringLiteral("500k"));
    m_videoBitrateCombo->addItem(QStringLiteral("1000k"), QStringLiteral("1000k"));
    m_videoBitrateCombo->addItem(QStringLiteral("2500k"), QStringLiteral("2500k"));
    m_videoBitrateCombo->addItem(QStringLiteral("5000k"), QStringLiteral("5000k"));
    m_videoBitrateCombo->addItem(QStringLiteral("8000k"), QStringLiteral("8000k"));
    m_videoBitrateCombo->setCurrentIndex(0);
    m_videoBitrateCombo->setToolTip(tr("Boş/Otomatik bırakılırsa CRF kullanılır."));
    videoForm->addRow(tr("Video bit hızı:"), m_videoBitrateCombo);

    m_resolutionCombo = new QComboBox(videoTab);
    m_resolutionCombo->addItem(tr("Kaynakla aynı"), QStringLiteral("same"));
    m_resolutionCombo->addItem(QStringLiteral("1920×1080"), QStringLiteral("1920:1080"));
    m_resolutionCombo->setItemData(1, QStringLiteral("1080p"), Qt::ToolTipRole);
    m_resolutionCombo->addItem(QStringLiteral("1280×720"), QStringLiteral("1280:720"));
    m_resolutionCombo->setItemData(2, QStringLiteral("720p"), Qt::ToolTipRole);
    m_resolutionCombo->addItem(QStringLiteral("854×480"), QStringLiteral("854:480"));
    m_resolutionCombo->setItemData(3, QStringLiteral("480p"), Qt::ToolTipRole);
    m_resolutionCombo->addItem(QStringLiteral("640×360"), QStringLiteral("640:360"));
    m_resolutionCombo->setItemData(4, QStringLiteral("360p"), Qt::ToolTipRole);
    m_resolutionCombo->addItem(tr("Özel…"), QStringLiteral("custom"));
    m_resolutionCombo->setToolTip(tr("Çıktı çözünürlüğü"));
    videoForm->addRow(tr("Çözünürlük:"), m_resolutionCombo);

    auto *whRow = new QHBoxLayout;
    m_widthSpin = new QSpinBox(videoTab);
    m_widthSpin->setRange(16, 7680);
    m_widthSpin->setValue(1280);
    m_widthSpin->setEnabled(false);
    m_heightSpin = new QSpinBox(videoTab);
    m_heightSpin->setRange(16, 4320);
    m_heightSpin->setValue(720);
    m_heightSpin->setEnabled(false);
    whRow->addWidget(new QLabel(tr("G:"), videoTab));
    whRow->addWidget(m_widthSpin);
    whRow->addWidget(new QLabel(tr("Y:"), videoTab));
    whRow->addWidget(m_heightSpin);
    whRow->addStretch(1);
    videoForm->addRow(QString(), whRow);

    m_fpsCombo = new QComboBox(videoTab);
    m_fpsCombo->addItem(tr("Aynı"), QStringLiteral(""));
    for (const char *fps : {"24", "25", "30", "50", "60"})
        m_fpsCombo->addItem(QString::fromLatin1(fps), QString::fromLatin1(fps));
    m_fpsCombo->setToolTip(tr("Saniyedeki kare sayısı"));
    videoForm->addRow(tr("Kare hızı:"), m_fpsCombo);

    m_faststartCheck = new QCheckBox(tr("Web için hızlı başlat"), videoTab);
    m_faststartCheck->setChecked(true);
    m_faststartCheck->setToolTip(tr("+faststart: moov verisini başa taşır. "
                                    "Tarayıcı oynatmaya başlamak için dosyanın tamamını indirmek zorunda kalmaz; "
                                    "özellikle web'de yayınlarken önerilir. Yalnızca MP4/MOV çıkışlarda etkilidir."));
    videoForm->addRow(QString(), m_faststartCheck);

    connect(m_resolutionCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        const bool custom = m_resolutionCombo->itemData(idx).toString() == QLatin1String("custom");
        m_widthSpin->setEnabled(custom);
        m_heightSpin->setEnabled(custom);
        updateCommandPreview();
    });

    tabs->addTab(videoTab, tr("Video"));

    // Ses sekmesi
    auto *audioTab = new QWidget(tabs);
    auto *audioForm = new QFormLayout(audioTab);
    m_audioCodecCombo = new QComboBox(audioTab);
    m_audioCodecCombo->addItem(tr("AAC"), QStringLiteral("aac"));
    m_audioCodecCombo->addItem(tr("MP3"), QStringLiteral("libmp3lame"));
    m_audioCodecCombo->setItemData(1, QStringLiteral("libmp3lame"), Qt::ToolTipRole);
    m_audioCodecCombo->addItem(tr("Opus"), QStringLiteral("libopus"));
    m_audioCodecCombo->setItemData(2, QStringLiteral("libopus"), Qt::ToolTipRole);
    m_audioCodecCombo->addItem(tr("AC-3"), QStringLiteral("ac3"));
    m_audioCodecCombo->setItemData(3, QStringLiteral("ac3"), Qt::ToolTipRole);
    m_audioCodecCombo->addItem(tr("Kopyala"), QStringLiteral("copy"));
    m_audioCodecCombo->addItem(tr("Ses yok"), QStringLiteral("none"));
    m_audioCodecCombo->setItemData(5, QStringLiteral("-an"), Qt::ToolTipRole);
    m_audioCodecCombo->setToolTip(tr("Ses codec"));
    audioForm->addRow(tr("Ses codec:"), m_audioCodecCombo);

    m_audioBitrateCombo = new QComboBox(audioTab);
    m_audioBitrateCombo->setEditable(true);
    for (const char *b : {"64k", "96k", "128k", "192k", "256k", "320k"})
        m_audioBitrateCombo->addItem(QString::fromLatin1(b), QString::fromLatin1(b));
    m_audioBitrateCombo->setCurrentText(QStringLiteral("128k"));
    audioForm->addRow(tr("Ses bit hızı:"), m_audioBitrateCombo);

    m_audioRateCombo = new QComboBox(audioTab);
    m_audioRateCombo->addItem(tr("Aynı"), QStringLiteral(""));
    m_audioRateCombo->addItem(QStringLiteral("44100 Hz"), QStringLiteral("44100"));
    m_audioRateCombo->addItem(QStringLiteral("48000 Hz"), QStringLiteral("48000"));
    audioForm->addRow(tr("Örnekleme hızı:"), m_audioRateCombo);

    m_audioChannelsCombo = new QComboBox(audioTab);
    m_audioChannelsCombo->addItem(tr("Aynı"), QStringLiteral(""));
    m_audioChannelsCombo->addItem(tr("Mono"), QStringLiteral("1"));
    m_audioChannelsCombo->setItemData(1, tr("1 kanal"), Qt::ToolTipRole);
    m_audioChannelsCombo->addItem(tr("Stereo"), QStringLiteral("2"));
    m_audioChannelsCombo->setItemData(2, tr("2 kanal"), Qt::ToolTipRole);
    audioForm->addRow(tr("Kanal:"), m_audioChannelsCombo);
    tabs->addTab(audioTab, tr("Ses"));

    // Kesme sekmesi
    auto *trimTab = new QWidget(tabs);
    auto *trimForm = new QFormLayout(trimTab);
    m_seekEdit = new QLineEdit(trimTab);
    m_seekEdit->setPlaceholderText(tr("örn. 00:00:10"));
    m_seekEdit->setToolTip(tr("Boş bırakılırsa baştan başlar; ffmpeg seçeneği: -ss"));
    trimForm->addRow(tr("Başlangıç:"), m_seekEdit);
    m_durationEdit = new QLineEdit(trimTab);
    m_durationEdit->setPlaceholderText(tr("örn. 00:00:30"));
    m_durationEdit->setToolTip(tr("Boş bırakılırsa sonuna kadar sürer; ffmpeg seçeneği: -t"));
    trimForm->addRow(tr("Süre:"), m_durationEdit);
    auto *trimHint = new QLabel(tr("Biçim: saniye ya da saat:dakika:saniye"), trimTab);
    trimHint->setToolTip(tr("Örnek: 75 ya da 00:01:15"));
    trimForm->addRow(QString(), trimHint);
    tabs->addTab(trimTab, tr("Kesme"));

    // Ek seçenekler sekmesi
    auto *extraTab = new QWidget(tabs);
    auto *extraLayout = new QVBoxLayout(extraTab);
    m_extraArgsEdit = new QLineEdit(extraTab);
    m_extraArgsEdit->setPlaceholderText(tr("Ek ffmpeg argümanları"));
    m_extraArgsEdit->setToolTip(tr("Örn. -pix_fmt yuv420p"));
    m_commandPreview = new QPlainTextEdit(extraTab);
    m_commandPreview->setReadOnly(true);
    // Sistem monospace fontunu kullan, özel renk/tema yok:
    m_commandPreview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    extraLayout->addWidget(new QLabel(tr("Ek argümanlar:"), extraTab));
    extraLayout->addWidget(m_extraArgsEdit);
    extraLayout->addWidget(new QLabel(tr("Oluşacak komut önizlemesi:"), extraTab));
    extraLayout->addWidget(m_commandPreview, 1);
    tabs->addTab(extraTab, tr("Komut"));

    // ---- İlerleme + butonlar (günlük yok, yalnızca çubuk) ----
    // Kurulum sayfasında gizlenebilmesi için ayrı bir kapta tutulur.
    m_bottomWidget = new QWidget(central);
    auto *bottomBox = new QVBoxLayout(m_bottomWidget);
    bottomBox->setContentsMargins(0, 6, 0, 0);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(6);
    m_progressBar = new QProgressBar(m_bottomWidget);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_progressBar->setMinimumWidth(140);
    m_progressBar->setMinimumHeight(16);
    bottomRow->addWidget(m_progressBar, 1);

    // Sistem temasının standart simgelerini kullan (tema uyumlu):
    m_startButton = new QPushButton(style()->standardIcon(QStyle::SP_MediaPlay), tr("Dönüştür"), m_bottomWidget);
    m_startButton->setDefault(true);
    m_startButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_stopButton = new QPushButton(style()->standardIcon(QStyle::SP_MediaStop), tr("Durdur"), m_bottomWidget);
    m_stopButton->setEnabled(false);
    m_stopButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    bottomRow->addWidget(m_startButton);
    bottomRow->addWidget(m_stopButton);
    bottomBox->addLayout(bottomRow);
    mainLayout->addWidget(m_bottomWidget, 0);

    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startConversion);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopConversion);

    // Önizlemeyi tetikleyen tüm değişiklikler:
    const auto refresh = [this] { updateCommandPreview(); };
    connect(m_outputEdit, &QLineEdit::textChanged, this, refresh);
    connect(m_videoCodecCombo, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_videoPresetCombo, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_videoCrfCombo, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_videoBitrateCombo, &QComboBox::currentTextChanged, this, refresh);
    connect(m_fpsCombo, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_faststartCheck, &QCheckBox::toggled, this, refresh);
    connect(m_audioCodecCombo, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_audioBitrateCombo, &QComboBox::currentTextChanged, this, refresh);
    connect(m_audioRateCombo, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_audioChannelsCombo, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_seekEdit, &QLineEdit::textChanged, this, refresh);
    connect(m_durationEdit, &QLineEdit::textChanged, this, refresh);
    connect(m_extraArgsEdit, &QLineEdit::textChanged, this, refresh);
    connect(m_widthSpin, &QSpinBox::valueChanged, this, refresh);
    connect(m_heightSpin, &QSpinBox::valueChanged, this, refresh);

    connect(m_topTabs, &QTabWidget::currentChanged, this, &MainWindow::onTopTabChanged);

    refreshFfmpegStatus();
    updateCommandPreview();
    // Açılışta Anasayfa seçili; Kurulum sayfasına geçince anasayfa öğeleri gizlenir.
    m_topTabs->setCurrentWidget(m_ioPage);
    onTopTabChanged(m_topTabs->currentIndex());
}

void MainWindow::browseInput()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Girdi dosyasını seç"), m_inputEdit->text());
    if (!path.isEmpty())
        m_inputEdit->setText(path);
}

void MainWindow::browseOutput()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Çıktı dosyasını seç"), m_outputEdit->text());
    if (!path.isEmpty())
        m_outputEdit->setText(path);
}

void MainWindow::onInputChanged()
{
    const QString path = m_inputEdit->text().trimmed();
    if (!path.isEmpty() && QFileInfo::exists(path))
        probeInputFile(path);
    updateCommandPreview();
}

void MainWindow::probeInputFile(const QString &path)
{
    // ffprobe yoksa sessizce geç; süre bilgisi ilerlemede ffmpeg çıktısından da gelir.
    if (QStandardPaths::findExecutable(QFileInfo(m_ffprobePath).fileName()).isEmpty()
        && !QFileInfo(m_ffprobePath).isFile()) {
        return;
    }
    QProcess probe;
    probe.start(m_ffprobePath,
                {QStringLiteral("-v"), QStringLiteral("error"),
                 QStringLiteral("-show_entries"), QStringLiteral("format=duration"),
                 QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1:nokey=1"), path});
    if (probe.waitForFinished(3000) && probe.exitCode() == 0) {
        const double secs = probe.readAllStandardOutput().trimmed().toDouble();
        if (secs > 0)
            m_totalSeconds = secs; // ilerleme yüzdesi için sessizce saklanır
    }
}

QStringList MainWindow::buildArguments() const
{
    QStringList args;
    // Güvenlik: asla üzerine yazma. Orijinal dosya korunur;
    // çıktı zaten varsa ffmpeg işlem yapmadan çıkar.
    args << QStringLiteral("-n");

    if (!m_seekEdit->text().trimmed().isEmpty())
        args << QStringLiteral("-ss") << m_seekEdit->text().trimmed();

    args << QStringLiteral("-i") << m_inputEdit->text().trimmed();

    if (!m_durationEdit->text().trimmed().isEmpty())
        args << QStringLiteral("-t") << m_durationEdit->text().trimmed();

    // Video
    const QString vcodec = m_videoCodecCombo->currentData().toString();
    if (vcodec == QLatin1String("copy")) {
        args << QStringLiteral("-c:v") << QStringLiteral("copy");
    } else {
        args << QStringLiteral("-c:v") << vcodec;
        if (vcodec == QLatin1String("libx264") || vcodec == QLatin1String("libx265"))
            args << QStringLiteral("-preset") << m_videoPresetCombo->currentText();

        const QString vbitrate = m_videoBitrateCombo->currentData().toString().isEmpty()
            ? m_videoBitrateCombo->currentText().trimmed()
            : m_videoBitrateCombo->currentData().toString();
        const bool autoRate = vbitrate.isEmpty() || vbitrate == tr("Otomatik");
        if (!autoRate && vbitrate != tr("Otomatik")) {
            args << QStringLiteral("-b:v") << vbitrate;
        } else {
            const QString crf = m_videoCrfCombo->currentData().toString();
            if (!crf.isEmpty()
                && (vcodec == QLatin1String("libx264") || vcodec == QLatin1String("libx265")
                    || vcodec == QLatin1String("libvpx-vp9") || vcodec == QLatin1String("libaom-av1")))
                args << QStringLiteral("-crf") << crf;
        }

        const QString scale = m_resolutionCombo->currentData().toString();
        if (scale == QLatin1String("custom"))
            args << QStringLiteral("-vf")
                 << QStringLiteral("scale=%1:%2").arg(m_widthSpin->value()).arg(m_heightSpin->value());
        else if (scale != QLatin1String("same") && !scale.isEmpty()) {
            QString vf = scale;
            args << QStringLiteral("-vf") << QStringLiteral("scale=%1").arg(vf);
        }

        const QString fps = m_fpsCombo->currentData().toString();
        if (!fps.isEmpty())
            args << QStringLiteral("-r") << fps;
    }

    // Ses
    const QString acodec = m_audioCodecCombo->currentData().toString();
    if (acodec == QLatin1String("none")) {
        args << QStringLiteral("-an");
    } else {
        args << QStringLiteral("-c:a") << acodec;
        if (acodec != QLatin1String("copy")) {
            if (!m_audioBitrateCombo->currentText().trimmed().isEmpty())
                args << QStringLiteral("-b:a") << m_audioBitrateCombo->currentText().trimmed();
            const QString rate = m_audioRateCombo->currentData().toString();
            if (!rate.isEmpty())
                args << QStringLiteral("-ar") << rate;
            const QString ch = m_audioChannelsCombo->currentData().toString();
            if (!ch.isEmpty())
                args << QStringLiteral("-ac") << ch;
        }
    }

    if (m_faststartCheck->isChecked())
        args << QStringLiteral("-movflags") << QStringLiteral("+faststart");

    const QString extra = m_extraArgsEdit->text().trimmed();
    if (!extra.isEmpty()) {
        // Basit boşlukla ayırma (tırnaklı argümanları da desteklemez; gelişmişi için QProcess::splitCommand kullan):
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        args << QProcess::splitCommand(extra);
#else
        args << extra.split(QLatin1Char(' '), Qt::SkipEmptyParts);
#endif
    }

    args << m_outputEdit->text().trimmed();
    return args;
}

QString MainWindow::commandPreviewText() const
{
    QStringList parts;
    // Önizlemede her zaman yalın "ffmpeg" yazılır; Windows dahil her sistemde
    // PATH üzerinden çözüldüğü için Linux'taki gibi aynen çalışır.
    // Gerçek çalıştırma yine çözümlenmiş m_ffmpegPath ile yapılır.
    parts << QStringLiteral("ffmpeg") << buildArguments();
    QString out;
    for (const QString &p : parts) {
        if (p.isEmpty())
            continue;
        out += (p.contains(QLatin1Char(' ')) ? QStringLiteral("\"%1\" ").arg(p) : p + QLatin1Char(' '));
    }
    return out.trimmed();
}

void MainWindow::updateCommandPreview()
{
    m_commandPreview->setPlainText(commandPreviewText());
}

void MainWindow::setRunning(bool running)
{
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    if (running)
        m_progressBar->setValue(0);
}

double MainWindow::durationToSeconds(const QString &d) const
{
    // "00:01:23.45" ya da "75.3" biçimlerini saniyeye çevirir.
    static const QRegularExpression hms(QStringLiteral(R"((?:(\d+):)?(\d+):(\d+(?:\.\d+)?))"));
    const auto m = hms.match(d);
    if (m.hasMatch()) {
        const double h = m.captured(1).isEmpty() ? 0 : m.captured(1).toDouble();
        const double min = m.captured(2).toDouble();
        const double sec = m.captured(3).toDouble();
        return h * 3600 + min * 60 + sec;
    }
    bool ok = false;
    const double v = d.toDouble(&ok);
    return ok ? v : 0.0;
}

void MainWindow::startConversion()
{
    const QString in = m_inputEdit->text().trimmed();
    const QString out = m_outputEdit->text().trimmed();
    if (in.isEmpty() || out.isEmpty()) {
        QMessageBox::warning(this, tr("Eksik bilgi"), tr("Lütfen girdi ve çıktı dosyalarını seçin."));
        return;
    }
    if (!QFileInfo::exists(in)) {
        QMessageBox::warning(this, tr("Girdi bulunamadı"), tr("Girdi dosyası diskte bulunamadı:\n%1").arg(in));
        return;
    }
    // Güvenlik: orijinal dosya asla ezilmesin.
    // 1) Çıktı ile girdi aynı dosya olamaz.
    const QString inCanon = QFileInfo(in).canonicalFilePath();
    const QString outCanon = QFileInfo(out).canonicalFilePath();
    const bool sameFile = (!inCanon.isEmpty() && inCanon == outCanon)
        || QFileInfo(in).absoluteFilePath() == QFileInfo(out).absoluteFilePath();
    if (sameFile) {
        QMessageBox::warning(this, tr("Güvenlik engeli"),
                             tr("Çıktı dosyası girdi dosyasıyla aynı olamaz.\n"
                                "Orijinal videoyu korumak için farklı bir dosya adı seçin."));
        return;
    }
    // 2) Var olan hiçbir dosya sessizce ezilmesin (-n ile ffmpeg de reddeder,
    //    ama kullanıcıya önceden açık mesaj veriyoruz).
    if (QFileInfo::exists(out)) {
        QMessageBox::warning(this, tr("Çıktı zaten var"),
                             tr("Bu çıktı dosyası zaten var ve üzerine yazılmayacak:\n%1\n\n"
                                "Lütfen farklı bir dosya adı seçin.").arg(out));
        return;
    }
    if (QStandardPaths::findExecutable(QFileInfo(m_ffmpegPath).fileName()).isEmpty()
        && !QFileInfo(m_ffmpegPath).isFile()) {
        QMessageBox::critical(this, tr("ffmpeg yok"),
                              tr("ffmpeg çalıştırılabilir dosyası bulunamadı.\nLütfen ffmpeg kurun ve PATH'e ekleyin."));
        return;
    }
    if (m_process->state() != QProcess::NotRunning) {
        QMessageBox::information(this, tr("Zaten çalışıyor"), tr("Devam eden bir dönüştürme var."));
        return;
    }

    m_errorBuffer.clear();
    m_progressBar->setValue(0);
    m_totalSeconds = 0.0; // Duration satırı ffmpeg stderr'den gelecek
    probeInputFile(in);   // varsa ffprobe ile süreyi önden al

    const QStringList args = buildArguments();
    m_process->setWorkingDirectory(QFileInfo(in).absolutePath());
    m_process->start(m_ffmpegPath, args);
    if (!m_process->waitForStarted(3000)) {
        QMessageBox::critical(this, tr("Başlatılamadı"),
                              tr("ffmpeg başlatılamadı:\n%1").arg(m_process->errorString()));
        return;
    }
    setRunning(true);
}

void MainWindow::stopConversion()
{
    if (m_process->state() != QProcess::NotRunning)
        m_process->kill();
}

void MainWindow::onProcessOutput()
{
    // Günlük gösterilmiyor; yalnızca ilerleme için ayrıştırılıyor.
    // Hata durumunda diyalogda göstermek için son satırlar kısaca saklanıyor.
    const QString chunk = QString::fromLocal8Bit(m_process->readAll());
    m_errorBuffer += chunk;
    if (m_errorBuffer.size() > 4000)
        m_errorBuffer = m_errorBuffer.right(4000);

    // Süreyi bir kez yakala: "Duration: 00:01:23.45"
    if (m_totalSeconds <= 0) {
        static const QRegularExpression durRe(QStringLiteral(R"(Duration:\s*(\S+))"));
        const auto m = durRe.match(chunk);
        if (m.hasMatch())
            m_totalSeconds = durationToSeconds(m.captured(1).trimmed().remove(QLatin1Char(',')));
    }

    // İlerlemeyi yakala: "time=00:00:12.34"
    static const QRegularExpression timeRe(QStringLiteral(R"(time=(\S+))"));
    auto it = timeRe.globalMatch(chunk);
    double last = -1;
    while (it.hasNext())
        last = durationToSeconds(it.next().captured(1));
    if (last >= 0 && m_totalSeconds > 0) {
        const int pct = qBound(0, static_cast<int>(last / m_totalSeconds * 100.0), 100);
        m_progressBar->setValue(pct);
    }
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    setRunning(false);
    if (status == QProcess::NormalExit && exitCode == 0) {
        m_progressBar->setValue(100);
        QMessageBox::information(this, tr("Tamamlandı"), tr("Dönüştürme başarıyla tamamlandı."));
    } else if (m_process->error() == QProcess::FailedToStart) {
        QMessageBox::critical(this, tr("ffmpeg başlatılamadı."),
                              tr("ffmpeg çalıştırılamadı. Kurulum sekmesinden yükleyin."));
    } else {
        m_progressBar->setValue(0);
        // Günlük paneli yok; hata ayıklama için son çıktıyı kısaca göster.
        const QString tail = m_errorBuffer.trimmed().right(1500);
        if (!tail.isEmpty() && exitCode != 0)
            QMessageBox::warning(this, tr("Dönüştürme başarısız"),
                                 tr("ffmpeg %1 koduyla bitti.\n\nSon çıktı:\n%2").arg(exitCode).arg(tail));
    }
}

QString MainWindow::distroDefaultCommand(const QString &distroKey) const
{
    if (distroKey == QLatin1String("debian"))
        return QStringLiteral("sudo apt update && sudo apt install -y ffmpeg");
    if (distroKey == QLatin1String("fedora"))
        return QStringLiteral("sudo dnf install -y ffmpeg");
    if (distroKey == QLatin1String("arch"))
        return QStringLiteral("sudo pacman -S --noconfirm ffmpeg");
    return QString();
}

void MainWindow::onDistroChanged(int index)
{
    const QString key = m_distroCombo->itemData(index).toString();
    const QString preset = distroDefaultCommand(key);
    if (key == QLatin1String("other")) {
        // Diğer dağıtımlar: kullanıcı komutu kendisi yazar.
        if (m_linuxCommandEdit->text().trimmed().isEmpty()
            || m_linuxCommandEdit->text().startsWith(QStringLiteral("sudo apt"))
            || m_linuxCommandEdit->text().startsWith(QStringLiteral("sudo dnf"))
            || m_linuxCommandEdit->text().startsWith(QStringLiteral("sudo pacman")))
            m_linuxCommandEdit->clear();
        m_linuxCommandEdit->setReadOnly(false);
        m_linuxCommandEdit->setPlaceholderText(tr("Kurulum komutunu buraya yaz…"));
    } else {
        m_linuxCommandEdit->setText(preset);
        m_linuxCommandEdit->setReadOnly(false); // istenirse düzenlenebilir
    }
}

void MainWindow::onTopTabChanged(int index)
{
    // Seçili olmayan üst sayfa QTabWidget tarafından zaten gizlenir.
    // Kodlayıcı seçenekleri ve alt ilerleme/buton alanı Kurulum ile
    // ilgisiz olduğundan Kurulum seçiliyken ayrıca gizlenir,
    // Anasayfa seçiliyken geri gösterilir.
    const bool onSetup = m_topTabs->widget(index) == m_setupPage;
    m_optionsTabs->setVisible(!onSetup);
    m_bottomWidget->setVisible(!onSetup);
    // QTabWidget yüksekliği en uzun sayfaya göre belirlenir; Anasayfa'da
    // Kurulum sayfasından kalan boşluk görünmesin diye yükseklik aktif
    // sayfaya uydurulur. Gizli sayfalar yükseklik hesabında yoksayılır,
    // Anasayfa bu yüksekliğe sabitlenir, Kurulum serbest bırakılır.
    for (int i = 0; i < m_topTabs->count(); ++i)
        m_topTabs->widget(i)->setSizePolicy(QSizePolicy::Expanding,
            i == index ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    if (onSetup) {
        m_topTabs->setMinimumHeight(0);
        m_topTabs->setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        m_topTabs->setFixedHeight(m_topTabs->minimumSizeHint().height());
    }
}

void MainWindow::setInstalling(bool installing)
{
    m_winInstallButton->setEnabled(!installing);
    m_linuxInstallButton->setEnabled(!installing);
    m_startButton->setEnabled(!installing && m_process->state() == QProcess::NotRunning);
    if (installing) {
        m_progressBar->setRange(0, 0); // belirsiz ilerleme
    } else {
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
    }
}

void MainWindow::installWindowsFfmpeg()
{
    if (m_installProcess->state() != QProcess::NotRunning) {
        QMessageBox::information(this, tr("Zaten çalışıyor"), tr("Devam eden bir kurulum var."));
        return;
    }
    m_installOutput.clear();
    setInstalling(true);
    // Gereksinimdeki komut aynen çalıştırılır.
    m_installProcess->start(QStringLiteral("winget"),
                            {QStringLiteral("install"), QStringLiteral("ffmpeg")});
    if (!m_installProcess->waitForStarted(5000)) {
        setInstalling(false);
        QMessageBox::critical(this, tr("Başlatılamadı"),
                              tr("winget başlatılamadı:\n%1").arg(m_installProcess->errorString()));
    }
}

void MainWindow::installLinuxFfmpeg()
{
    if (m_installProcess->state() != QProcess::NotRunning) {
        QMessageBox::information(this, tr("Zaten çalışıyor"), tr("Devam eden bir kurulum var."));
        return;
    }
    QString cmd = m_linuxCommandEdit->text().trimmed();
    if (cmd.isEmpty()) {
        QMessageBox::warning(this, tr("Eksik komut"), tr("Lütfen önce kurulum komutunu yazın ya da bir dağıtım seçin."));
        return;
    }
    // Grafiksel oturumda sudo parola soramaz; varsa pkexec ile parola penceresi açılır.
    if (!QStandardPaths::findExecutable(QStringLiteral("pkexec")).isEmpty())
        cmd.replace(QStringLiteral("sudo "), QStringLiteral("pkexec "));
    m_installOutput.clear();
    setInstalling(true);
    m_installProcess->start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), cmd});
    if (!m_installProcess->waitForStarted(5000)) {
        setInstalling(false);
        QMessageBox::critical(this, tr("Başlatılamadı"),
                              tr("Kurulum komutu başlatılamadı:\n%1").arg(m_installProcess->errorString()));
    }
}

void MainWindow::onInstallFinished(int exitCode, QProcess::ExitStatus status)
{
    setInstalling(false);
    m_installOutput = QString::fromLocal8Bit(m_installProcess->readAll());
    refreshFfmpegStatus();
    if (status == QProcess::NormalExit && exitCode == 0) {
        QMessageBox::information(this, tr("Kurulum tamamlandı"), tr("ffmpeg kurulumu başarıyla bitti."));
    } else {
        const QString tail = m_installOutput.trimmed().right(1500);
        QMessageBox::warning(this, tr("Kurulum başarısız"),
                             tail.isEmpty()
                                 ? tr("Kurulum komutu başarısız bitti.")
                                 : tr("Kurulum komutu başarısız bitti.\n\nSon çıktı:\n%1").arg(tail));
    }
}

void MainWindow::refreshFfmpegStatus()
{
    const QString found = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (!found.isEmpty())
        m_ffmpegPath = found;
    if (!QStandardPaths::findExecutable(QStringLiteral("ffprobe")).isEmpty())
        m_ffprobePath = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));

    if (!found.isEmpty()) {
        m_ffmpegStatusLabel->setText(tr("ffmpeg bulundu: %1").arg(found));
    } else {
        m_ffmpegStatusLabel->setText(tr("ffmpeg bulunamadı. Yukarıdan işletim sistemine uygun sekmeyle yükle."));
    }
}
