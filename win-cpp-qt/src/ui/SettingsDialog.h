#pragma once

#include "config/TreemapConfig.h"
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVector>
#include <functional>

namespace wtw::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const config::TreemapSettings& initial, QWidget* parent = nullptr);

    config::TreemapSettings effectiveSettings() const { return m_effective; }

signals:
    void settingsApplied(const config::TreemapSettings& settings);

private slots:
    void onResetDefaults();
    void onApply();
    void onOk();

private:
    bool validateAndCollect(config::TreemapSettings* out, QString* error);
    void setError(const QString& text);
    void clearError();
    void loadIntoEditors(const config::TreemapSettings& s);
    bool isDirty() const;

    config::TreemapSettings m_initial;
    config::TreemapSettings m_effective;
    QLabel* m_errorLabel = nullptr;

    struct RowEditors {
        QString key;
        QLineEdit* value = nullptr;
        QLineEdit* colorEdit = nullptr;
        QLabel* swatch = nullptr;
        QPushButton* pick = nullptr;
        QComboBox* combo = nullptr;
        QSpinBox* spin = nullptr;
        QDoubleSpinBox* dspin = nullptr;
    };
    QVector<RowEditors> m_rows;
};

void showSettingsDialog(QWidget* parent, const config::TreemapSettings& initial,
                        const std::function<void(const config::TreemapSettings&)>& onApplied);

}  // namespace wtw::ui
