#include "ui/OverlayWindow.h"

#include <QGuiApplication>
#include <QScreen>
#include <QVBoxLayout>

namespace locus {

OverlayWindow::OverlayWindow(QWidget *content, QWidget *parent)
    : QWidget(parent), content_(content) {
  // Qt::Tool is easy to miss on macOS (no dock tile, can fail to raise).
  // Use a normal top-level window that stays above others.
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                 Qt::NoDropShadowWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating, false);
  setWindowTitle(QStringLiteral("Locus"));
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  if (content_) {
    content_->setParent(this);
    layout->addWidget(content_);
  }
  resize(560, 560);
}

void OverlayWindow::setContent(QWidget *content) {
  if (content_ == content)
    return;
  if (content_) {
    content_->setParent(nullptr);
    content_->deleteLater();
  }
  content_ = content;
  if (content_) {
    layout()->addWidget(content_);
  }
}

void OverlayWindow::showAt(QPoint globalCenter) {
  QScreen *screen = QGuiApplication::screenAt(globalCenter);
  if (!screen)
    screen = QGuiApplication::primaryScreen();

  QPoint topLeft = globalCenter - QPoint(width() / 2, height() / 2);
  if (screen) {
    const QRect geo = screen->availableGeometry();
    topLeft.setX(qBound(geo.left(), topLeft.x(), geo.right() - width() + 1));
    topLeft.setY(qBound(geo.top(), topLeft.y(), geo.bottom() - height() + 1));
  }

  move(topLeft);
  show();
  raise();
  activateWindow();
  if (content_)
    content_->setFocus(Qt::ActiveWindowFocusReason);
}

void OverlayWindow::resizeToContent() {
  if (!content_)
    return;
  const QSize hint = content_->sizeHint();
  if (hint.isValid())
    resize(hint);
}

} // namespace locus
