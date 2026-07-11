#include "ui/SettingsDialog.h"

#include "config/ConfigStore.h"

#include <QAbstractTableModel>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace wtw::ui {

enum GridColumn { ColName = 0, ColValue = 1, ColSwatch = 2, ColPicker = 3, ColCount = 4 };

class SettingsGridModel : public QAbstractTableModel {
public:
    explicit SettingsGridModel(QVector<SettingsRowState>* states, QObject* parent = nullptr)
        : QAbstractTableModel(parent), m_states(states) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        if (parent.isValid() || !m_states) {
            return 0;
        }
        return m_states->size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return ColCount;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
            return {};
        }
        switch (section) {
        case ColName:
            return QStringLiteral("Parameter");
        case ColValue:
            return QStringLiteral("Value");
        case ColSwatch:
            return QStringLiteral("Preview");
        case ColPicker:
            return QString();
        default:
            return {};
        }
    }

    QVariant data(const QModelIndex& index, int role) const override {
        Q_UNUSED(role);
        Q_UNUSED(index);
        return {};
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override {
        if (!index.isValid()) {
            return Qt::NoItemFlags;
        }
        return Qt::ItemIsEnabled;
    }

private:
    QVector<SettingsRowState>* m_states = nullptr;
};

namespace {

constexpr int kDefaultDialogW = 540;
constexpr int kDefaultDialogH = 440;
constexpr int kMaxDialogW = 640;
constexpr int kMaxDialogH = 520;
constexpr int kMinDialogW = 380;
constexpr int kMinDialogH = 280;
constexpr int kGridRowHeight = 24;

int clampDialogDimension(int value, int fallback, int minV, int maxV) {
    if (value <= 0 || value > maxV) {
        return fallback;
    }
    return qBound(minV, value, maxV);
}

QLabel* makeNameLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setWordWrap(false);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setMargin(2);
    return label;
}

QLabel* makeSwatch(QWidget* parent) {
    auto* swatch = new QLabel(parent);
    swatch->setFixedSize(24, 16);
    swatch->setFrameStyle(QFrame::Box | QFrame::Plain);
    swatch->setLineWidth(1);
    return swatch;
}

void paintSwatch(QLabel* swatch, const QString& value) {
    if (!swatch) {
        return;
    }
    QColor c;
    if (!parseHexColor(value, &c)) {
        swatch->setStyleSheet(QStringLiteral("background:#ffffff;"));
        return;
    }
    swatch->setStyleSheet(QStringLiteral("background:%1;").arg(c.name()));
}

QWidget* makeEmptyCell(QWidget* parent) {
    auto* cell = new QWidget(parent);
    cell->setMinimumHeight(kGridRowHeight);
    return cell;
}

}  // namespace

SettingsDialog::SettingsDialog(const config::TreemapSettings& initial, QWidget* parent)
    : QDialog(parent), m_initial(initial), m_effective(initial) {
    setWindowTitle(QStringLiteral("Settings"));
    const int dialogW =
        clampDialogDimension(m_initial.settingsDialogW, kDefaultDialogW, kMinDialogW, kMaxDialogW);
    const int dialogH =
        clampDialogDimension(m_initial.settingsDialogH, kDefaultDialogH, kMinDialogH, kMaxDialogH);
    resize(dialogW, dialogH);
    if (m_initial.settingsDialogX || m_initial.settingsDialogY) {
        move(m_initial.settingsDialogX, m_initial.settingsDialogY);
    }

    m_states = treemapToRowStates(initial);
    m_committed.resize(m_states.size());
    commitStates();

    auto* root = new QVBoxLayout(this);
    m_table = new QTableView(this);
    m_model = new SettingsGridModel(&m_states, m_table);
    m_table->setModel(m_model);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(true);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColValue, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColSwatch, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(ColPicker, QHeaderView::Fixed);
    m_table->setColumnWidth(ColSwatch, 36);
    m_table->setColumnWidth(ColPicker, 32);
    m_table->verticalHeader()->setDefaultSectionSize(kGridRowHeight);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    root->addWidget(m_table, 1);

    buildGrid();

    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet(QStringLiteral("color:#b00020"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);

    auto* buttons = new QDialogButtonBox(this);
    auto* resetBtn = buttons->addButton(QStringLiteral("Reset Treemap Defaults"), QDialogButtonBox::ResetRole);
    buttons->addButton(QDialogButtonBox::Apply);
    buttons->addButton(QDialogButtonBox::Ok);
    buttons->addButton(QDialogButtonBox::Cancel);
    root->addWidget(buttons);

    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::onResetDefaults);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &SettingsDialog::onApply);
    connect(buttons->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &SettingsDialog::onOk);
    connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::buildGrid() {
    m_rowWidgets.resize(m_states.size());
    for (int row = 0; row < m_states.size(); ++row) {
        attachRowWidgets(row);
    }
}

void SettingsDialog::attachRowWidgets(int row) {
    if (row < 0 || row >= m_states.size()) {
        return;
    }
    const SettingsRowState& state = m_states[row];
    const SettingsRowSchema* schema = state.schema;
    if (!schema) {
        return;
    }

    RowWidgets widgets;
    widgets.nameCell = makeNameLabel(schema->label, m_table);
    m_table->setIndexWidget(m_model->index(row, ColName), widgets.nameCell);

    if (schema->kind == SettingsEditorKind::Dropdown) {
        widgets.combo = new QComboBox(m_table);
        widgets.combo->setEditable(true);
        widgets.combo->addItems(schema->options);
        widgets.combo->setCurrentText(state.pendingValue);
        widgets.valueCell = widgets.combo;
        connect(widgets.combo, &QComboBox::currentTextChanged, this, [this, row](const QString& text) {
            if (row >= 0 && row < m_states.size()) {
                m_states[row].pendingValue = text;
            }
            clearError();
        });
    } else {
        widgets.lineEdit = new QLineEdit(state.pendingValue, m_table);
        widgets.valueCell = widgets.lineEdit;
        connect(widgets.lineEdit, &QLineEdit::textChanged, this, [this, row, schema](const QString& text) {
            if (row >= 0 && row < m_states.size()) {
                m_states[row].pendingValue = text;
            }
            if (schema->kind == SettingsEditorKind::Color && row < m_rowWidgets.size()) {
                paintSwatch(qobject_cast<QLabel*>(m_rowWidgets[row].swatchCell), text);
            }
            clearError();
        });
    }
    m_table->setIndexWidget(m_model->index(row, ColValue), widgets.valueCell);

    if (schema->kind == SettingsEditorKind::Color) {
        widgets.swatchCell = makeSwatch(m_table);
        paintSwatch(qobject_cast<QLabel*>(widgets.swatchCell), state.pendingValue);
        m_table->setIndexWidget(m_model->index(row, ColSwatch), widgets.swatchCell);

        auto* pick = new QPushButton(QStringLiteral("…"), m_table);
        pick->setFixedSize(28, kGridRowHeight - 4);
        widgets.pickerCell = pick;
        m_table->setIndexWidget(m_model->index(row, ColPicker), pick);
        connect(pick, &QPushButton::clicked, this, [this, row]() {
            if (row < 0 || row >= m_rowWidgets.size() || !m_rowWidgets[row].lineEdit) {
                return;
            }
            QColor initial;
            parseHexColor(m_rowWidgets[row].lineEdit->text(), &initial);
            const QColor chosen = QColorDialog::getColor(initial, this, QStringLiteral("Choose color"));
            if (!chosen.isValid()) {
                return;
            }
            const QString hex = formatRgbHex(chosen);
            m_rowWidgets[row].lineEdit->setText(hex);
            if (row < m_states.size()) {
                m_states[row].pendingValue = hex;
            }
            paintSwatch(qobject_cast<QLabel*>(m_rowWidgets[row].swatchCell), hex);
            clearError();
        });
    } else {
        widgets.swatchCell = makeEmptyCell(m_table);
        widgets.pickerCell = makeEmptyCell(m_table);
        m_table->setIndexWidget(m_model->index(row, ColSwatch), widgets.swatchCell);
        m_table->setIndexWidget(m_model->index(row, ColPicker), widgets.pickerCell);
    }

    m_rowWidgets[row] = widgets;
}

void SettingsDialog::syncWidgetsToStates() {
    for (int row = 0; row < m_states.size() && row < m_rowWidgets.size(); ++row) {
        const RowWidgets& w = m_rowWidgets[row];
        if (w.lineEdit) {
            m_states[row].pendingValue = w.lineEdit->text();
        } else if (w.combo) {
            m_states[row].pendingValue = w.combo->currentText();
        }
    }
}

void SettingsDialog::syncStatesToWidgets() {
    for (int row = 0; row < m_states.size() && row < m_rowWidgets.size(); ++row) {
        const QString value = m_states[row].pendingValue;
        RowWidgets& w = m_rowWidgets[row];
        if (w.lineEdit) {
            w.lineEdit->setText(value);
        } else if (w.combo) {
            w.combo->setCurrentText(value);
        }
        if (m_states[row].schema && m_states[row].schema->kind == SettingsEditorKind::Color) {
            paintSwatch(qobject_cast<QLabel*>(w.swatchCell), value);
        }
    }
}

void SettingsDialog::commitStates() {
    for (int i = 0; i < m_states.size(); ++i) {
        m_committed[i] = m_states[i].lastGood;
    }
}

bool SettingsDialog::isDirty() const {
    for (int i = 0; i < m_states.size(); ++i) {
        if (m_states[i].pendingValue != m_committed[i]) {
            return true;
        }
    }
    return false;
}

void SettingsDialog::focusRow(int row) {
    if (row < 0 || row >= m_rowWidgets.size()) {
        return;
    }
    const RowWidgets& w = m_rowWidgets[row];
    if (w.lineEdit) {
        w.lineEdit->setFocus();
    } else if (w.combo) {
        w.combo->setFocus();
    }
    m_table->scrollTo(m_model->index(row, ColValue));
}

bool SettingsDialog::collectAndValidate(config::TreemapSettings* out, QString* error, int* errorRow) {
    syncWidgetsToStates();
    const QVector<SettingsValidationError> fieldErrors = validateAllRows(m_states);
    if (!fieldErrors.isEmpty()) {
        if (error) {
            *error = fieldErrors.front().message;
        }
        if (errorRow) {
            for (int i = 0; i < m_states.size(); ++i) {
                if (m_states[i].schema && m_states[i].schema->key == fieldErrors.front().key) {
                    *errorRow = i;
                    break;
                }
            }
        }
        return false;
    }
    const QVector<SettingsValidationError> objectErrors = validateObjectRows(m_states);
    if (!objectErrors.isEmpty()) {
        if (error) {
            *error = objectErrors.front().message;
        }
        return false;
    }
    if (!rowStatesToTreemap(m_states, out, error)) {
        return false;
    }
    out->settingsDialogX = x();
    out->settingsDialogY = y();
    out->settingsDialogW = width();
    out->settingsDialogH = height();
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

void SettingsDialog::onResetDefaults() {
    m_states = treemapToRowStates(config::defaultTreemapSettings());
    syncStatesToWidgets();
    clearError();
}

void SettingsDialog::onApply() {
    config::TreemapSettings candidate;
    QString err;
    int errRow = -1;
    if (!collectAndValidate(&candidate, &err, &errRow)) {
        setError(err);
        if (errRow >= 0) {
            focusRow(errRow);
        }
        return;
    }
    clearError();
    if (!config::saveTreemap(candidate)) {
        setError(QStringLiteral("Settings could not be saved. Check that the config file is writable."));
        return;
    }
    m_effective = candidate;
    for (int i = 0; i < m_states.size(); ++i) {
        m_states[i].lastGood = m_states[i].pendingValue;
    }
    commitStates();
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
