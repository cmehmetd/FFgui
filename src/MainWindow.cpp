// FFgui — Qt + C++ ile yazılmış FFmpeg arayüzü.
// Copyright (C) 2026 FFgui contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
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

    // Video sekmesi: solda seçenekler, sağda önizleme
    auto *videoTab = new QWidget(tabs);
    auto *videoMain = new QHBoxLayout(videoTab);
    auto *videoFormWidget = new QWidget(videoTab);
    auto *videoForm = new QFormLayout(videoFormWidget);
    videoForm->setContentsMargins(0, 0, 0, 0);
    videoFormWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_videoCodecCombo = new QComboBox(videoFormWidget);
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

    // Sağ taraf: rastgele kare önizleme + altında orijinal video bilgileri
    videoMain->addWidget(videoFormWidget, 1);
    auto *previewBox = new QVBoxLayout;
    previewBox->setContentsMargins(6, 0, 0, 0);
    m_thumbLabel = new QLabel(videoTab);
    m_thumbLabel->setFixedSize(220, 124);
    m_thumbLabel->setAlignment(Qt::AlignCenter);
    m_thumbLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_thumbLabel->setText(QString());
    m_thumbLabel->setToolTip(tr("Girdi videosundan rastgele bir kare"));
    previewBox->addWidget(m_thumbLabel, 0, Qt::AlignTop);
    m_mediaInfoLabel = new QLabel(videoTab);
    m_mediaInfoLabel->setWordWrap(true);
    m_mediaInfoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_mediaInfoLabel->setText(QString());
    m_mediaInfoLabel->setToolTip(tr("Orijinal videonun çözünürlük, süre, codec ve boyut bilgileri"));
    previewBox->addWidget(m_mediaInfoLabel, 1);
    videoMain->addLayout(previewBox, 0);

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

    // Altyazı sekmesi: harici dosyayı ayrı kanal olarak ekle ya da görüntüye kalıcı yaz
    auto *subTab = new QWidget(tabs);
    auto *subForm = new QFormLayout(subTab);
    m_subModeCombo = new QComboBox(subTab);
    m_subModeCombo->addItem(tr("Yok"), QStringLiteral("none"));
    m_subModeCombo->addItem(tr("Ayrı kanal olarak ekle"), QStringLiteral("embed"));
    m_subModeCombo->setItemData(1, tr("Harici altyazıyı çıktıya ayrı seçilebilir kanal olarak ekler (-map)"),
                                Qt::ToolTipRole);
    m_subModeCombo->addItem(tr("Görüntüye kalıcı yaz"), QStringLiteral("burn"));
    m_subModeCombo->setItemData(2, tr("Altyazıyı görüntünün üstüne kalıcı olarak işler (-vf subtitles); geri alınamaz, yeniden kodlama zorunludur"),
                                Qt::ToolTipRole);
    m_subModeCombo->setToolTip(tr("Ayrı kanal: açılıp kapatılabilen altyazı. Kalıcı yazma: görüntüye işlenir, geri alınamaz"));
    subForm->addRow(tr("Kip:"), m_subModeCombo);

    auto *subRow = new QHBoxLayout;
    m_subFileEdit = new QLineEdit(subTab);
    m_subFileEdit->setPlaceholderText(tr("Altyazı dosyası (.srt/.ass/.vtt)…"));
    m_subFileEdit->setClearButtonEnabled(true);
    m_subFileEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_subFileEdit->setMinimumWidth(120);
    m_subFileEdit->setToolTip(tr("Harici altyazı dosyası; eklemede ikinci girdi (-i), kalıcı yazmada filtre girdisi olur"));
    auto *subBrowse = new QPushButton(tr("Gözat…"), subTab);
    subRow->addWidget(m_subFileEdit, 1);
    subRow->addWidget(subBrowse);
    subForm->addRow(tr("Dosya:"), subRow);

    m_subLangEdit = new QLineEdit(subTab);
    m_subLangEdit->setPlaceholderText(tr("örn. tur"));
    m_subLangEdit->setText(QStringLiteral("tur"));
    m_subLangEdit->setToolTip(tr("Yalnızca ayrı kanal kipinde kullanılır; ISO 639 kodu (-metadata:s:s:0 language=)"));
    subForm->addRow(tr("Dil:"), m_subLangEdit);

    m_subCodecCombo = new QComboBox(subTab);
    m_subCodecCombo->addItem(tr("Otomatik"), QStringLiteral("auto"));
    m_subCodecCombo->addItem(QStringLiteral("mov_text"), QStringLiteral("mov_text"));
    m_subCodecCombo->addItem(QStringLiteral("srt"), QStringLiteral("srt"));
    m_subCodecCombo->addItem(QStringLiteral("ass"), QStringLiteral("ass"));
    m_subCodecCombo->addItem(QStringLiteral("webvtt"), QStringLiteral("webvtt"));
    m_subCodecCombo->addItem(tr("Kopyala"), QStringLiteral("copy"));
    m_subCodecCombo->setToolTip(tr("Yalnızca ayrı kanal kipinde kullanılır; Otomatik çıkış uzantısına göre seçer (mp4→mov_text, mkv→srt)"));
    subForm->addRow(tr("Altyazı codec:"), m_subCodecCombo);

    m_subDefaultCheck = new QCheckBox(tr("Varsayılan altyazı yap"), subTab);
    m_subDefaultCheck->setChecked(true);
    m_subDefaultCheck->setToolTip(tr("Yalnızca ayrı kanal kipinde kullanılır (-disposition:s:0 default)"));
    subForm->addRow(QString(), m_subDefaultCheck);

    auto *subHint = new QLabel(tr("Ayrı kanal: kapatılabilir altyazı.\nKalıcı yazma: geri alınamaz, yeniden kodlama zorunludur."), subTab);
    subHint->setWordWrap(true);
    subForm->addRow(QString(), subHint);
    tabs->addTab(subTab, tr("Altyazı"));

    connect(subBrowse, &QPushButton::clicked, this, &MainWindow::browseSubtitle);
    connect(m_subModeCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onSubModeChanged);

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

    // Bittiğinde yapılacak işlem: ffmpeg komutunu etkilemez, yalnızca
    // başarılı dönüşümden sonra çalışır (60 sn geri sayım + vazgeçme).
    auto *postRow = new QHBoxLayout;
    postRow->setSpacing(6);
    postRow->addStretch(1);
    auto *postLabel = new QLabel(tr("Bittiğinde:"), m_bottomWidget);
    m_postActionCombo = new QComboBox(m_bottomWidget);
    m_postActionCombo->addItem(tr("Hiçbir şey"), QStringLiteral("none"));
    m_postActionCombo->addItem(tr("Programı kapat"), QStringLiteral("close"));
    m_postActionCombo->addItem(tr("Uykuya al"), QStringLiteral("sleep"));
    m_postActionCombo->addItem(tr("Bilgisayarı kapat"), QStringLiteral("shutdown"));
    m_postActionCombo->setToolTip(tr("Yalnızca dönüşüm başarılı biterse çalışır; 60 saniye geri sayımla vazgeçilebilir"));
    postLabel->setToolTip(m_postActionCombo->toolTip());
    postRow->addWidget(postLabel);
    postRow->addWidget(m_postActionCombo);
    bottomBox->addLayout(postRow);
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
    connect(m_subModeCombo, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_subFileEdit, &QLineEdit::textChanged, this, refresh);
    connect(m_subLangEdit, &QLineEdit::textChanged, this, refresh);
    connect(m_subCodecCombo, &QComboBox::currentIndexChanged, this, refresh);
    connect(m_subDefaultCheck, &QCheckBox::toggled, this, refresh);
    connect(m_extraArgsEdit, &QLineEdit::textChanged, this, refresh);
    connect(m_widthSpin, &QSpinBox::valueChanged, this, refresh);
    connect(m_heightSpin, &QSpinBox::valueChanged, this, refresh);

    connect(m_topTabs, &QTabWidget::currentChanged, this, &MainWindow::onTopTabChanged);

    refreshFfmpegStatus();
    onSubModeChanged(m_subModeCombo->currentIndex());
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

void MainWindow::browseSubtitle()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Altyazı dosyasını seç"), m_subFileEdit->text(),
        tr("Altyazı (*.srt *.ass *.ssa *.vtt);;Tümü (*)"));
    if (!path.isEmpty())
        m_subFileEdit->setText(path);
}

void MainWindow::onSubModeChanged(int index)
{
    const QString mode = m_subModeCombo->itemData(index).toString();
    const bool active = mode != QLatin1String("none");
    const bool embed = mode == QLatin1String("embed");
    m_subFileEdit->setEnabled(active);
    // Kalıcı yazmada dil/codec/varsayılan anlamsız (görüntüye işlenir).
    m_subLangEdit->setEnabled(embed);
    m_subCodecCombo->setEnabled(embed);
    m_subDefaultCheck->setEnabled(embed);
    updateCommandPreview();
}

void MainWindow::onInputChanged()
{
    const QString path = m_inputEdit->text().trimmed();
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        probeInputFile(path);
    } else {
        m_totalSeconds = 0.0;
        if (m_thumbLabel) {
            m_thumbLabel->setPixmap(QPixmap());
            m_thumbLabel->setText(QString());
        }
        if (m_mediaInfoLabel)
            m_mediaInfoLabel->setText(QString());
    }
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
    refreshMediaPreview();
}

QString MainWindow::formatPreviewDuration(double secs) const
{
    if (secs < 0)
        secs = 0;
    const int total = static_cast<int>(secs);
    const int h = total / 3600;
    const int m = (total % 3600) / 60;
    const int s = total % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

void MainWindow::refreshMediaPreview()
{
    if (!m_thumbLabel || !m_mediaInfoLabel)
        return;
    const QString path = m_inputEdit->text().trimmed();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        m_thumbLabel->setPixmap(QPixmap());
        m_thumbLabel->setText(QString());
        m_mediaInfoLabel->setText(QString());
        return;
    }

    // ---- Orijinal video bilgileri (ffprobe json) ----
    QString details = QFileInfo(path).fileName();
    QProcess probe;
    probe.start(m_ffprobePath,
                {QStringLiteral("-v"), QStringLiteral("error"),
                 QStringLiteral("-select_streams"), QStringLiteral("v:0"),
                 QStringLiteral("-show_entries"), QStringLiteral("stream=width,height,codec_name,avg_frame_rate"),
                 QStringLiteral("-show_entries"), QStringLiteral("format=duration,size,bit_rate"),
                 QStringLiteral("-of"), QStringLiteral("json"), path});
    if (probe.waitForFinished(4000) && probe.exitCode() == 0) {
        const QJsonDocument doc = QJsonDocument::fromJson(probe.readAllStandardOutput());
        const QJsonObject root = doc.object();
        const QJsonObject fmt = root.value(QStringLiteral("format")).toObject();
        const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
        const QJsonObject vs = streams.isEmpty() ? QJsonObject() : streams.first().toObject();

        const int w = vs.value(QStringLiteral("width")).toInt(0);
        const int h = vs.value(QStringLiteral("height")).toInt(0);
        const QString vcodec = vs.value(QStringLiteral("codec_name")).toString();
        double fps = 0.0;
        const QString fpsStr = vs.value(QStringLiteral("avg_frame_rate")).toString();
        if (fpsStr.contains(QLatin1Char('/'))) {
            const QStringList parts = fpsStr.split(QLatin1Char('/'));
            const double num = parts.value(0).toDouble();
            const double den = parts.value(1).toDouble();
            if (den != 0)
                fps = num / den;
        }
        const double dur = fmt.value(QStringLiteral("duration")).toString().toDouble();
        if (dur > 0)
            m_totalSeconds = dur;
        const qint64 size = static_cast<qint64>(fmt.value(QStringLiteral("size")).toString().toDouble());
        const qint64 bitrate = static_cast<qint64>(fmt.value(QStringLiteral("bit_rate")).toString().toDouble());

        QStringList lines;
        lines << QFileInfo(path).fileName();
        QString line2;
        if (w > 0 && h > 0)
            line2 += QStringLiteral("%1×%2").arg(w).arg(h);
        if (dur > 0)
            line2 += (line2.isEmpty() ? QString() : QStringLiteral(" • ")) + formatPreviewDuration(dur);
        if (!vcodec.isEmpty())
            line2 += (line2.isEmpty() ? QString() : QStringLiteral(" • ")) + vcodec;
        if (fps > 0.5)
            line2 += (line2.isEmpty() ? QString() : QStringLiteral(" • "))
                + tr("%1 fps").arg(QString::number(fps, 'f', fps >= 59.9 ? 0 : 1));
        if (!line2.isEmpty())
            lines << line2;
        QString line3;
        if (size > 0)
            line3 += QLocale().formattedDataSize(size);
        if (bitrate > 0)
            line3 += (line3.isEmpty() ? QString() : QStringLiteral(" • "))
                + tr("%1 kb/s").arg(qRound(static_cast<double>(bitrate) / 1000.0));
        if (!line3.isEmpty())
            lines << line3;
        if (streams.isEmpty())
            lines << tr("Video akışı bulunamadı.");
        details = lines.join(QLatin1Char('\n'));
    } else {
        details = QStringLiteral("%1\n%2").arg(QFileInfo(path).fileName(), tr("Bilgi alınamadı."));
    }
    m_mediaInfoLabel->setText(details);

    // ---- Rastgele kare (ffmpeg tek kare) ----
    const bool ffmpegOk = !QStandardPaths::findExecutable(QFileInfo(m_ffmpegPath).fileName()).isEmpty()
        || QFileInfo(m_ffmpegPath).isFile();
    if (!ffmpegOk) {
        m_thumbLabel->setPixmap(QPixmap());
        m_thumbLabel->setText(tr("Önizleme yok"));
        return;
    }
    double t = 1.0;
    if (m_totalSeconds > 2.0)
        t = m_totalSeconds * (0.10 + 0.80 * QRandomGenerator::global()->generateDouble());
    const QString thumbPath = QDir::temp().filePath(QStringLiteral("FFgui_thumb.jpg"));
    QProcess grab;
    grab.start(m_ffmpegPath,
               {QStringLiteral("-y"), QStringLiteral("-v"), QStringLiteral("error"),
                QStringLiteral("-ss"), QString::number(t, 'f', 2),
                QStringLiteral("-i"), path,
                QStringLiteral("-vframes"), QStringLiteral("1"),
                QStringLiteral("-vf"), QStringLiteral("scale=320:-1"), thumbPath});
    if (grab.waitForFinished(6000) && grab.exitCode() == 0 && QFileInfo::exists(thumbPath)) {
        const QPixmap pm(thumbPath);
        if (!pm.isNull()) {
            m_thumbLabel->setPixmap(pm.scaled(m_thumbLabel->size(), Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation));
            return;
        }
    }
    m_thumbLabel->setPixmap(QPixmap());
    m_thumbLabel->setText(tr("Önizleme alınamadı"));
}

QStringList MainWindow::buildArguments() const
{
    QStringList args;
    // Güvenlik: asla üzerine yazma. Orijinal dosya korunur;
    // çıktı zaten varsa ffmpeg işlem yapmadan çıkar.
    args << QStringLiteral("-n");

    const QString subMode = m_subModeCombo ? m_subModeCombo->currentData().toString() : QStringLiteral("none");
    const QString subFile = m_subFileEdit ? m_subFileEdit->text().trimmed() : QString();
    const bool useEmbed = subMode == QLatin1String("embed") && !subFile.isEmpty();
    const bool useBurn = subMode == QLatin1String("burn") && !subFile.isEmpty();

    if (!m_seekEdit->text().trimmed().isEmpty())
        args << QStringLiteral("-ss") << m_seekEdit->text().trimmed();

    args << QStringLiteral("-i") << m_inputEdit->text().trimmed();
    if (useEmbed)
        args << QStringLiteral("-i") << subFile;

    if (!m_durationEdit->text().trimmed().isEmpty())
        args << QStringLiteral("-t") << m_durationEdit->text().trimmed();

    // Ayrı kanal kipinde açık eşleme gerekir; yoksa ffmpeg varsayılan eşlemeyle
    // altyazıyı düşürebilir ya da yanlış sesi seçebilir.
    // "?": girdide ses yoksa bile komutun bozulmaması için.
    if (useEmbed) {
        args << QStringLiteral("-map") << QStringLiteral("0:v?");
        const QString acodecEarly = m_audioCodecCombo->currentData().toString();
        if (acodecEarly != QLatin1String("none"))
            args << QStringLiteral("-map") << QStringLiteral("0:a?");
        args << QStringLiteral("-map") << QStringLiteral("1");
    }

    // Video filtreleri tek "-vf" altında birleşir (scale + kalıcı yazma).
    QStringList videoFilters;
    const QString scaleKey = m_resolutionCombo->currentData().toString();
    if (scaleKey == QLatin1String("custom"))
        videoFilters << QStringLiteral("scale=%1:%2").arg(m_widthSpin->value()).arg(m_heightSpin->value());
    else if (scaleKey != QLatin1String("same") && !scaleKey.isEmpty())
        videoFilters << QStringLiteral("scale=%1").arg(scaleKey);

    if (useBurn) {
        // subtitles filtresi: yolu tek tırnak içine al, özel karakterleri kaçır.
        // Windows "C:\..." için ters bölüleri bölüye çevir (ffmpeg kabul eder).
        QString esc = subFile;
        esc.replace(QLatin1Char('\\'), QLatin1String("/"));
        esc.replace(QLatin1String("'"), QLatin1String("'\\''"));
        esc.replace(QLatin1Char(':'), QLatin1String("\\:"));
        esc.replace(QLatin1Char('['), QLatin1String("\\["));
        esc.replace(QLatin1Char(']'), QLatin1String("\\]"));
        esc.replace(QLatin1Char(','), QLatin1String("\\,"));
        esc.replace(QLatin1Char(';'), QLatin1String("\\;"));
        videoFilters << QStringLiteral("subtitles='%1'").arg(esc);
    }

    // Video
    const QString vcodec = m_videoCodecCombo->currentData().toString();
    if (vcodec == QLatin1String("copy")) {
        args << QStringLiteral("-c:v") << QStringLiteral("copy");
        // Kopyalamada scale geçersizdir ama kalıcı yazma filtresi önizlemede
        // bilerek gösterilir; startConversion zaten engeller.
        if (useBurn && !videoFilters.isEmpty())
            args << QStringLiteral("-vf") << videoFilters.join(QLatin1Char(','));
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

        if (!videoFilters.isEmpty())
            args << QStringLiteral("-vf") << videoFilters.join(QLatin1Char(','));

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

    // Altyazı codec / metadata (yalnızca ayrı kanal kipinde)
    if (useEmbed) {
        QString scodec = m_subCodecCombo->currentData().toString();
        if (scodec == QLatin1String("auto") || scodec.isEmpty()) {
            const QString outLower = m_outputEdit->text().trimmed().toLower();
            if (outLower.endsWith(QLatin1String(".mp4")) || outLower.endsWith(QLatin1String(".m4v"))
                || outLower.endsWith(QLatin1String(".mov")) || outLower.endsWith(QLatin1String(".3gp"))
                || outLower.endsWith(QLatin1String(".3g2")))
                scodec = QStringLiteral("mov_text");
            else if (outLower.endsWith(QLatin1String(".webm")))
                scodec = QStringLiteral("webvtt");
            else
                scodec = QStringLiteral("srt");
        }
        args << QStringLiteral("-c:s") << scodec;
        const QString lang = m_subLangEdit->text().trimmed().toLower();
        if (!lang.isEmpty())
            args << QStringLiteral("-metadata:s:s:0") << QStringLiteral("language=%1").arg(lang);
        args << QStringLiteral("-disposition:s:0")
             << (m_subDefaultCheck->isChecked() ? QStringLiteral("default") : QStringLiteral("0"));
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
    // Altyazı doğrulama
    const QString subMode = m_subModeCombo->currentData().toString();
    const QString subFile = m_subFileEdit->text().trimmed();
    if (subMode != QLatin1String("none")) {
        if (subFile.isEmpty()) {
            QMessageBox::warning(this, tr("Altyazı yok"),
                                 tr("Altyazı kipi seçili ama dosya boş.\nLütfen altyazı dosyası seçin ya da kipi Yok yapın."));
            return;
        }
        if (!QFileInfo::exists(subFile)) {
            QMessageBox::warning(this, tr("Altyazı bulunamadı"),
                                 tr("Altyazı dosyası diskte bulunamadı:\n%1").arg(subFile));
            return;
        }
        if (QFileInfo(subFile).absoluteFilePath() == QFileInfo(out).absoluteFilePath()) {
            QMessageBox::warning(this, tr("Güvenlik engeli"),
                                 tr("Altyazı dosyası çıktı dosyasıyla aynı olamaz."));
            return;
        }
        if (subMode == QLatin1String("burn")
            && m_videoCodecCombo->currentData().toString() == QLatin1String("copy")) {
            QMessageBox::warning(this, tr("Kalıcı yazma için yeniden kodlama zorunludur"),
                                 tr("Altyazıyı görüntüye kalıcı yazma görüntüyü değiştirir, yeniden kodlama zorunludur.\n"
                                    "Lütfen Video codec olarak Kopyala yerine H.264/H.265 seçin."));
            return;
        }
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
        // Başarılı bitince kullanıcı seçtiyse geri sayımlı işlem çalışır,
        // seçilmediyse eskisi gibi bilgi penceresi gösterilir.
        maybeRunPostAction();
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

QString MainWindow::postActionText(const QString &action) const
{
    if (action == QLatin1String("close"))
        return tr("program kapatılacak");
    if (action == QLatin1String("sleep"))
        return tr("bilgisayar uykuya alınacak");
    if (action == QLatin1String("shutdown"))
        return tr("bilgisayar kapatılacak");
    return QString();
}

void MainWindow::maybeRunPostAction()
{
    const QString action = m_postActionCombo ? m_postActionCombo->currentData().toString()
                                             : QStringLiteral("none");
    if (action.isEmpty() || action == QLatin1String("none")) {
        QMessageBox::information(this, tr("Tamamlandı"), tr("Dönüştürme başarıyla tamamlandı."));
        return;
    }
    // Önceki yarım kalmış sayaç varsa temizle.
    cancelPostAction();
    m_pendingPostAction = action;
    m_postCountdown = 60;

    m_postDialog = new QDialog(this);
    m_postDialog->setWindowTitle(tr("Dönüştürme bitti"));
    m_postDialog->setAttribute(Qt::WA_DeleteOnClose, false);
    auto *layout = new QVBoxLayout(m_postDialog);
    m_postCountdownLabel = new QLabel(m_postDialog);
    m_postCountdownLabel->setWordWrap(true);
    layout->addWidget(m_postCountdownLabel);

    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    auto *nowButton = new QPushButton(tr("Hemen yap"), m_postDialog);
    nowButton->setToolTip(tr("Geri sayımı beklemeden işlemi şimdi çalıştır"));
    auto *cancelButton = new QPushButton(tr("Vazgeç"), m_postDialog);
    cancelButton->setToolTip(tr("İşlemi iptal et, hiçbir şey yapma"));
    btnRow->addWidget(nowButton);
    btnRow->addWidget(cancelButton);
    layout->addLayout(btnRow);

    connect(nowButton, &QPushButton::clicked, this, &MainWindow::executePostActionNow);
    connect(cancelButton, &QPushButton::clicked, this, &MainWindow::cancelPostAction);

    m_postTimer = new QTimer(m_postDialog);
    m_postTimer->setInterval(1000);
    connect(m_postTimer, &QTimer::timeout, this, &MainWindow::onPostCountdownTick);
    onPostCountdownTick(); // ilk yazıyı hemen yaz
    m_postTimer->start();
    m_postDialog->setModal(true);
    m_postDialog->show();
}

void MainWindow::onPostCountdownTick()
{
    if (!m_postDialog || !m_postCountdownLabel) {
        cancelPostAction();
        return;
    }
    if (m_postCountdown <= 0) {
        const QString action = m_pendingPostAction;
        cancelPostAction();
        runPostAction(action);
        return;
    }
    m_postCountdownLabel->setText(
        tr("Dönüştürme bitti.\n%1 saniye sonra %2.")
            .arg(m_postCountdown)
            .arg(postActionText(m_pendingPostAction)));
    --m_postCountdown;
}

void MainWindow::cancelPostAction()
{
    // NOT: zamanlayıcı sinyalinin içinden de çağrılabilir; doğrudan
    // delete güvensiz olur, o yüzden diyalog deleteLater ile silinir.
    if (m_postTimer)
        m_postTimer->stop();
    m_postTimer = nullptr; // sahibi diyalog, onunla silinir
    QDialog *dlg = m_postDialog;
    m_postDialog = nullptr;
    m_postCountdownLabel = nullptr;
    m_pendingPostAction.clear();
    m_postCountdown = 0;
    if (dlg) {
        dlg->close();
        dlg->deleteLater();
    }
}

void MainWindow::executePostActionNow()
{
    const QString action = m_pendingPostAction;
    cancelPostAction();
    if (!action.isEmpty() && action != QLatin1String("none"))
        runPostAction(action);
}

void MainWindow::runPostAction(const QString &action)
{
    if (action == QLatin1String("close")) {
        close();
        return;
    }
#ifdef Q_OS_WIN
    if (action == QLatin1String("sleep")) {
        if (!QProcess::startDetached(QStringLiteral("rundll32.exe"),
                                     {QStringLiteral("powrprof.dll,SetSuspendState"),
                                      QStringLiteral("0,1,0")}))
            QMessageBox::warning(this, tr("Uyku başarısız"),
                                 tr("Bilgisayar uykuya alınamadı."));
        return;
    }
    if (action == QLatin1String("shutdown")) {
        if (!QProcess::startDetached(QStringLiteral("shutdown"),
                                     {QStringLiteral("/s"), QStringLiteral("/t"),
                                      QStringLiteral("0")}))
            QMessageBox::warning(this, tr("Kapatma başarısız"),
                                 tr("Bilgisayar kapatılamadı."));
        return;
    }
#else
    if (action == QLatin1String("sleep")) {
        if (!QStandardPaths::findExecutable(QStringLiteral("systemctl")).isEmpty()) {
            if (!QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("suspend")}))
                QMessageBox::warning(this, tr("Uyku başarısız"),
                                     tr("Bilgisayar uykuya alınamadı."));
            return;
        }
        QMessageBox::warning(this, tr("Uyku desteklenmiyor"),
                             tr("Uykuya alma için systemctl bulunamadı."));
        return;
    }
    if (action == QLatin1String("shutdown")) {
        if (!QStandardPaths::findExecutable(QStringLiteral("systemctl")).isEmpty()) {
            if (!QProcess::startDetached(QStringLiteral("systemctl"), {QStringLiteral("poweroff")}))
                QMessageBox::warning(this, tr("Kapatma başarısız"),
                                     tr("Bilgisayar kapatılamadı."));
            return;
        }
        QMessageBox::warning(this, tr("Kapatma desteklenmiyor"),
                             tr("Kapatma için systemctl bulunamadı."));
        return;
    }
#endif
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
