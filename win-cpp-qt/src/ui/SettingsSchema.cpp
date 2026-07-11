#include "ui/SettingsSchema.h"

#include <QFontDatabase>
#include <QRegularExpression>
#include <cmath>

namespace wtw::ui {

namespace {

QStringList loadFontOptions() {
    QStringList families = QFontDatabase::families();
    families.sort(Qt::CaseInsensitive);
    if (families.isEmpty()) {
        return {QStringLiteral("Arial"), QStringLiteral("Calibri"), QStringLiteral("Consolas"),
                QStringLiteral("Courier New"), QStringLiteral("Georgia"), QStringLiteral("Segoe UI"),
                QStringLiteral("Tahoma"), QStringLiteral("Times New Roman"), QStringLiteral("Verdana")};
    }
    return families;
}

bool isPointUnitKey(const QString& key) {
    const QString k = key.toLower();
    return k == QStringLiteral("treemap.mintilewidth") || k == QStringLiteral("treemap.mintileheight") ||
           k == QStringLiteral("treemap.tilepaddingleft") || k == QStringLiteral("treemap.tilepaddingtop") ||
           k == QStringLiteral("treemap.tilepaddingright") || k == QStringLiteral("treemap.tilepaddingbottom") ||
           k == QStringLiteral("treemap.headingmaxfontsize") || k == QStringLiteral("treemap.headingminfontsize");
}

QString valueForKey(const config::TreemapSettings& t, const QString& key) {
    const QString k = key.toLower();
    if (k == QStringLiteral("treemap.maxtiles")) {
        return QString::number(t.maxTiles);
    }
    if (k == QStringLiteral("treemap.clumpthreshold")) {
        return QString::number(t.clumpThreshold, 'g', 12);
    }
    if (k == QStringLiteral("treemap.mintilewidth")) {
        return formatPointsValue(t.minTileWidthPt);
    }
    if (k == QStringLiteral("treemap.mintileheight")) {
        return formatPointsValue(t.minTileHeightPt);
    }
    if (k == QStringLiteral("treemap.tilepaddingleft")) {
        return formatPointsValue(t.tilePaddingLeftPt);
    }
    if (k == QStringLiteral("treemap.tilepaddingtop")) {
        return formatPointsValue(t.tilePaddingTopPt);
    }
    if (k == QStringLiteral("treemap.tilepaddingright")) {
        return formatPointsValue(t.tilePaddingRightPt);
    }
    if (k == QStringLiteral("treemap.tilepaddingbottom")) {
        return formatPointsValue(t.tilePaddingBottomPt);
    }
    if (k == QStringLiteral("treemap.tilefontname")) {
        return t.tileFontName.trimmed();
    }
    if (k == QStringLiteral("treemap.headingmaxfontsize")) {
        return formatPointsValue(t.headingMaxFontSizePt);
    }
    if (k == QStringLiteral("treemap.headingminfontsize")) {
        return formatPointsValue(t.headingMinFontSizePt);
    }
    if (k == QStringLiteral("treemap.headinglineheight")) {
        return QString::number(t.headingLineHeight, 'g', 12);
    }
    if (k == QStringLiteral("treemap.detailsfontsizeratio")) {
        return QString::number(t.detailsFontSizeRatio, 'g', 12);
    }
    if (k == QStringLiteral("treemap.detailslineheight")) {
        return QString::number(t.detailsLineHeight, 'g', 12);
    }
    if (k == QStringLiteral("treemap.abovedetailsheightratio")) {
        return QString::number(t.aboveDetailsRatio, 'g', 12);
    }
    if (k == QStringLiteral("treemap.labelplaceholder")) {
        return t.labelPlaceholder;
    }
    if (k == QStringLiteral("treemap.labeldummy")) {
        return t.labelDummy;
    }
    if (k == QStringLiteral("treemap.nativefolderbgcolor")) {
        return formatRgbHex(t.nativeFolderBg);
    }
    if (k == QStringLiteral("treemap.nativefoldertextcolor")) {
        return formatRgbHex(t.nativeFolderText);
    }
    if (k == QStringLiteral("treemap.packedfolderbgcolor")) {
        return formatRgbHex(t.packedFolderBg);
    }
    if (k == QStringLiteral("treemap.packedfoldertextcolor")) {
        return formatRgbHex(t.packedFolderText);
    }
    if (k == QStringLiteral("treemap.nativefilebgcolor")) {
        return formatRgbHex(t.nativeFileBg);
    }
    if (k == QStringLiteral("treemap.nativefiletextcolor")) {
        return formatRgbHex(t.nativeFileText);
    }
    if (k == QStringLiteral("treemap.packedfilebgcolor")) {
        return formatRgbHex(t.packedFileBg);
    }
    if (k == QStringLiteral("treemap.packedfiletextcolor")) {
        return formatRgbHex(t.packedFileText);
    }
    if (k == QStringLiteral("treemap.nativeclumpbgcolor")) {
        return formatRgbHex(t.nativeClumpBg);
    }
    if (k == QStringLiteral("treemap.nativeclumptextcolor")) {
        return formatRgbHex(t.nativeClumpText);
    }
    if (k == QStringLiteral("treemap.packedclumpbgcolor")) {
        return formatRgbHex(t.packedClumpBg);
    }
    if (k == QStringLiteral("treemap.packedclumptextcolor")) {
        return formatRgbHex(t.packedClumpText);
    }
    if (k == QStringLiteral("treemap.win.exefiles")) {
        return t.winExeFiles.trimmed();
    }
    if (k == QStringLiteral("treemap.linux.exefiles")) {
        return t.linuxExeFiles.trimmed();
    }
    if (k == QStringLiteral("treemap.macos.exefiles")) {
        return t.macosExeFiles.trimmed();
    }
    return QString();
}

bool setValueForKey(config::TreemapSettings* t, const QString& key, const QString& raw, QString* error) {
    const QString k = key.toLower();
    const QString s = raw.trimmed();

    if (k == QStringLiteral("treemap.maxtiles")) {
        bool ok = false;
        const int v = s.toInt(&ok);
        if (!ok || v < 1) {
            *error = QStringLiteral("must be an integer >= 1");
            return false;
        }
        t->maxTiles = v;
        return true;
    }
    if (k == QStringLiteral("treemap.clumpthreshold")) {
        bool ok = false;
        const double v = s.toDouble(&ok);
        if (!ok || v <= 0 || v > 1) {
            *error = QStringLiteral("must be a number > 0 and <= 1");
            return false;
        }
        t->clumpThreshold = v;
        return true;
    }
    if (k == QStringLiteral("treemap.mintilewidth") || k == QStringLiteral("treemap.mintileheight") ||
        k == QStringLiteral("treemap.headingmaxfontsize") || k == QStringLiteral("treemap.headingminfontsize")) {
        int pt = 0;
        if (!parsePointsInputToPt(s, false, &pt) || pt < 1) {
            *error = QStringLiteral("must be a size like 20pt or 4mm, >= 1pt");
            return false;
        }
        if (k == QStringLiteral("treemap.mintilewidth")) {
            t->minTileWidthPt = pt;
        } else if (k == QStringLiteral("treemap.mintileheight")) {
            t->minTileHeightPt = pt;
        } else if (k == QStringLiteral("treemap.headingmaxfontsize")) {
            t->headingMaxFontSizePt = pt;
        } else {
            t->headingMinFontSizePt = pt;
        }
        return true;
    }
    if (k == QStringLiteral("treemap.tilepaddingleft") || k == QStringLiteral("treemap.tilepaddingtop") ||
        k == QStringLiteral("treemap.tilepaddingright") || k == QStringLiteral("treemap.tilepaddingbottom")) {
        int pt = 0;
        if (!parsePointsInputToPt(s, true, &pt) || pt < 0) {
            *error = QStringLiteral("must be a size like 20pt or 4mm, >= 0pt");
            return false;
        }
        if (k == QStringLiteral("treemap.tilepaddingleft")) {
            t->tilePaddingLeftPt = pt;
        } else if (k == QStringLiteral("treemap.tilepaddingtop")) {
            t->tilePaddingTopPt = pt;
        } else if (k == QStringLiteral("treemap.tilepaddingright")) {
            t->tilePaddingRightPt = pt;
        } else {
            t->tilePaddingBottomPt = pt;
        }
        return true;
    }
    if (k == QStringLiteral("treemap.tilefontname")) {
        if (s.isEmpty()) {
            *error = QStringLiteral("font name must not be empty");
            return false;
        }
        t->tileFontName = s;
        return true;
    }
    if (k == QStringLiteral("treemap.headinglineheight") || k == QStringLiteral("treemap.detailsfontsizeratio") ||
        k == QStringLiteral("treemap.detailslineheight") || k == QStringLiteral("treemap.abovedetailsheightratio")) {
        bool ok = false;
        const double v = s.toDouble(&ok);
        if (!ok || v <= 0) {
            *error = QStringLiteral("must be > 0");
            return false;
        }
        if (k == QStringLiteral("treemap.headinglineheight")) {
            t->headingLineHeight = v;
        } else if (k == QStringLiteral("treemap.detailsfontsizeratio")) {
            t->detailsFontSizeRatio = v;
        } else if (k == QStringLiteral("treemap.detailslineheight")) {
            t->detailsLineHeight = v;
        } else {
            t->aboveDetailsRatio = v;
        }
        return true;
    }
    if (k == QStringLiteral("treemap.labelplaceholder") || k == QStringLiteral("treemap.labeldummy")) {
        if (s.isEmpty()) {
            *error = QStringLiteral("must not be empty");
            return false;
        }
        if (k == QStringLiteral("treemap.labelplaceholder")) {
            t->labelPlaceholder = s;
        } else {
            t->labelDummy = s;
        }
        return true;
    }
    if (k.endsWith(QStringLiteral("color"))) {
        QColor c;
        if (!parseHexColor(s, &c)) {
            *error = QStringLiteral("must be in #RRGGBB format");
            return false;
        }
        if (k == QStringLiteral("treemap.nativefolderbgcolor")) {
            t->nativeFolderBg = c;
        } else if (k == QStringLiteral("treemap.nativefoldertextcolor")) {
            t->nativeFolderText = c;
        } else if (k == QStringLiteral("treemap.packedfolderbgcolor")) {
            t->packedFolderBg = c;
        } else if (k == QStringLiteral("treemap.packedfoldertextcolor")) {
            t->packedFolderText = c;
        } else if (k == QStringLiteral("treemap.nativefilebgcolor")) {
            t->nativeFileBg = c;
        } else if (k == QStringLiteral("treemap.nativefiletextcolor")) {
            t->nativeFileText = c;
        } else if (k == QStringLiteral("treemap.packedfilebgcolor")) {
            t->packedFileBg = c;
        } else if (k == QStringLiteral("treemap.packedfiletextcolor")) {
            t->packedFileText = c;
        } else if (k == QStringLiteral("treemap.nativeclumpbgcolor")) {
            t->nativeClumpBg = c;
        } else if (k == QStringLiteral("treemap.nativeclumptextcolor")) {
            t->nativeClumpText = c;
        } else if (k == QStringLiteral("treemap.packedclumpbgcolor")) {
            t->packedClumpBg = c;
        } else {
            t->packedClumpText = c;
        }
        return true;
    }
    if (k == QStringLiteral("treemap.win.exefiles")) {
        t->winExeFiles = s;
        return true;
    }
    if (k == QStringLiteral("treemap.linux.exefiles")) {
        t->linuxExeFiles = s;
        return true;
    }
    if (k == QStringLiteral("treemap.macos.exefiles")) {
        t->macosExeFiles = s;
        return true;
    }
    *error = QStringLiteral("unknown key");
    return false;
}

}  // namespace

QString formatPointsValue(int pt) { return QString::number(pt) + QStringLiteral("pt"); }

bool parsePointsInputToPt(const QString& text, bool allowZero, int* ptOut) {
    if (!ptOut) {
        return false;
    }
    QString raw = text.trimmed().toLower();
    if (raw.isEmpty()) {
        return false;
    }
    QString unit = QStringLiteral("pt");
    if (raw.endsWith(QStringLiteral("mm"))) {
        unit = QStringLiteral("mm");
        raw = raw.left(raw.size() - 2).trimmed();
    } else if (raw.endsWith(QStringLiteral("pt"))) {
        raw = raw.left(raw.size() - 2).trimmed();
    }
    bool ok = false;
    const double v = raw.toDouble(&ok);
    if (!ok) {
        return false;
    }
    if (!allowZero && v <= 0) {
        return false;
    }
    if (allowZero && v < 0) {
        return false;
    }
    double pt = v;
    if (unit == QStringLiteral("mm")) {
        pt = v * 72.0 / 25.4;
    }
    *ptOut = static_cast<int>(std::lround(pt));
    return true;
}

QString formatRgbHex(const QColor& c) {
    return QStringLiteral("#%1%2%3")
        .arg(c.red(), 2, 16, QChar('0'))
        .arg(c.green(), 2, 16, QChar('0'))
        .arg(c.blue(), 2, 16, QChar('0'));
}

bool parseHexColor(const QString& text, QColor* out) {
    if (!out) {
        return false;
    }
    QString s = text.trimmed();
    if (s.startsWith(QLatin1Char('#'))) {
        s = s.mid(1);
    }
    if (s.size() != 6) {
        return false;
    }
    bool ok = false;
    const int rgb = s.toInt(&ok, 16);
    if (!ok) {
        return false;
    }
    *out = QColor((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    return out->isValid();
}

const QVector<SettingsRowSchema>& treemapRowSchemas() {
    static const QVector<SettingsRowSchema> schemas = [] {
        const QStringList fonts = loadFontOptions();
        QVector<SettingsRowSchema> rows;
        auto addNumeric = [&](const QString& key, const QString& label, double minV, double maxV, int decimals,
                              bool points = false, bool allowZero = false) {
            SettingsRowSchema s;
            s.key = key;
            s.label = label;
            s.kind = SettingsEditorKind::Numeric;
            s.minValue = minV;
            s.maxValue = maxV;
            s.decimals = decimals;
            s.allowPointUnits = points;
            s.allowZeroPoint = allowZero;
            rows.push_back(s);
        };
        auto addText = [&](const QString& key, const QString& label, bool required = false) {
            SettingsRowSchema s;
            s.key = key;
            s.label = label;
            s.kind = SettingsEditorKind::Text;
            s.requireNonEmpty = required;
            rows.push_back(s);
        };
        auto addColor = [&](const QString& key, const QString& label) {
            SettingsRowSchema s;
            s.key = key;
            s.label = label;
            s.kind = SettingsEditorKind::Color;
            rows.push_back(s);
        };

        addNumeric(QStringLiteral("treemap.maxTiles"), QStringLiteral("Maximum number of tiles"), 1, 9999, 0);
        addNumeric(QStringLiteral("treemap.clumpThreshold"), QStringLiteral("Clump threshold ratio"), 0.000001, 1, 6);
        addNumeric(QStringLiteral("treemap.minTileWidth"), QStringLiteral("Minimum tile width"), 1, 4096, 0, true);
        addNumeric(QStringLiteral("treemap.minTileHeight"), QStringLiteral("Minimum tile height"), 1, 4096, 0, true);
        addNumeric(QStringLiteral("treemap.tilePaddingLeft"), QStringLiteral("Tile left padding"), 0, 1024, 0, true,
                   true);
        addNumeric(QStringLiteral("treemap.tilePaddingTop"), QStringLiteral("Tile top padding"), 0, 1024, 0, true,
                   true);
        addNumeric(QStringLiteral("treemap.tilePaddingRight"), QStringLiteral("Tile right padding"), 0, 1024, 0, true,
                   true);
        addNumeric(QStringLiteral("treemap.tilePaddingBottom"), QStringLiteral("Tile bottom padding"), 0, 1024, 0,
                   true, true);

        SettingsRowSchema fontRow;
        fontRow.key = QStringLiteral("treemap.tileFontName");
        fontRow.label = QStringLiteral("Tile font family");
        fontRow.kind = SettingsEditorKind::Dropdown;
        fontRow.options = fonts;
        rows.push_back(fontRow);

        addNumeric(QStringLiteral("treemap.headingMaxFontSize"), QStringLiteral("Heading maximum font size"), 1, 1024,
                   0, true);
        addNumeric(QStringLiteral("treemap.headingMinFontSize"), QStringLiteral("Heading minimum font size"), 1, 1024,
                   0, true);
        addNumeric(QStringLiteral("treemap.headingLineHeight"), QStringLiteral("Heading line height ratio"), 0.01, 10,
                   3);
        addNumeric(QStringLiteral("treemap.detailsFontSizeRatio"), QStringLiteral("Details font size ratio"), 0.01,
                   10, 3);
        addNumeric(QStringLiteral("treemap.detailsLineHeight"), QStringLiteral("Details line height ratio"), 0.01, 10,
                   3);
        addNumeric(QStringLiteral("treemap.aboveDetailsHeightRatio"), QStringLiteral("Above-details height ratio"),
                   0.01, 10, 3);
        addText(QStringLiteral("treemap.labelPlaceholder"), QStringLiteral("Placeholder label text"), true);
        addText(QStringLiteral("treemap.labelDummy"), QStringLiteral("Dummy label text"), true);

        addColor(QStringLiteral("treemap.nativeFolderBgColor"), QStringLiteral("Native folder background color"));
        addColor(QStringLiteral("treemap.nativeFolderTextColor"), QStringLiteral("Native folder text color"));
        addColor(QStringLiteral("treemap.packedFolderBgColor"), QStringLiteral("Packed folder background color"));
        addColor(QStringLiteral("treemap.packedFolderTextColor"), QStringLiteral("Packed folder text color"));
        addColor(QStringLiteral("treemap.nativeFileBgColor"), QStringLiteral("Native file background color"));
        addColor(QStringLiteral("treemap.nativeFileTextColor"), QStringLiteral("Native file text color"));
        addColor(QStringLiteral("treemap.packedFileBgColor"), QStringLiteral("Packed file background color"));
        addColor(QStringLiteral("treemap.packedFileTextColor"), QStringLiteral("Packed file text color"));
        addColor(QStringLiteral("treemap.nativeClumpBgColor"), QStringLiteral("Native clump background color"));
        addColor(QStringLiteral("treemap.nativeClumpTextColor"), QStringLiteral("Native clump text color"));
        addColor(QStringLiteral("treemap.packedClumpBgColor"), QStringLiteral("Packed clump background color"));
        addColor(QStringLiteral("treemap.packedClumpTextColor"), QStringLiteral("Packed clump text color"));

        addText(QStringLiteral("treemap.win.exeFiles"), QStringLiteral("Windows executable extensions"));
        addText(QStringLiteral("treemap.linux.exeFiles"), QStringLiteral("Linux executable extensions"));
        addText(QStringLiteral("treemap.macos.exeFiles"), QStringLiteral("macOS executable extensions"));
        return rows;
    }();
    return schemas;
}

QVector<SettingsRowState> treemapToRowStates(const config::TreemapSettings& cfg) {
    config::TreemapSettings effective = cfg;
    if (effective.maxTiles == 0) {
        effective = config::defaultTreemapSettings();
    }
    QVector<SettingsRowState> out;
    const auto& schemas = treemapRowSchemas();
    out.reserve(schemas.size());
    for (const SettingsRowSchema& schema : schemas) {
        SettingsRowState state;
        state.schema = &schema;
        state.pendingValue = valueForKey(effective, schema.key);
        state.lastGood = state.pendingValue;
        out.push_back(state);
    }
    return out;
}

bool rowStatesToTreemap(const QVector<SettingsRowState>& states, config::TreemapSettings* out, QString* error) {
    if (!out) {
        return false;
    }
    config::TreemapSettings cfg = config::defaultTreemapSettings();
    for (const SettingsRowState& state : states) {
        if (!state.schema) {
            if (error) {
                *error = QStringLiteral("Internal error: missing schema");
            }
            return false;
        }
        QString rowError;
        if (!setValueForKey(&cfg, state.schema->key, state.pendingValue, &rowError)) {
            if (error) {
                *error = state.schema->label + QStringLiteral(": ") + rowError;
            }
            return false;
        }
    }
    *out = cfg;
    return true;
}

QVector<SettingsValidationError> validateAllRows(const QVector<SettingsRowState>& states) {
    static const QRegularExpression exeRe(QStringLiteral("^[^#\\r\\n]*$"));
    QVector<SettingsValidationError> out;
    for (const SettingsRowState& state : states) {
        if (!state.schema) {
            continue;
        }
        const QString val = state.pendingValue.trimmed();
        const SettingsRowSchema* schema = state.schema;
        QString message;

        switch (schema->kind) {
        case SettingsEditorKind::Text: {
            if (schema->key == QStringLiteral("treemap.win.exeFiles") ||
                schema->key == QStringLiteral("treemap.linux.exeFiles") ||
                schema->key == QStringLiteral("treemap.macos.exeFiles")) {
                if (!exeRe.match(val).hasMatch()) {
                    message = schema->label + QStringLiteral(" contains invalid characters");
                }
            }
            if (message.isEmpty() && schema->requireNonEmpty && val.isEmpty()) {
                message = schema->label + QStringLiteral(" must not be empty");
            }
            break;
        }
        case SettingsEditorKind::Numeric: {
            if (val.isEmpty()) {
                message = schema->label + QStringLiteral(" must not be empty; enter a numeric value");
                break;
            }
            if (schema->allowPointUnits || isPointUnitKey(schema->key)) {
                int pt = 0;
                if (!parsePointsInputToPt(val, schema->allowZeroPoint, &pt)) {
                    message = schema->label + QStringLiteral(" must be a size like 20pt or 4mm");
                    break;
                }
                const double f = pt;
                if (f < schema->minValue || f > schema->maxValue) {
                    message = QStringLiteral("%1 must be between %2pt and %3pt")
                                  .arg(schema->label)
                                  .arg(schema->minValue)
                                  .arg(schema->maxValue);
                }
            } else {
                bool ok = false;
                const double f = val.toDouble(&ok);
                if (!ok) {
                    message = schema->label + QStringLiteral(" must be a number");
                    break;
                }
                if (f < schema->minValue || f > schema->maxValue) {
                    message = QStringLiteral("%1 must be between %2 and %3")
                                  .arg(schema->label)
                                  .arg(schema->minValue)
                                  .arg(schema->maxValue);
                }
            }
            break;
        }
        case SettingsEditorKind::Dropdown:
            if (val.isEmpty()) {
                message = schema->label + QStringLiteral(" must not be empty");
            }
            break;
        case SettingsEditorKind::Color: {
            QColor c;
            if (!parseHexColor(val, &c)) {
                message = schema->label + QStringLiteral(" must be in #RRGGBB format");
            }
            break;
        }
        }

        if (!message.isEmpty()) {
            out.push_back({schema->key, schema->label, message});
        }
    }
    return out;
}

QVector<SettingsValidationError> validateObjectRows(const QVector<SettingsRowState>& states) {
    Q_UNUSED(states);
    return {};
}

}  // namespace wtw::ui
