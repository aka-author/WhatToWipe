#pragma once

#include "config/TreemapConfig.h"
#include "ui/SettingsSchema.h"

#include <QDialog>
#include <QVector>
#include <functional>

class QTableView;
class QLabel;
class QLineEdit;
class QComboBox;
class QWidget;

namespace wtw::ui {

class SettingsGridModel;

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
    struct RowWidgets {
        QWidget* nameCell = nullptr;
        QWidget* valueCell = nullptr;
        QWidget* swatchCell = nullptr;
        QWidget* pickerCell = nullptr;
        QLineEdit* lineEdit = nullptr;
        QComboBox* combo = nullptr;
    };

    void buildGrid();
    void attachRowWidgets(int row);
    void syncWidgetsToStates();
    void syncStatesToWidgets();
    bool collectAndValidate(config::TreemapSettings* out, QString* error, int* errorRow);
    void setError(const QString& text);
    void clearError();
    bool isDirty() const;
    void commitStates();
    void focusRow(int row);
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

    config::TreemapSettings m_initial;
    config::TreemapSettings m_effective;
    QVector<SettingsRowState> m_states;
    QVector<QString> m_committed;
    QVector<RowWidgets> m_rowWidgets;

    SettingsGridModel* m_model = nullptr;
    QTableView* m_table = nullptr;
    QLabel* m_errorLabel = nullptr;
};

void showSettingsDialog(QWidget* parent, const config::TreemapSettings& initial,
                        const std::function<void(const config::TreemapSettings&)>& onApplied);

}  // namespace wtw::ui
