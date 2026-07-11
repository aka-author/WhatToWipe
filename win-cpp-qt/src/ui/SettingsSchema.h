#pragma once

#include "config/TreemapConfig.h"

#include <QColor>
#include <QString>
#include <QVector>

namespace wtw::ui {

enum class SettingsEditorKind { Text, Numeric, Dropdown, Color };

struct SettingsRowSchema {
    QString key;
    QString label;
    SettingsEditorKind kind = SettingsEditorKind::Text;
    QStringList options;
    double minValue = 0.0;
    double maxValue = 0.0;
    int decimals = 0;
    bool allowPointUnits = false;
    bool allowZeroPoint = false;
    bool requireNonEmpty = false;
};

struct SettingsRowState {
    const SettingsRowSchema* schema = nullptr;
    QString pendingValue;
    QString lastGood;
};

struct SettingsValidationError {
    QString key;
    QString label;
    QString message;
};

const QVector<SettingsRowSchema>& treemapRowSchemas();
QVector<SettingsRowState> treemapToRowStates(const config::TreemapSettings& cfg);
bool rowStatesToTreemap(const QVector<SettingsRowState>& states, config::TreemapSettings* out, QString* error);
QVector<SettingsValidationError> validateAllRows(const QVector<SettingsRowState>& states);
QVector<SettingsValidationError> validateObjectRows(const QVector<SettingsRowState>& states);

QString formatPointsValue(int pt);
bool parsePointsInputToPt(const QString& text, bool allowZero, int* ptOut);
bool parseHexColor(const QString& text, QColor* out);
QString formatRgbHex(const QColor& c);

}  // namespace wtw::ui
