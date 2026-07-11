#include "ui/SettingsDialog.h"

#include "config/ConfigStore.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSpinBox>

namespace wtw::ui {

namespace {

QString colorToHex(const QColor& c) {
    return QStringLiteral("#%1%2%3")
        .arg(c.red(), 2, 16, QChar('0'))
        .arg(c.green(), 2, 16, QChar('0'))
        .arg(c.blue(), 2, 16, QChar('0'));
}

QColor parseHexColor(const QString& s) {
    QString t = s.trimmed();
    if (t.startsWith(QLatin1Char('#'))) {
        t = t.mid(1);
    }
    if (t.size() != 6) {
        return QColor();
    }
    bool ok = false;
    const int rgb = t.toInt(&ok, 16);
    if (!ok) {
        return QColor();
    }
    return QColor((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
}

void updateSwatch(QLabel* swatch, const QColor& c) {
    if (!swatch) {
        return;
    }
    swatch->setStyleSheet(QStringLiteral("background:%1;border:1px solid #888;").arg(c.name()));
}

}  // namespace

SettingsDialog::SettingsDialog(const config::TreemapSettings& initial, QWidget* parent)
    : QDialog(parent), m_initial(initial), m_effective(initial) {
    setWindowTitle(QStringLiteral("Settings"));
    resize(m_initial.settingsDialogW, m_initial.settingsDialogH);
    if (m_initial.settingsDialogX || m_initial.settingsDialogY) {
        move(m_initial.settingsDialogX, m_initial.settingsDialogY);
    }

    auto* rootLayout = new QVBoxLayout(this);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* gridHost = new QWidget(scroll);
    auto* grid = new QGridLayout(gridHost);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 0);

    int row = 0;
    auto addInt = [&](const QString& name, const QString& key, int value) {
        SettingsDialog::RowEditors e;
        e.key = key;
        grid->addWidget(new QLabel(name, gridHost), row, 0);
        e.spin = new QSpinBox(gridHost);
        e.spin->setRange(1, 100000);
        e.spin->setValue(value);
        grid->addWidget(e.spin, row, 1);
        m_rows.push_back(e);
        ++row;
    };
    auto addPercent = [&](const QString& name, const QString& key, double fraction) {
        SettingsDialog::RowEditors e;
        e.key = key;
        grid->addWidget(new QLabel(name, gridHost), row, 0);
        e.dspin = new QDoubleSpinBox(gridHost);
        e.dspin->setRange(0.0001, 1.0);
        e.dspin->setDecimals(4);
        e.dspin->setSingleStep(0.001);
        e.dspin->setValue(fraction);
        grid->addWidget(e.dspin, row, 1);
        m_rows.push_back(e);
        ++row;
    };
    auto addDouble = [&](const QString& name, const QString& key, double value) {
        SettingsDialog::RowEditors e;
        e.key = key;
        grid->addWidget(new QLabel(name, gridHost), row, 0);
        e.dspin = new QDoubleSpinBox(gridHost);
        e.dspin->setRange(0.01, 100.0);
        e.dspin->setDecimals(3);
        e.dspin->setValue(value);
        grid->addWidget(e.dspin, row, 1);
        m_rows.push_back(e);
        ++row;
    };
    auto addText = [&](const QString& name, const QString& key, const QString& value) {
        SettingsDialog::RowEditors e;
        e.key = key;
        grid->addWidget(new QLabel(name, gridHost), row, 0);
        e.value = new QLineEdit(value, gridHost);
        grid->addWidget(e.value, row, 1);
        m_rows.push_back(e);
        ++row;
    };
    auto addColor = [&](const QString& name, const QString& key, const QColor& color) {
        SettingsDialog::RowEditors e;
        e.key = key;
        grid->addWidget(new QLabel(name, gridHost), row, 0);
        e.colorEdit = new QLineEdit(colorToHex(color), gridHost);
        e.swatch = new QLabel(gridHost);
        e.swatch->setFixedSize(24, 24);
        updateSwatch(e.swatch, color);
        e.pick = new QPushButton(QStringLiteral("…"), gridHost);
        auto* rowLayout = new QHBoxLayout();
        rowLayout->addWidget(e.colorEdit, 1);
        rowLayout->addWidget(e.swatch);
        rowLayout->addWidget(e.pick);
        auto* wrap = new QWidget(gridHost);
        wrap->setLayout(rowLayout);
        grid->addWidget(wrap, row, 1);
        connect(e.colorEdit, &QLineEdit::textChanged, this, [swatch = e.swatch](const QString& t) {
            updateSwatch(swatch, parseHexColor(t));
        });
        connect(e.pick, &QPushButton::clicked, this, [this, edit = e.colorEdit, swatch = e.swatch]() {
            const QColor c = QColorDialog::getColor(parseHexColor(edit->text()), this);
            if (c.isValid()) {
                edit->setText(colorToHex(c));
                updateSwatch(swatch, c);
            }
        });
        m_rows.push_back(e);
        ++row;
    };
    auto addFont = [&](const QString& name, const QString& key, const QString& value) {
        SettingsDialog::RowEditors e;
        e.key = key;
        grid->addWidget(new QLabel(name, gridHost), row, 0);
        e.combo = new QComboBox(gridHost);
        e.combo->setEditable(true);
        e.combo->addItems(QFontDatabase::families());
        e.combo->setCurrentText(value);
        grid->addWidget(e.combo, row, 1);
        m_rows.push_back(e);
        ++row;
    };

    addInt(QStringLiteral("treemap.maxTiles"), QStringLiteral("maxTiles"), initial.maxTiles);
    addPercent(QStringLiteral("treemap.clumpThreshold"), QStringLiteral("clumpThreshold"), initial.clumpThreshold);
    addInt(QStringLiteral("treemap.minTileWidth"), QStringLiteral("minTileWidthPt"), initial.minTileWidthPt);
    addInt(QStringLiteral("treemap.minTileHeight"), QStringLiteral("minTileHeightPt"), initial.minTileHeightPt);
    addInt(QStringLiteral("treemap.tilePaddingLeft"), QStringLiteral("tilePaddingLeftPt"), initial.tilePaddingLeftPt);
    addInt(QStringLiteral("treemap.tilePaddingTop"), QStringLiteral("tilePaddingTopPt"), initial.tilePaddingTopPt);
    addInt(QStringLiteral("treemap.tilePaddingRight"), QStringLiteral("tilePaddingRightPt"),
           initial.tilePaddingRightPt);
    addInt(QStringLiteral("treemap.tilePaddingBottom"), QStringLiteral("tilePaddingBottomPt"),
           initial.tilePaddingBottomPt);

    addColor(QStringLiteral("treemap.nativeFolderBgColor"), QStringLiteral("nativeFolderBg"), initial.nativeFolderBg);
    addColor(QStringLiteral("treemap.nativeFolderTextColor"), QStringLiteral("nativeFolderText"),
             initial.nativeFolderText);
    addColor(QStringLiteral("treemap.packedFolderBgColor"), QStringLiteral("packedFolderBg"), initial.packedFolderBg);
    addColor(QStringLiteral("treemap.packedFolderTextColor"), QStringLiteral("packedFolderText"),
             initial.packedFolderText);
    addColor(QStringLiteral("treemap.nativeFileBgColor"), QStringLiteral("nativeFileBg"), initial.nativeFileBg);
    addColor(QStringLiteral("treemap.nativeFileTextColor"), QStringLiteral("nativeFileText"), initial.nativeFileText);
    addColor(QStringLiteral("treemap.packedFileBgColor"), QStringLiteral("packedFileBg"), initial.packedFileBg);
    addColor(QStringLiteral("treemap.packedFileTextColor"), QStringLiteral("packedFileText"), initial.packedFileText);
    addColor(QStringLiteral("treemap.nativeClumpBgColor"), QStringLiteral("nativeClumpBg"), initial.nativeClumpBg);
    addColor(QStringLiteral("treemap.nativeClumpTextColor"), QStringLiteral("nativeClumpText"), initial.nativeClumpText);
    addColor(QStringLiteral("treemap.packedClumpBgColor"), QStringLiteral("packedClumpBg"), initial.packedClumpBg);
    addColor(QStringLiteral("treemap.packedClumpTextColor"), QStringLiteral("packedClumpText"), initial.packedClumpText);

    addFont(QStringLiteral("treemap.tileFontName"), QStringLiteral("tileFontName"), initial.tileFontName);
    addInt(QStringLiteral("treemap.headingMaxFontSize"), QStringLiteral("headingMaxFontSizePt"),
           initial.headingMaxFontSizePt);
    addInt(QStringLiteral("treemap.headingMinFontSize"), QStringLiteral("headingMinFontSizePt"),
           initial.headingMinFontSizePt);
    addDouble(QStringLiteral("treemap.headingLineHeight"), QStringLiteral("headingLineHeight"), initial.headingLineHeight);
    addDouble(QStringLiteral("treemap.detailsFontSizeRatio"), QStringLiteral("detailsFontSizeRatio"),
              initial.detailsFontSizeRatio);
    addDouble(QStringLiteral("treemap.detailsLineHeight"), QStringLiteral("detailsLineHeight"), initial.detailsLineHeight);
    addDouble(QStringLiteral("treemap.aboveDetailsHeightRatio"), QStringLiteral("aboveDetailsRatio"),
              initial.aboveDetailsRatio);
    addText(QStringLiteral("treemap.labelPlaceholder"), QStringLiteral("labelPlaceholder"), initial.labelPlaceholder);
    addText(QStringLiteral("treemap.labelDummy"), QStringLiteral("labelDummy"), initial.labelDummy);
    addText(QStringLiteral("treemap.win.exeFiles"), QStringLiteral("winExeFiles"), initial.winExeFiles);
    addText(QStringLiteral("treemap.linux.exeFiles"), QStringLiteral("linuxExeFiles"), initial.linuxExeFiles);
    addText(QStringLiteral("treemap.macos.exeFiles"), QStringLiteral("macosExeFiles"), initial.macosExeFiles);
    addInt(QStringLiteral("treemap.ui.settingsDialogX"), QStringLiteral("settingsDialogX"), initial.settingsDialogX);
    addInt(QStringLiteral("treemap.ui.settingsDialogY"), QStringLiteral("settingsDialogY"), initial.settingsDialogY);
    addInt(QStringLiteral("treemap.ui.settingsDialogW"), QStringLiteral("settingsDialogW"), initial.settingsDialogW);
    addInt(QStringLiteral("treemap.ui.settingsDialogH"), QStringLiteral("settingsDialogH"), initial.settingsDialogH);

    scroll->setWidget(gridHost);
    rootLayout->addWidget(scroll, 1);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet(QStringLiteral("color:#b00020"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    rootLayout->addWidget(m_errorLabel);

    auto* buttons = new QDialogButtonBox(this);
    auto* resetBtn = buttons->addButton(QStringLiteral("Reset Treemap Defaults"), QDialogButtonBox::ResetRole);
    buttons->addButton(QDialogButtonBox::Apply);
    buttons->addButton(QDialogButtonBox::Ok);
    buttons->addButton(QDialogButtonBox::Cancel);
    rootLayout->addWidget(buttons);

    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::onResetDefaults);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &SettingsDialog::onApply);
    connect(buttons->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &SettingsDialog::onOk);
    connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::loadIntoEditors(const config::TreemapSettings& s) {
    for (const RowEditors& e : m_rows) {
        if (e.key == QStringLiteral("maxTiles") && e.spin) {
            e.spin->setValue(s.maxTiles);
        } else if (e.key == QStringLiteral("clumpThreshold") && e.dspin) {
            e.dspin->setValue(s.clumpThreshold);
        } else if (e.key == QStringLiteral("minTileWidthPt") && e.spin) {
            e.spin->setValue(s.minTileWidthPt);
        } else if (e.key == QStringLiteral("minTileHeightPt") && e.spin) {
            e.spin->setValue(s.minTileHeightPt);
        } else if (e.key == QStringLiteral("tilePaddingLeftPt") && e.spin) {
            e.spin->setValue(s.tilePaddingLeftPt);
        } else if (e.key == QStringLiteral("tilePaddingTopPt") && e.spin) {
            e.spin->setValue(s.tilePaddingTopPt);
        } else if (e.key == QStringLiteral("tilePaddingRightPt") && e.spin) {
            e.spin->setValue(s.tilePaddingRightPt);
        } else if (e.key == QStringLiteral("tilePaddingBottomPt") && e.spin) {
            e.spin->setValue(s.tilePaddingBottomPt);
        } else if (e.key == QStringLiteral("nativeFolderBg") && e.colorEdit) {
            e.colorEdit->setText(colorToHex(s.nativeFolderBg));
        } else if (e.key == QStringLiteral("nativeFolderText") && e.colorEdit) {
            e.colorEdit->setText(colorToHex(s.nativeFolderText));
        } else if (e.key == QStringLiteral("packedFolderBg") && e.colorEdit) {
            e.colorEdit->setText(colorToHex(s.packedFolderBg));
        } else if (e.key == QStringLiteral("packedFolderText") && e.colorEdit) {
            e.colorEdit->setText(colorToHex(s.packedFolderText));
        } else if (e.key == QStringLiteral("nativeFileBg") && e.colorEdit) {
            e.colorEdit->setText(colorToHex(s.nativeFileBg));
        } else if (e.key == QStringLiteral("nativeFileText") && e.colorEdit) {
            e.colorEdit->setText(colorToHex(s.nativeFileText));
        } else if (e.key == QStringLiteral("packedFileBg") && e.colorEdit) {
            e.colorEdit->setText(colorToHex(s.packedFileBg));
        } else if (e.key == QStringLiteral("packedFileText") && e.colorEdit) {
            e.colorEdit->setText(colorToHex(s.packedFileText));
        } else if (e.key == QStringLiteral("nativeClumpBg") && e.colorEdit) {
            e.colorEdit->setText(colorToHex(s.nativeClumpBg));
        } else if (e.key == QStringLiteral("nativeClumpText") && e.colorEdit) {
            e.colorEdit->setText(colorToHex(s.nativeClumpText));
        } else if (e.key == QStringLiteral("packedClumpBg") && e.colorEdit) {
            e.colorEdit->setText(colorToHex(s.packedClumpBg));
        } else if (e.key == QStringLiteral("packedClumpText") && e.colorEdit) {
            e.colorEdit->setText(colorToHex(s.packedClumpText));
        } else if (e.key == QStringLiteral("tileFontName") && e.combo) {
            e.combo->setCurrentText(s.tileFontName);
        } else if (e.key == QStringLiteral("headingMaxFontSizePt") && e.spin) {
            e.spin->setValue(s.headingMaxFontSizePt);
        } else if (e.key == QStringLiteral("headingMinFontSizePt") && e.spin) {
            e.spin->setValue(s.headingMinFontSizePt);
        } else if (e.key == QStringLiteral("headingLineHeight") && e.dspin) {
            e.dspin->setValue(s.headingLineHeight);
        } else if (e.key == QStringLiteral("detailsFontSizeRatio") && e.dspin) {
            e.dspin->setValue(s.detailsFontSizeRatio);
        } else if (e.key == QStringLiteral("detailsLineHeight") && e.dspin) {
            e.dspin->setValue(s.detailsLineHeight);
        } else if (e.key == QStringLiteral("aboveDetailsRatio") && e.dspin) {
            e.dspin->setValue(s.aboveDetailsRatio);
        } else if (e.key == QStringLiteral("labelPlaceholder") && e.value) {
            e.value->setText(s.labelPlaceholder);
        } else if (e.key == QStringLiteral("labelDummy") && e.value) {
            e.value->setText(s.labelDummy);
        } else if (e.key == QStringLiteral("winExeFiles") && e.value) {
            e.value->setText(s.winExeFiles);
        } else if (e.key == QStringLiteral("linuxExeFiles") && e.value) {
            e.value->setText(s.linuxExeFiles);
        } else if (e.key == QStringLiteral("macosExeFiles") && e.value) {
            e.value->setText(s.macosExeFiles);
        } else if (e.key == QStringLiteral("settingsDialogX") && e.spin) {
            e.spin->setValue(s.settingsDialogX);
        } else if (e.key == QStringLiteral("settingsDialogY") && e.spin) {
            e.spin->setValue(s.settingsDialogY);
        } else if (e.key == QStringLiteral("settingsDialogW") && e.spin) {
            e.spin->setValue(s.settingsDialogW);
        } else if (e.key == QStringLiteral("settingsDialogH") && e.spin) {
            e.spin->setValue(s.settingsDialogH);
        }
    }
}

bool SettingsDialog::validateAndCollect(config::TreemapSettings* out, QString* error) {
    config::TreemapSettings s = m_initial;
    for (const RowEditors& e : m_rows) {
        if (e.key == QStringLiteral("maxTiles") && e.spin && e.spin->value() < 1) {
            *error = QStringLiteral("treemap.maxTiles must be > 0");
            return false;
        }
        if (e.key == QStringLiteral("clumpThreshold") && e.dspin && e.dspin->value() <= 0) {
            *error = QStringLiteral("treemap.clumpThreshold must be > 0");
            return false;
        }
        if (e.key == QStringLiteral("headingMinFontSizePt") && e.spin && e.key == QStringLiteral("headingMinFontSizePt")) {
            if (e.spin->value() < 1) {
                *error = QStringLiteral("treemap.headingMinFontSize must be > 0");
                return false;
            }
        }
    }

    for (const RowEditors& e : m_rows) {
        if (e.key == QStringLiteral("maxTiles") && e.spin) {
            s.maxTiles = e.spin->value();
        } else if (e.key == QStringLiteral("clumpThreshold") && e.dspin) {
            s.clumpThreshold = e.dspin->value();
        } else if (e.key == QStringLiteral("minTileWidthPt") && e.spin) {
            s.minTileWidthPt = e.spin->value();
        } else if (e.key == QStringLiteral("minTileHeightPt") && e.spin) {
            s.minTileHeightPt = e.spin->value();
        } else if (e.key == QStringLiteral("tilePaddingLeftPt") && e.spin) {
            s.tilePaddingLeftPt = e.spin->value();
        } else if (e.key == QStringLiteral("tilePaddingTopPt") && e.spin) {
            s.tilePaddingTopPt = e.spin->value();
        } else if (e.key == QStringLiteral("tilePaddingRightPt") && e.spin) {
            s.tilePaddingRightPt = e.spin->value();
        } else if (e.key == QStringLiteral("tilePaddingBottomPt") && e.spin) {
            s.tilePaddingBottomPt = e.spin->value();
        } else if (e.key.endsWith(QStringLiteral("Bg")) || e.key.endsWith(QStringLiteral("Text"))) {
            if (e.colorEdit) {
                const QColor c = parseHexColor(e.colorEdit->text());
                if (!c.isValid()) {
                    *error = QStringLiteral("Invalid color for %1").arg(e.key);
                    return false;
                }
                if (e.key == QStringLiteral("nativeFolderBg")) {
                    s.nativeFolderBg = c;
                } else if (e.key == QStringLiteral("nativeFolderText")) {
                    s.nativeFolderText = c;
                } else if (e.key == QStringLiteral("packedFolderBg")) {
                    s.packedFolderBg = c;
                } else if (e.key == QStringLiteral("packedFolderText")) {
                    s.packedFolderText = c;
                } else if (e.key == QStringLiteral("nativeFileBg")) {
                    s.nativeFileBg = c;
                } else if (e.key == QStringLiteral("nativeFileText")) {
                    s.nativeFileText = c;
                } else if (e.key == QStringLiteral("packedFileBg")) {
                    s.packedFileBg = c;
                } else if (e.key == QStringLiteral("packedFileText")) {
                    s.packedFileText = c;
                } else if (e.key == QStringLiteral("nativeClumpBg")) {
                    s.nativeClumpBg = c;
                } else if (e.key == QStringLiteral("nativeClumpText")) {
                    s.nativeClumpText = c;
                } else if (e.key == QStringLiteral("packedClumpBg")) {
                    s.packedClumpBg = c;
                } else if (e.key == QStringLiteral("packedClumpText")) {
                    s.packedClumpText = c;
                }
            }
        } else if (e.key == QStringLiteral("tileFontName") && e.combo) {
            s.tileFontName = e.combo->currentText().trimmed();
            if (s.tileFontName.isEmpty()) {
                *error = QStringLiteral("treemap.tileFontName must not be empty");
                return false;
            }
        } else if (e.key == QStringLiteral("headingMaxFontSizePt") && e.spin) {
            s.headingMaxFontSizePt = e.spin->value();
        } else if (e.key == QStringLiteral("headingMinFontSizePt") && e.spin) {
            s.headingMinFontSizePt = e.spin->value();
        } else if (e.key == QStringLiteral("headingLineHeight") && e.dspin) {
            s.headingLineHeight = e.dspin->value();
        } else if (e.key == QStringLiteral("detailsFontSizeRatio") && e.dspin) {
            s.detailsFontSizeRatio = e.dspin->value();
        } else if (e.key == QStringLiteral("detailsLineHeight") && e.dspin) {
            s.detailsLineHeight = e.dspin->value();
        } else if (e.key == QStringLiteral("aboveDetailsRatio") && e.dspin) {
            s.aboveDetailsRatio = e.dspin->value();
        } else if (e.key == QStringLiteral("labelPlaceholder") && e.value) {
            s.labelPlaceholder = e.value->text();
        } else if (e.key == QStringLiteral("labelDummy") && e.value) {
            s.labelDummy = e.value->text();
        } else if (e.key == QStringLiteral("winExeFiles") && e.value) {
            s.winExeFiles = e.value->text();
        } else if (e.key == QStringLiteral("linuxExeFiles") && e.value) {
            s.linuxExeFiles = e.value->text();
        } else if (e.key == QStringLiteral("macosExeFiles") && e.value) {
            s.macosExeFiles = e.value->text();
        } else if (e.key == QStringLiteral("settingsDialogX") && e.spin) {
            s.settingsDialogX = e.spin->value();
        } else if (e.key == QStringLiteral("settingsDialogY") && e.spin) {
            s.settingsDialogY = e.spin->value();
        } else if (e.key == QStringLiteral("settingsDialogW") && e.spin) {
            s.settingsDialogW = e.spin->value();
        } else if (e.key == QStringLiteral("settingsDialogH") && e.spin) {
            s.settingsDialogH = e.spin->value();
        }
    }

    if (s.headingMinFontSizePt > s.headingMaxFontSizePt) {
        *error = QStringLiteral("headingMinFontSize must be <= headingMaxFontSize");
        return false;
    }

    s.settingsDialogX = x();
    s.settingsDialogY = y();
    s.settingsDialogW = width();
    s.settingsDialogH = height();
    *out = s;
    return true;
}

void SettingsDialog::setError(const QString& text) {
    m_errorLabel->setText(text);
    m_errorLabel->show();
}

void SettingsDialog::clearError() {
    m_errorLabel->clear();
    m_errorLabel->hide();
}

bool SettingsDialog::isDirty() const {
    config::TreemapSettings candidate;
    QString err;
    if (!const_cast<SettingsDialog*>(this)->validateAndCollect(&candidate, &err)) {
        return true;
    }
    return candidate.maxTiles != m_initial.maxTiles || candidate.clumpThreshold != m_initial.clumpThreshold ||
           candidate.tileFontName != m_initial.tileFontName;
}

void SettingsDialog::onResetDefaults() {
    loadIntoEditors(config::defaultTreemapSettings());
    clearError();
}

void SettingsDialog::onApply() {
    config::TreemapSettings candidate;
    QString err;
    if (!validateAndCollect(&candidate, &err)) {
        setError(err);
        return;
    }
    clearError();
    if (!config::saveTreemap(candidate)) {
        setError(QStringLiteral("Could not save configuration file."));
        return;
    }
    m_effective = candidate;
    emit settingsApplied(m_effective);
}

void SettingsDialog::onOk() {
    onApply();
    if (m_errorLabel->isHidden()) {
        accept();
    }
}

void showSettingsDialog(QWidget* parent, const config::TreemapSettings& initial,
                        const std::function<void(const config::TreemapSettings&)>& onApplied) {
    SettingsDialog dlg(initial, parent);
    QObject::connect(&dlg, &SettingsDialog::settingsApplied, &dlg,
                     [onApplied](const config::TreemapSettings& s) { onApplied(s); });
    dlg.exec();
}

}  // namespace wtw::ui
