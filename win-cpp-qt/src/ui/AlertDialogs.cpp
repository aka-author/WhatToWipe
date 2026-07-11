#include "ui/AlertDialogs.h"

#include "app/Product.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace wtw::ui {

namespace {

static const char* kErr001 =
    "The folder could not be opened for scanning. Check that it still exists and that you have permission to read it.";
static const char* kErr002 =
    "Scanning stopped because of an error. The treemap may be incomplete or empty.";
static const char* kErr003 = "The folder could not be opened in File Explorer.";
static const char* kInterruption = "Scanning was interrupted.";

QDialog* makeAlert(QWidget* parent, const QString& title, const QString& body, const QColor& iconColor,
                   const QString& iconGlyph) {
    auto* dlg = new QDialog(parent);
    dlg->setModal(true);
    dlg->setWindowTitle(title);
    auto* layout = new QVBoxLayout(dlg);
    auto* row = new QHBoxLayout();
    auto* icon = new QLabel(dlg);
    icon->setFixedSize(32, 32);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QStringLiteral("background:%1;color:%2;font-weight:bold;")
                            .arg(iconColor.name(), iconColor == QColor(Qt::yellow) ? "#000" : "#fff"));
    icon->setText(iconGlyph);
    auto* text = new QLabel(body, dlg);
    text->setWordWrap(true);
    row->addWidget(icon);
    row->addWidget(text, 1);
    layout->addLayout(row);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, dlg);
    if (auto* ok = buttons->button(QDialogButtonBox::Ok)) {
        ok->setDefault(true);
        ok->setFocus();
    }
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    layout->addWidget(buttons);
    return dlg;
}

void showError(QWidget* parent, const QString& code, const QString& message) {
    const QString body = QStringLiteral("Error code: %1\n\n%2").arg(code, message);
    auto* dlg = makeAlert(parent, wtw::app::productDisplayName(), body, QColor(255, 220, 0), QStringLiteral("!"));
    dlg->exec();
    dlg->deleteLater();
}

}  // namespace

void showError001(QWidget* parent) { showError(parent, QStringLiteral("001"), QString::fromUtf8(kErr001)); }

void showError002(QWidget* parent) { showError(parent, QStringLiteral("002"), QString::fromUtf8(kErr002)); }

void showError003(QWidget* parent) { showError(parent, QStringLiteral("003"), QString::fromUtf8(kErr003)); }

void showError004(QWidget* parent, const QString& folderPath) {
    QString msg = QStringLiteral("The folder %1 is not found. Might you have deleted it in the meantime?")
                      .arg(folderPath);
    showError(parent, QStringLiteral("004"), msg);
}

void showInterruptionAlert(QWidget* parent) {
    auto* dlg = makeAlert(parent, wtw::app::productDisplayName(), QString::fromUtf8(kInterruption),
                          QColor(30, 120, 220), QStringLiteral("i"));
    dlg->exec();
    dlg->deleteLater();
}

}  // namespace wtw::ui
