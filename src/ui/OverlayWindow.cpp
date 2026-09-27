#include "ui/OverlayWindow.h"

#include "platforms/CrystalBackdrop.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QScreen>
#include <QVBoxLayout>

namespace locus {
namespace {

// The overlay's single focus target: typing filters, Esc clears-then-
// dismisses, Return activates. (Empty class body needed for signals.)
class SearchField : public QLineEdit {
  Q_OBJECT
public:
  using QLineEdit::QLineEdit;

signals:
  void dismissed();
  void activated();

protected:
  void keyPressEvent(QKeyEvent *event) override {
    const int key = event->key();
    if (key == Qt::Key_Escape) {
      if (text().isEmpty())
        emit dismissed();
      else
        clear();
      event->accept();
      return;
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
      emit activated();
      event->accept();
      return;
    }
    QLineEdit::keyPressEvent(event);
  }
};

} // namespace


OverlayWindow::OverlayWindow(QWidget *content, QWidget *parent)
    : QWidget(parent), content_(content) {
  // Qt::Tool is easy to miss on macOS (no dock tile, can fail to raise),
  // but on Windows a plain Qt::Window shows a taskbar button for the widget.
#ifdef Q_OS_WIN
  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                 Qt::NoDropShadowWindowHint);
#else
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                 Qt::NoDropShadowWindowHint);
#endif
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating, false);
  setWindowTitle(QStringLiteral("Locus"));
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Search strip: always visible, always focused while the overlay is open.
  // Neutral gray alpha works over both dark and light glass.
  search_ = new SearchField(this);
  search_->setObjectName(QStringLiteral("overlaySearch"));
  search_->setPlaceholderText(tr("Type to filter…"));
  search_->setStyleSheet(QStringLiteral(
      "QLineEdit#overlaySearch { background: rgba(127,127,127,56);"
      " border: none; border-radius: 8px; padding: 8px 14px; font-size: 15px;"
      " margin: 10px 12px 4px 12px; }"));
  layout->addWidget(search_);
  connect(search_, &QLineEdit::textChanged, this,
          &OverlayWindow::searchChanged);
  auto *field = static_cast<SearchField *>(search_);
  connect(field, &SearchField::dismissed, this,
          &OverlayWindow::searchDismissed);
  connect(field, &SearchField::activated, this,
          &OverlayWindow::searchActivated);
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
  // The search field owns keyboard focus; the view is mouse-only.
  search_->setFocus(Qt::ActiveWindowFocusReason);
}

QString OverlayWindow::searchQuery() const { return search_->text(); }

void OverlayWindow::resetSearch() { search_->clear(); }

void OverlayWindow::resizeToContent() {
  // The content's sizeHint covers the grid only; add the search strip's
  // height (field + its stylesheet margins) or the grid's bottom clips.
  if (!content_)
    return;
  const QSize hint = content_->sizeHint();
  if (hint.isValid()) {
    // Stylesheet margins aren't part of sizeHint: 10px top + 4px bottom.
    resize(hint.width(), hint.height() + search_->sizeHint().height() + 14);
  }
}

void OverlayWindow::hideEvent(QHideEvent *event) {
  // The crystal glass is a detached companion window — nothing hides it
  // automatically, so drop it whenever the overlay goes away.
  installCrystalBackdrop(this, QRectF(), false);
  QWidget::hideEvent(event);
}

} // namespace locus

// SearchField is file-local with Q_OBJECT — pull in its metaobject code.
#include "OverlayWindow.moc"
