// FFgui — Qt + C++ ile yazılmış FFmpeg arayüzü.
// Copyright (C) 2026 FFgui contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QIcon>

#include "MainWindow.h"

// Bilerek QApplication::setStyle / setPalette / setStyleSheet çağrısı YOK.
// Qt, platform eklentisi üzerinden sistem temasını kendisi seçer
// (örn. Linux'ta Breeze/Adwaita/gtk, Windows'ta windowsvista, macOS'ta macos).
// Koyu/açık kip de sistem paletinden otomatik gelir.

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FFgui"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("FFgui"));
    // Uygulama simgesi (görev çubuğu vb.): src/FFgui.png, icons.qrc ile gömülü.
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/FFgui.png")));

    MainWindow w;
    w.show();
    return app.exec();
}
