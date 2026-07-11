#include "ui/AboutDialog.h"

#include "app/Product.h"
#include "platform/AppVersion.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace wtw::ui {

namespace {

constexpr int kImageMaxW = 300;
constexpr int kImageMaxH = 260;
constexpr int kTextWidth = 300;

QSize aboutImageCellSize(const QPixmap& pixmap) {
    if (pixmap.isNull() || kImageMaxW <= 0 || kImageMaxH <= 0) {
        return QSize(kImageMaxW, kImageMaxH);
    }
    const int sw = pixmap.width();
    const int sh = pixmap.height();
    if (sw <= 0 || sh <= 0) {
        return QSize(kImageMaxW, kImageMaxH);
    }
    const double sfx = static_cast<double>(kImageMaxW) / sw;
    const double sfy = static_cast<double>(kImageMaxH) / sh;
    double scale = sfx;
    if (sfy < scale) {
        scale = sfy;
    }
    if (scale > 1.0) {
        scale = 1.0;
    }
    return QSize(static_cast<int>(sw * scale + 0.5), static_cast<int>(sh * scale + 0.5));
}

QLabel* makeLineLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setWordWrap(false);
    label->setMaximumWidth(kTextWidth);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return label;
}

}  // namespace

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    const QString version = platform::fileVersionDotted();
    setWindowTitle(QStringLiteral("About %1").arg(wtw::app::productDisplayName()));
    setWindowIcon(QIcon(QStringLiteral(":/app/app.ico")));

    QPixmap art(QStringLiteral(":/app/about-bunny.png"));
    const QSize cell = aboutImageCellSize(art);
    if (!art.isNull() && (art.width() != cell.width() || art.height() != cell.height())) {
        art = art.scaled(cell, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(8);

    if (!art.isNull()) {
        auto* image = new QLabel(this);
        image->setPixmap(art);
        image->setFixedSize(cell);
        image->setAlignment(Qt::AlignCenter);
        root->addWidget(image, 0, Qt::AlignHCenter);
    }

    const QString product = wtw::app::productDisplayName();
    root->addWidget(makeLineLabel(QStringLiteral("%1 is a disk space analyzer.").arg(product), this));
    root->addWidget(makeLineLabel(
        QStringLiteral("It scans a folder, shows its subfolders as a treemap,"), this));
    root->addWidget(makeLineLabel(
        QStringLiteral("and helps you decide what to keep, move, or remove."), this));
    root->addWidget(makeLineLabel(QStringLiteral("Version %1").arg(version), this));

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    auto* ok = new QPushButton(QStringLiteral("OK"), this);
    ok->setDefault(true);
    ok->setAutoDefault(true);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    buttonRow->addWidget(ok);
    root->addLayout(buttonRow);

    int dlgW = cell.width() + 80;
    if (dlgW < 360) {
        dlgW = 360;
    }
    int dlgH = cell.height() + 220;
    if (dlgH < 300) {
        dlgH = 300;
    }
    setFixedSize(dlgW, dlgH);
}

}  // namespace wtw::ui
