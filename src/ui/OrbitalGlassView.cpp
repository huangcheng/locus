#include "ui/OrbitalGlassView.h"

#include "core/HitTest.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

namespace navi {

OrbitalGlassView::OrbitalGlassView(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  resize(560, 560);
}

void OrbitalGlassView::setScene(const SceneModel &scene) {
  scene_ = scene;
  update();
}

void OrbitalGlassView::setAppearance(Appearance appearance) {
  appearance_ = appearance;
  update();
}

void OrbitalGlassView::setIcon(const QString &pinId, const QIcon &icon) {
  icons_.insert(pinId, icon);
  update();
}

void OrbitalGlassView::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  const bool dark = appearance_ != Appearance::Light;
  const QRectF disc(70, 70, 420, 420);

  // Atmosphere
  {
    QRadialGradient glow(rect().center(), width() * 0.55);
    glow.setColorAt(0.0, dark ? QColor(80, 50, 20, 60) : QColor(200, 180, 140, 40));
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect(), glow);
  }

  // Smoke disc — keep readable on both light/dark desktops
  {
    QRadialGradient fill(disc.center(), disc.width() / 2.0);
    if (dark) {
      fill.setColorAt(0.0, QColor(70, 55, 40, 220));
      fill.setColorAt(0.45, QColor(35, 28, 22, 235));
      fill.setColorAt(1.0, QColor(12, 10, 8, 245));
    } else {
      fill.setColorAt(0.0, QColor(255, 255, 255, 240));
      fill.setColorAt(0.55, QColor(248, 248, 252, 250));
      fill.setColorAt(1.0, QColor(230, 234, 240, 255));
    }
    p.setBrush(fill);
    p.setPen(QPen(dark ? QColor(255, 220, 160, 100) : QColor(120, 130, 150, 90), 1.25));
    p.drawEllipse(disc);
  }

  auto penFor = [&](const QString &key) -> QPen {
    if (key == QLatin1String("orbital.inner"))
      return QPen(dark ? QColor(255, 224, 166, 82) : QColor(115, 82, 31, 71), 1.5);
    if (key == QLatin1String("orbital.outer"))
      return QPen(dark ? QColor(255, 255, 255, 51) : QColor(51, 64, 89, 56), 1.25);
    if (key == QLatin1String("hover.well"))
      return QPen(dark ? QColor(255, 224, 140, 140) : QColor(191, 140, 51, 128), 1.5);
    return QPen(Qt::NoPen);
  };

  for (const auto &dec : scene_.decorations) {
    if (dec.kind != DecorationKind::Ellipse)
      continue;
    if (dec.styleKey == QLatin1String("hover.well")) {
      QRadialGradient well(dec.bounds.center(), dec.bounds.width() / 2.0);
      well.setColorAt(0.0, dark ? QColor(242, 199, 102, 90) : QColor(242, 199, 102, 102));
      well.setColorAt(0.55, QColor(217, 140, 51, 26));
      well.setColorAt(1.0, QColor(0, 0, 0, 0));
      p.setBrush(well);
    } else {
      p.setBrush(Qt::NoBrush);
    }
    p.setPen(penFor(dec.styleKey));
    p.drawEllipse(dec.bounds);
  }

  // Core hub
  {
    const QRectF core(200, 200, 160, 160);
    QRadialGradient coreFill(core.center(), 80);
    if (dark) {
      coreFill.setColorAt(0.0, QColor(60, 45, 30, 200));
      coreFill.setColorAt(1.0, QColor(20, 14, 10, 230));
    } else {
      coreFill.setColorAt(0.0, QColor(255, 255, 255, 240));
      coreFill.setColorAt(1.0, QColor(245, 245, 248, 255));
    }
    p.setBrush(coreFill);
    p.setPen(QPen(dark ? QColor(255, 220, 160, 90) : QColor(180, 160, 120, 120), 1));
    p.drawEllipse(core);

    p.setPen(dark ? QColor(242, 199, 102) : QColor(120, 80, 30));
    QFont title = font();
    title.setPixelSize(18);
    title.setBold(true);
    p.setFont(title);
    p.drawText(QRectF(200, 248, 160, 28), Qt::AlignCenter, scene_.hub.selectedTitle);

    p.setPen(dark ? QColor(180, 180, 180, 160) : QColor(140, 140, 150));
    QFont brand = font();
    brand.setPixelSize(11);
    p.setFont(brand);
    p.drawText(QRectF(200, 278, 160, 18), Qt::AlignCenter, scene_.hub.brandSubtitle);
  }

  for (const auto &item : scene_.items) {
    if (item.role != ItemRole::Item)
      continue;
    const QRectF r = item.bounds;
    QPainterPath path;
    path.addRoundedRect(r, 10, 10);
    p.fillPath(path, dark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 15));

    const QIcon icon = icons_.value(item.id);
    if (!icon.isNull()) {
      icon.paint(&p, r.toRect());
    } else {
      p.setPen(dark ? QColor(255, 255, 255, 180) : QColor(40, 40, 50));
      QFont f = font();
      f.setPixelSize(qMax(9, int(r.height() / 4)));
      p.setFont(f);
      p.drawText(r, Qt::AlignCenter, item.id.left(2).toUpper());
    }
  }
}

void OrbitalGlassView::mouseMoveEvent(QMouseEvent *event) {
  const QString id = hitTest(scene_, event->position());
  if (id != lastHover_) {
    lastHover_ = id;
    emit itemHovered(id);
  }
}

void OrbitalGlassView::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;
  const QString id = hitTest(scene_, event->position());
  if (!id.isEmpty())
    emit itemActivated(id);
  else if (!QRectF(70, 70, 420, 420).contains(event->position()))
    emit dismissRequested();
}

void OrbitalGlassView::wheelEvent(QWheelEvent *event) {
  const qreal delta = event->angleDelta().y() / 8.0 / 15.0; // notches
  emit rotationDelta(delta * 0.12);
}

void OrbitalGlassView::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape)
    emit dismissRequested();
  else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
           !lastHover_.isEmpty())
    emit itemActivated(lastHover_);
  else
    QWidget::keyPressEvent(event);
}

void OrbitalGlassView::leaveEvent(QEvent *) {
  if (!lastHover_.isEmpty()) {
    lastHover_.clear();
    emit itemHovered({});
  }
}

} // namespace navi
