#include "config/ConfigStore.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

namespace wtw::config {

static QString configDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/Erase & Rewrite");
}

QString fsConfigPath() { return configDir() + QStringLiteral("/Erase & Rewrite.config.txt"); }

QString legacyConfigPath() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/WhatToWipe/WhatToWipe.config.txt");
}

static QColor parseHex(const QString& s) {
    QString t = s.trimmed();
    if (t.startsWith('#')) t = t.mid(1);
    if (t.size() != 6) return QColor();
    bool ok = false;
    const int rgb = t.toInt(&ok, 16);
    if (!ok) return QColor();
    return QColor((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
}

static QString fmtHex(const QColor& c) {
    return QStringLiteral("#%1%2%3")
        .arg(c.red(), 2, 16, QChar('0'))
        .arg(c.green(), 2, 16, QChar('0'))
        .arg(c.blue(), 2, 16, QChar('0'));
}

static int parsePt(const QString& v) {
    QString s = v.trimmed().toLower();
    s.remove(QStringLiteral("pt"));
    bool ok = false;
    const int n = s.toInt(&ok);
    return (ok && n > 0) ? n : 0;
}

static double parseRatio(const QString& v) {
    bool ok = false;
    const double f = v.trimmed().toDouble(&ok);
    return (ok && f > 0) ? f : 0;
}

static double parsePercent(const QString& v) {
    QString s = v.trimmed().toLower();
    if (s.endsWith('%')) {
        s.chop(1);
        bool ok = false;
        const double f = s.toDouble(&ok);
        return (ok && f > 0) ? f / 100.0 : 0;
    }
    bool ok = false;
    double f = s.toDouble(&ok);
    if (!ok || f <= 0) return 0;
    if (f > 1) f /= 100.0;
    return f;
}

void applyTreemapLines(TreemapSettings& d, const QString& text) {
    const auto lines = text.split('\n');
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const int eq = line.indexOf('=');
        if (eq < 0) continue;
        const QString key = line.left(eq).trimmed().toLower();
        const QString val = line.mid(eq + 1).trimmed();
        if (key == QStringLiteral("treemap.maxtiles")) { if (int n = val.toInt(); n > 0) d.maxTiles = n; }
        else if (key == QStringLiteral("treemap.clumpthreshold")) { if (double f = parsePercent(val); f > 0) d.clumpThreshold = f; }
        else if (key == QStringLiteral("treemap.mintilewidth")) { if (int n = parsePt(val); n) d.minTileWidthPt = n; }
        else if (key == QStringLiteral("treemap.mintileheight")) { if (int n = parsePt(val); n) d.minTileHeightPt = n; }
        else if (key == QStringLiteral("treemap.tilepaddingleft")) { if (int n = parsePt(val); n) d.tilePaddingLeftPt = n; }
        else if (key == QStringLiteral("treemap.tilepaddingtop")) { if (int n = parsePt(val); n) d.tilePaddingTopPt = n; }
        else if (key == QStringLiteral("treemap.tilepaddingright")) { if (int n = parsePt(val); n) d.tilePaddingRightPt = n; }
        else if (key == QStringLiteral("treemap.tilepaddingbottom")) { if (int n = parsePt(val); n) d.tilePaddingBottomPt = n; }
        else if (key == QStringLiteral("treemap.nativefolderbgcolor")) d.nativeFolderBg = parseHex(val);
        else if (key == QStringLiteral("treemap.nativefoldertextcolor")) d.nativeFolderText = parseHex(val);
        else if (key == QStringLiteral("treemap.packedfolderbgcolor")) d.packedFolderBg = parseHex(val);
        else if (key == QStringLiteral("treemap.packedfoldertextcolor")) d.packedFolderText = parseHex(val);
        else if (key == QStringLiteral("treemap.nativefilebgcolor")) d.nativeFileBg = parseHex(val);
        else if (key == QStringLiteral("treemap.nativefiletextcolor")) d.nativeFileText = parseHex(val);
        else if (key == QStringLiteral("treemap.packedfilebgcolor")) d.packedFileBg = parseHex(val);
        else if (key == QStringLiteral("treemap.packedfiletextcolor")) d.packedFileText = parseHex(val);
        else if (key == QStringLiteral("treemap.nativeclumpbgcolor")) d.nativeClumpBg = parseHex(val);
        else if (key == QStringLiteral("treemap.nativeclumptextcolor")) d.nativeClumpText = parseHex(val);
        else if (key == QStringLiteral("treemap.packedclumpbgcolor")) d.packedClumpBg = parseHex(val);
        else if (key == QStringLiteral("treemap.packedclumptextcolor")) d.packedClumpText = parseHex(val);
        else if (key == QStringLiteral("treemap.tilefontname")) d.tileFontName = val;
        else if (key == QStringLiteral("treemap.headingmaxfontsize")) { if (int n = parsePt(val); n) d.headingMaxFontSizePt = n; }
        else if (key == QStringLiteral("treemap.headingminfontsize")) { if (int n = parsePt(val); n) d.headingMinFontSizePt = n; }
        else if (key == QStringLiteral("treemap.headinglineheight")) { if (double f = parseRatio(val); f) d.headingLineHeight = f; }
        else if (key == QStringLiteral("treemap.detailsfontsizeratio")) { if (double f = parseRatio(val); f) d.detailsFontSizeRatio = f; }
        else if (key == QStringLiteral("treemap.detailslineheight")) { if (double f = parseRatio(val); f) d.detailsLineHeight = f; }
        else if (key == QStringLiteral("treemap.abovedetailsheightratio")) { if (double f = parseRatio(val); f) d.aboveDetailsRatio = f; }
        else if (key == QStringLiteral("treemap.labelplaceholder")) d.labelPlaceholder = val;
        else if (key == QStringLiteral("treemap.labeldummy")) d.labelDummy = val;
        else if (key == QStringLiteral("treemap.win.exefiles")) d.winExeFiles = val;
        else if (key == QStringLiteral("treemap.linux.exefiles")) d.linuxExeFiles = val;
        else if (key == QStringLiteral("treemap.macos.exefiles")) d.macosExeFiles = val;
        else if (key == QStringLiteral("treemap.ui.settingsdialogx")) d.settingsDialogX = val.toInt();
        else if (key == QStringLiteral("treemap.ui.settingsdialogy")) d.settingsDialogY = val.toInt();
        else if (key == QStringLiteral("treemap.ui.settingsdialogw")) { if (int n = val.toInt(); n > 0) d.settingsDialogW = n; }
        else if (key == QStringLiteral("treemap.ui.settingsdialogh")) { if (int n = val.toInt(); n > 0) d.settingsDialogH = n; }
    }
}

QString serializeTreemap(const TreemapSettings& t) {
    QString out;
    auto w = [&](const QString& k, const QString& v) { out += k + QStringLiteral(" = ") + v + QLatin1Char('\n'); };
    w(QStringLiteral("treemap.maxTiles"), QString::number(t.maxTiles));
    w(QStringLiteral("treemap.clumpThreshold"), QString::number(t.clumpThreshold * 100.0, 'g', 6) + QStringLiteral("%"));
    w(QStringLiteral("treemap.minTileWidth"), QString::number(t.minTileWidthPt) + QStringLiteral("pt"));
    w(QStringLiteral("treemap.minTileHeight"), QString::number(t.minTileHeightPt) + QStringLiteral("pt"));
    w(QStringLiteral("treemap.tilePaddingLeft"), QString::number(t.tilePaddingLeftPt) + QStringLiteral("pt"));
    w(QStringLiteral("treemap.tilePaddingTop"), QString::number(t.tilePaddingTopPt) + QStringLiteral("pt"));
    w(QStringLiteral("treemap.tilePaddingRight"), QString::number(t.tilePaddingRightPt) + QStringLiteral("pt"));
    w(QStringLiteral("treemap.tilePaddingBottom"), QString::number(t.tilePaddingBottomPt) + QStringLiteral("pt"));
    w(QStringLiteral("treemap.nativeFolderBgColor"), fmtHex(t.nativeFolderBg));
    w(QStringLiteral("treemap.nativeFolderTextColor"), fmtHex(t.nativeFolderText));
    w(QStringLiteral("treemap.packedFolderBgColor"), fmtHex(t.packedFolderBg));
    w(QStringLiteral("treemap.packedFolderTextColor"), fmtHex(t.packedFolderText));
    w(QStringLiteral("treemap.nativeFileBgColor"), fmtHex(t.nativeFileBg));
    w(QStringLiteral("treemap.nativeFileTextColor"), fmtHex(t.nativeFileText));
    w(QStringLiteral("treemap.packedFileBgColor"), fmtHex(t.packedFileBg));
    w(QStringLiteral("treemap.packedFileTextColor"), fmtHex(t.packedFileText));
    w(QStringLiteral("treemap.nativeClumpBgColor"), fmtHex(t.nativeClumpBg));
    w(QStringLiteral("treemap.nativeClumpTextColor"), fmtHex(t.nativeClumpText));
    w(QStringLiteral("treemap.packedClumpBgColor"), fmtHex(t.packedClumpBg));
    w(QStringLiteral("treemap.packedClumpTextColor"), fmtHex(t.packedClumpText));
    w(QStringLiteral("treemap.tileFontName"), t.tileFontName);
    w(QStringLiteral("treemap.headingMaxFontSize"), QString::number(t.headingMaxFontSizePt) + QStringLiteral("pt"));
    w(QStringLiteral("treemap.headingMinFontSize"), QString::number(t.headingMinFontSizePt) + QStringLiteral("pt"));
    w(QStringLiteral("treemap.headingLineHeight"), QString::number(t.headingLineHeight, 'g', 6));
    w(QStringLiteral("treemap.detailsFontSizeRatio"), QString::number(t.detailsFontSizeRatio, 'g', 6));
    w(QStringLiteral("treemap.detailsLineHeight"), QString::number(t.detailsLineHeight, 'g', 6));
    w(QStringLiteral("treemap.aboveDetailsHeightRatio"), QString::number(t.aboveDetailsRatio, 'g', 6));
    w(QStringLiteral("treemap.labelPlaceholder"), t.labelPlaceholder);
    w(QStringLiteral("treemap.labelDummy"), t.labelDummy);
    w(QStringLiteral("treemap.win.exeFiles"), t.winExeFiles);
    w(QStringLiteral("treemap.linux.exeFiles"), t.linuxExeFiles);
    w(QStringLiteral("treemap.macos.exeFiles"), t.macosExeFiles);
    w(QStringLiteral("treemap.ui.settingsDialogX"), QString::number(t.settingsDialogX));
    w(QStringLiteral("treemap.ui.settingsDialogY"), QString::number(t.settingsDialogY));
    w(QStringLiteral("treemap.ui.settingsDialogW"), QString::number(t.settingsDialogW));
    w(QStringLiteral("treemap.ui.settingsDialogH"), QString::number(t.settingsDialogH));
    out += QStringLiteral("\nscanning.updateInterval = 0.5 sec\n");
    return out;
}

bool saveTreemap(const TreemapSettings& settings) {
    const QString path = fsConfigPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray data = serializeTreemap(settings).toUtf8();
    if (file.write(data) != data.size()) return false;
    return file.commit();
}

bool importLegacyIntoFsFile() {
    const QString legacy = legacyConfigPath();
    if (!QFile::exists(legacy)) return true;
    QFile f(legacy);
    if (!f.open(QIODevice::ReadOnly)) return false;
    TreemapSettings merged = defaultTreemapSettings();
    applyTreemapLines(merged, QString::fromUtf8(f.readAll()));
    return saveTreemap(merged);
}

bool loadOrInitTreemap(TreemapSettings& out) {
    const QString path = fsConfigPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (!QFile::exists(path)) {
        out = defaultTreemapSettings();
        if (!saveTreemap(out)) return false;
        return importLegacyIntoFsFile() && loadOrInitTreemap(out);
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        out = defaultTreemapSettings();
        return false;
    }
    out = defaultTreemapSettings();
    applyTreemapLines(out, QString::fromUtf8(f.readAll()));
    return true;
}

}  // namespace wtw::config
