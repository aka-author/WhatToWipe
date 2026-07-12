#include "ui/AboutDialog.h"

#include "app/Product.h"
#include "platform/AppVersion.h"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace wtw::ui {

namespace {

constexpr int kImageMaxW = 220;
constexpr int kImageMaxH = 200;
constexpr int kContentWidth = 320;

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

void applyCuteStyle(QWidget* dialog, QPushButton* okButton) {
    dialog->setObjectName(QStringLiteral("aboutDialog"));
    okButton->setObjectName(QStringLiteral("aboutOk"));
    dialog->setStyleSheet(QStringLiteral(
        "QDialog#aboutDialog {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #fffaf7, stop:1 #f3ebe4);"
        "}"
        "QFrame#aboutArtFrame {"
        "  background: #fffcf9;"
        "  border: 2px solid #ead9cf;"
        "  border-radius: 18px;"
        "}"
        "QLabel#aboutIntro {"
        "  color: #5a4840;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        "QLabel#aboutBody {"
        "  color: #6f5f56;"
        "  font-size: 12px;"
        "  line-height: 130%;"
        "}"
        "QLabel#aboutVersion {"
        "  color: #7a6a62;"
        "  font-size: 11px;"
        "  background: #efe5dc;"
        "  border: 1px solid #e2d4cb;"
        "  border-radius: 11px;"
        "  padding: 4px 14px;"
        "}"
        "QPushButton#aboutOk {"
        "  min-width: 92px;"
        "  min-height: 30px;"
        "  padding: 4px 22px;"
        "  background: #e8a0b8;"
        "  color: #4a3038;"
        "  border: 1px solid #d88aa4;"
        "  border-radius: 15px;"
        "  font-weight: 600;"
        "}"
        "QPushButton#aboutOk:hover { background: #f0b0c6; }"
        "QPushButton#aboutOk:pressed { background: #d890a8; }"
        "QPushButton#aboutOk:focus { outline: none; border: 1px solid #c87898; }"));
}

QLabel* makeCenteredLabel(const QString& text, const QString& objectName, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    label->setMaximumWidth(kContentWidth);
    label->setMinimumWidth(kContentWidth);
    return label;
}

}  // namespace

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    const QString version = platform::fileVersionDotted();
    const QString product = wtw::app::productDisplayName();

    setWindowTitle(QStringLiteral("About %1").arg(product));

    QPixmap art(QStringLiteral(":/app/about-bunny.png"));
    const QSize cell = aboutImageCellSize(art);
    if (!art.isNull() && art.size() != cell) {
        art = art.scaled(cell, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 18);
    root->setSpacing(10);

    if (!art.isNull()) {
        auto* artFrame = new QFrame(this);
        artFrame->setObjectName(QStringLiteral("aboutArtFrame"));
        auto* artLayout = new QVBoxLayout(artFrame);
        artLayout->setContentsMargins(14, 12, 14, 12);
        auto* image = new QLabel(artFrame);
        image->setPixmap(art);
        image->setAlignment(Qt::AlignCenter);
        artLayout->addWidget(image);
        root->addWidget(artFrame, 0, Qt::AlignHCenter);
        root->addSpacing(4);
    }

    root->addWidget(
        makeCenteredLabel(QStringLiteral("%1 is a disk space analyzer.").arg(product),
                          QStringLiteral("aboutIntro"), this));

    auto* body = makeCenteredLabel(
        QStringLiteral("It scans a folder, shows its subfolders as a treemap,\n"
                       "and helps you decide what to keep, move, or remove."),
        QStringLiteral("aboutBody"), this);
    QFont bodyFont = body->font();
    bodyFont.setPointSize(bodyFont.pointSize());
    body->setFont(bodyFont);
    root->addWidget(body);

    auto* versionLabel =
        makeCenteredLabel(QStringLiteral("Version %1").arg(version), QStringLiteral("aboutVersion"), this);
    versionLabel->setAlignment(Qt::AlignCenter);
    root->addSpacing(2);
    root->addWidget(versionLabel, 0, Qt::AlignHCenter);

    root->addSpacing(6);
    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    auto* ok = new QPushButton(QStringLiteral("OK"), this);
    ok->setDefault(true);
    ok->setAutoDefault(true);
    ok->setCursor(Qt::PointingHandCursor);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    buttonRow->addWidget(ok);
    buttonRow->addStretch();
    root->addLayout(buttonRow);

    applyCuteStyle(this, ok);

    const int framePad = 56;
    const int dlgW = qMax(380, kContentWidth + framePad);
    const int artBlock = art.isNull() ? 0 : cell.height() + 48;
    const int dlgH = qMax(340, artBlock + 210);
    setFixedSize(dlgW, dlgH);
}

}  // namespace wtw::ui
