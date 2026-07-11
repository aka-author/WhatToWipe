#pragma once

#include <QColor>
#include <QString>

namespace wtw::config {

struct TreemapSettings {
    int maxTiles = 20;
    double clumpThreshold = 0.01;
    int minTileWidthPt = 50;
    int minTileHeightPt = 50;
    int tilePaddingLeftPt = 5;
    int tilePaddingTopPt = 5;
    int tilePaddingRightPt = 5;
    int tilePaddingBottomPt = 5;

    QColor nativeFolderBg{0x80, 0xef, 0x80};
    QColor nativeFolderText{0, 0, 0};
    QColor packedFolderBg{0x06, 0x40, 0x2b};
    QColor packedFolderText{0xff, 0xff, 0xff};
    QColor nativeFileBg{0xff, 0xb0, 0x9c};
    QColor nativeFileText{0, 0, 0};
    QColor packedFileBg{0x90, 0x00, 0x00};
    QColor packedFileText{0xff, 0xff, 0xff};
    QColor nativeClumpBg{0xaa, 0xaa, 0xaa};
    QColor nativeClumpText{0, 0, 0};
    QColor packedClumpBg{0x32, 0x32, 0x32};
    QColor packedClumpText{0xff, 0xff, 0xff};

    QString tileFontName = QStringLiteral("Segoe UI");
    int headingMaxFontSizePt = 30;
    int headingMinFontSizePt = 8;
    double headingLineHeight = 1.2;
    double detailsFontSizeRatio = 0.8;
    double detailsLineHeight = 1.5;
    double aboveDetailsRatio = 1.0;
    QString labelPlaceholder = QStringLiteral("...");
    QString labelDummy = QStringLiteral("...");

    QString winExeFiles = QStringLiteral("bat, com, exe, dll, scr, msi");
    QString linuxExeFiles;
    QString macosExeFiles;

    int settingsDialogX = 0;
    int settingsDialogY = 0;
    int settingsDialogW = 540;
    int settingsDialogH = 440;
};

TreemapSettings defaultTreemapSettings();

}  // namespace wtw::config
