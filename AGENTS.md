# AGENTS.md — FFgui

Qt 6 Widgets + C++17 ile yazılmış FFmpeg arayüzü. Kaynak: `src/main.cpp`,
`src/MainWindow.{h,cpp}`. Arayüz dili Türkçe (`tr()` ile sarılır).

## Derleme ve doğrulama

```bash
cmake -S . -B build && cmake --build build
```

- Qt6 Widgets gerektirir. Windows'ta Qt bulunamazsa
  `-DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2022_64"` verilir.
- `build/` commitlenmez.
- Değişiklikten sonra derleme + `QT_QPA_PLATFORM=offscreen ./build/FFgui`
  (kısa çalıştırma, çökme kontrolü) yapılır.
- ffmpeg komut mantığı değiştiyse gerçek dosyayla doğrulanır
  (`/tmp/opencode/ffguitest` içinde test videoları üretilebilir).

## Katı politikalar (bozulmaz)

- **Tema yok:** `setStyleSheet()`, `setStyle()`, özel `QPalette` yasak.
  Renk/font sistem temasından gelir; simgeler `QStyle::standardIcon()`,
  dosya diyalogları native (`QFileDialog`).
- **Üzerine yazma yok:** ffmpeg her zaman `-n` ile çalışır. Çıktı == girdi
  veya çıktı zaten varsa işlem `QMessageBox` ile başlamadan engellenir.
  Önizleme amaçlı geçici dosyalar (`/tmp/FFgui_thumb.jpg`) bunun dışındadır.
- Yeni `ffmpeg` bayrağı eklenirse hem `buildArguments()` hem canlı komut
  önizlemesi (`Komut` sekmesi) aynı kaynaktan beslenmelidir.

## Kod sözleşmeleri

- Tüm UI metinleri `tr()` içinde; kısaltma/açıklama metne değil `setToolTip`
  içine yazılır.
- Yeni sekme/kontrol eklenirse `updateCommandPreview()` bağlantısı
  (`textChanged` / `currentIndexChanged` / `toggled`) eklenir.
- `startConversion()` içinde kullanıcı girdisi doğrulanır (dosya varlığı,
  altyazı kipinde dosya + `Kopyala`-ile-kalıcı-yazma çakışması gibi).
- `QProcess::splitCommand()` (Qt6) kullanılır; elle boşlukla bölme yapılmaz.
- Altyazı yolu `subtitles=` filtresine gömülürken kaçış uygulanır
  (`\`, `:[];,`, tek tırnak; Windows `\` → `/`).
- Video filtresi tektir: `scale` + `subtitles` virgülle birleştirilip tek
  `-vf` altında verilir.
- Gömme (ayrı kanal) kipinde açık `-map 0:v? -map 0:a? -map 1` kullanılır
  (`?` sonekleri sessiz girdilerde komutu korur; Ses-yok kipinde ses
  maplenmez). Altyazı codec otomatiği: mp4/mov→`mov_text`,
  webm→`webvtt`, diğer→`srt`.
- Girdi değişiminde (`onInputChanged`) yalnızca dosya varsa `ffprobe`
  çalışır; seçim yokken önizleme paneli boş bırakılır.

## Commit

- Mesaj dili Türkçe, kısa başlık + madde listesi.
- Sadece kaynak + belge eklenir (`README.md`, `src/*`); `build/` eklenmez.
