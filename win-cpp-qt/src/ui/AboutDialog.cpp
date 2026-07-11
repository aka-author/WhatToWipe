#include "ui/AboutDialog.h"

#include "app/Product.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

namespace wtw::ui {

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("About %1").arg(wtw::app::productDisplayName()));
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel(wtw::app::productDisplayName(), this);
    QFont f = title->font();
    f.setPointSize(f.pointSize() + 4);
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);
    const QString ver = QCoreApplication::applicationVersion();
    layout->addWidget(new QLabel(QStringLiteral("Version %1").arg(ver), this));
    layout->addWidget(
        new QLabel(QStringLiteral("Disk usage treemap for Windows (functional specification)."), this));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
}

}  // namespace wtw::ui
