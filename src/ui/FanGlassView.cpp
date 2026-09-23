#include "ui/FanGlassView.h"

#include "core/HitTest.h"
#include "platforms/macos/MacActivation.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace locus {
namespace {

constexpr qreal kEmphasisInRate = 18.0;
constexpr qreal kEmphasisOutRate = 24.0;
constexpr qreal kEpsilon = 0.004;

QColor mixColor(const QColor &a, const QColor &b, qreal t) {
  return QColor(int(a.red() + (b.red() - a.red()) * t),
                int(a.green() + (b.green() - a.green()) * t),
                int(a.blue() + (b.blue() - a.blue()) * t),
                int(a.alpha() + (b.alpha() - a.alpha()) * t));
}

// Soft-shadow cache: a blurred rounded-rect alpha mask per card size. Every
// card in a hand shares geometry, so each entry is hit constantly.
QHash<QString, QImage> gShadowCache;

QImage cardShadowMap(const QSizeF &size, qreal radius, qreal blur) {
  const QString key = QStringLiteral("%1x%2:%3:%4")
                          .arg(qRound(size.width() * 2))
                          .arg(qRound(size.height() * 2))
                          .arg(qRound(radius * 2))
                          .arg(qRound(blur * 2));
  const auto it = gShadowCache.constFind(key);
  if (it != gShadowCache.constEnd())
    return it.value();

  const int m = qCeil(blur * 2.0);
  QImage img(QSize(qCeil(size.width()) + 2 * m, qCeil(size.height()) + 2 * m),
             QImage::Format_Alpha8);
  img.fill(0);
  {
    QPainter ip(&img);
    ip.setRenderHint(QPainter::Antialiasing, true);
    ip.setPen(Qt::NoPen);
    ip.setBrush(Qt::black);
    ip.drawRoundedRect(QRectF(m, m, size.width(), size.height()), radius,
                       radius);
  }

  // Three separable box-blur passes approximate a gaussian.
  const int r = qMax(1, qRound(blur / 2.0));
  const int w = img.width();
  const int h = img.height();
  const int div = 2 * r + 1;
  QImage tmp(img.size(), QImage::Format_Alpha8);
  for (int pass = 0; pass < 3; ++pass) {
    for (int y = 0; y < h; ++y) {
      const uchar *src = img.constScanLine(y);
      uchar *dst = tmp.scanLine(y);
      int acc = 0;
      for (int x = -r; x <= r; ++x)
        acc += src[qBound(0, x, w - 1)];
      for (int x = 0; x < w; ++x) {
        dst[x] = uchar(acc / div);
        acc += src[qBound(0, x + r + 1, w - 1)] - src[qBound(0, x - r, w - 1)];
      }
    }
    for (int x = 0; x < w; ++x) {
      int acc = 0;
      for (int y = -r; y <= r; ++y)
        acc += tmp.constScanLine(qBound(0, y, h - 1))[x];
      for (int y = 0; y < h; ++y) {
        img.scanLine(y)[x] = uchar(acc / div);
        acc += tmp.constScanLine(qBound(0, y + r + 1, h - 1))[x] -
               tmp.constScanLine(qBound(0, y - r, h - 1))[x];
      }
    }
  }
  gShadowCache.insert(key, img);
  return img;
}

void drawShadow(QPainter &p, const QRectF &card, qreal radius, qreal blur,
                qreal opacity, qreal yOff) {
  if (opacity <= 0.01)
    return;
  const QImage img = cardShadowMap(card.size(), radius, blur);
  p.setOpacity(opacity);
  p.drawImage(QPointF((card.width() - img.width()) / 2.0,
                      (card.height() - img.height()) / 2.0 + yOff),
              img);
  p.setOpacity(1.0);
}

} // namespace

FanGlassView::FanGlassView(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  animTimer_.setInterval(16);
  connect(&animTimer_, &QTimer::timeout, this, &FanGlassView::advanceAnimation);
}

void FanGlassView::setScene(const SceneModel &scene) {
  scene_ = scene;
  extent_ = 560;
  for (const auto &dec : scene.decorations) {
    if (dec.styleKey == QLatin1String("fan.canvas")) {
      extent_ = qMax(320, int(std::ceil(qMax(dec.bounds.width(),
                                             dec.bounds.height()))));
      break;
    }
  }
  updateGeometry();
  update();
}

void FanGlassView::setAppearance(Appearance appearance) {
  appearance_ = appearance;
  update();
}

void FanGlassView::setIcon(const QString &pinId, const QIcon &icon) {
  icons_.insert(pinId, icon);
  update();
}

void FanGlassView::setHoverTarget(const QString &id) {
  if (id == lastHover_)
    return;
  lastHover_ = id;
  if (!id.isEmpty())
    emit itemHovered(id);
  if (macReduceMotion()) {
    for (auto it = emphasis_.begin(); it != emphasis_.end(); ++it)
      it.value() = 0.0;
    if (!id.isEmpty())
      emphasis_[id] = 1.0;
    else if (!scene_.hub.focusedId.isEmpty())
      emphasis_[scene_.hub.focusedId] = 1.0;
    update();
    return;
  }
  if (!animTimer_.isActive()) {
    animClock_.restart();
    animTimer_.start();
  }
}

void FanGlassView::advanceAnimation() {
  const qreal dt = qMin(0.05, animClock_.restart() / 1000.0);
  bool dirty = false;
  bool animating = false;

  for (const auto &item : scene_.items) {
    if (item.role != ItemRole::Item)
      continue;
    qreal want = 0.0;
    if (item.id == lastHover_ ||
        (lastHover_.isEmpty() && item.id == scene_.hub.focusedId))
      want = 1.0;
    else if (item.id == scene_.hub.focusedId)
      want = 0.4;
    const qreal cur = emphasis_.value(item.id, 0.0);
    const qreal rate = want > cur ? kEmphasisInRate : kEmphasisOutRate;
    const qreal next = cur + (want - cur) * (1.0 - std::exp(-rate * dt));
    if (qAbs(next - want) > kEpsilon) {
      emphasis_[item.id] = next;
      animating = true;
      dirty = true;
    } else if (!qFuzzyCompare(1.0 + cur, 1.0 + want)) {
      emphasis_[item.id] = want;
      dirty = true;
    }
  }

  if (dirty)
    update();
  if (!animating)
    animTimer_.stop();
}

void FanGlassView::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  p.setRenderHint(QPainter::TextAntialiasing, true);
  const bool dark = appearance_ != Appearance::Light;

  QVector<const PlacedItem *> cards;
  cards.reserve(scene_.items.size());
  for (const auto &item : scene_.items) {
    if (item.role == ItemRole::Item)
      cards.push_back(&item);
  }
  std::sort(cards.begin(), cards.end(),
            [](const PlacedItem *a, const PlacedItem *b) { return a->z < b->z; });
  // The raised card must never stay covered by its neighbours: pull every
  // face-up card out of the z order and draw it last.
  std::stable_partition(cards.begin(), cards.end(), [&](const PlacedItem *it) {
    const qreal e =
        emphasis_.value(it->id, it->id == scene_.hub.focusedId ? 1.0 : 0.0);
    return e <= 0.5;
  });

  for (const PlacedItem *item : cards) {
    const bool focused = item->id == scene_.hub.focusedId;
    const qreal e = emphasis_.value(item->id, focused ? 1.0 : 0.0);
    const bool faceUp = e > 0.5;
    const qreal tilt = item->angle.value_or(0.0);
    const QPointF center = item->bounds.center();
    const qreal scale = 1.0 + 0.05 * e;

    p.save();
    p.translate(center);
    p.rotate(qRadiansToDegrees(tilt));
    p.translate(0, -14.0 * e);
    p.scale(scale, scale);
    p.translate(-item->bounds.width() / 2.0, -item->bounds.height() / 2.0);

    const QRectF card(0, 0, item->bounds.width(), item->bounds.height());
    const qreal radius = 14.0;
    const QColor amber = dark ? QColor(247, 176, 60) : QColor(214, 134, 22);

    // Real soft shadows: resting card hugs the hand, raised card floats.
    // Rest shadows stack across a hand, so keep them faint.
    drawShadow(p, card, radius + 2, 6.0, (dark ? 0.10 : 0.11) * (1.0 - e),
               3.0);
    drawShadow(p, card, radius + 3, 11.0, (dark ? 0.50 : 0.34) * e, 9.0);

    // Amber glow ring bleeding out from under a raised card.
    if (e > 0.01) {
      for (int i = 3; i >= 1; --i) {
        const qreal d = i * 1.8;
        QColor g = amber;
        g.setAlpha(qRound((dark ? 30.0 : 22.0) * e / i));
        QPainterPath glow;
        glow.addRoundedRect(card.adjusted(-d, -d, d, d), radius + d,
                            radius + d);
        p.setPen(QPen(g, 1.3));
        p.setBrush(Qt::NoBrush);
        p.drawPath(glow);
      }
    }

    QLinearGradient fill(0, 0, 0, card.height());
    QColor border;
    qreal borderW = 1.0;
    if (faceUp) {
      if (dark) {
        fill.setColorAt(0, QColor(60, 60, 70));
        fill.setColorAt(1, QColor(40, 40, 48));
      } else {
        fill.setColorAt(0, QColor(255, 255, 255));
        fill.setColorAt(1, QColor(246, 246, 250));
      }
      border = amber;
      border.setAlpha(dark ? 235 : 225);
      borderW = 1.5;
    } else if (dark) {
      fill.setColorAt(0, QColor(255, 255, 255, 40));
      fill.setColorAt(1, QColor(255, 255, 255, 18));
      border = QColor(255, 255, 255, 54);
    } else {
      fill.setColorAt(0, QColor(255, 255, 255));
      fill.setColorAt(1, QColor(241, 241, 245));
      border = QColor(0, 0, 0, 24);
    }
    // Keyboard focus (emphasis below the face-up flip) warms the rim.
    if (!faceUp && e > 0.05)
      border = mixColor(border, amber, e * 0.85);

    QPainterPath path;
    path.addRoundedRect(card, radius, radius);
    p.setBrush(fill);
    p.setPen(QPen(border, borderW));
    p.drawPath(path);
    p.setClipPath(path);

    const QIcon icon = icons_.value(item->id);
    if (faceUp) {
      // Icon-only face: centered, no caption.
      const qreal iconBox = qMin(iconSize_ * 1.15, card.width() * 0.62);
      const QRectF iconRect((card.width() - iconBox) / 2.0,
                            (card.height() - iconBox) / 2.0, iconBox, iconBox);
      QPainterPath iconClip;
      iconClip.addRoundedRect(iconRect, 11.0, 11.0);
      p.save();
      p.setClipPath(iconClip, Qt::IntersectClip);
      if (!icon.isNull())
        icon.paint(&p, iconRect.toRect());
      else
        p.fillRect(iconRect, dark ? QColor(255, 255, 255, 28)
                                  : QColor(0, 0, 0, 14));
      p.restore();
    } else {
      // Compact index corner — matches a playing-card rank pip.
      const QRectF iconRect(8, 9, 28, 28);
      QPainterPath iconClip;
      iconClip.addRoundedRect(iconRect, 8, 8);
      p.save();
      p.setClipPath(iconClip, Qt::IntersectClip);
      if (!icon.isNull())
        icon.paint(&p, iconRect.toRect());
      else
        p.fillRect(iconRect,
                   dark ? QColor(255, 255, 255, 36) : QColor(0, 0, 0, 16));
      p.restore();
    }
    p.restore();
  }
}

void FanGlassView::mouseMoveEvent(QMouseEvent *event) {
  const QPointF pos = event->position();
  // The raised card paints on top of its neighbours and is lifted/scaled, so
  // give it first claim on the cursor — otherwise the plain z-ordered
  // hit-test would keep handing hover to a card that is visually underneath.
  if (!lastHover_.isEmpty()) {
    for (const auto &item : scene_.items) {
      if (item.id != lastHover_ || item.role != ItemRole::Item)
        continue;
      const QPointF c = item.bounds.center();
      QTransform t;
      t.translate(c.x(), c.y());
      t.rotate(qRadiansToDegrees(item.angle.value_or(0.0)));
      t.translate(0, -14.0);
      t.scale(1.05, 1.05);
      t.translate(-c.x(), -c.y());
      const QPolygonF base =
          (item.shape && !item.shape->isEmpty()) ? *item.shape
                                                 : QPolygonF(item.bounds);
      if (t.map(base).containsPoint(pos, Qt::OddEvenFill)) {
        setHoverTarget(lastHover_);
        return;
      }
      break;
    }
  }
  setHoverTarget(hitTest(scene_, pos));
}

void FanGlassView::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;
  const QString id = hitTest(scene_, event->position());
  if (!id.isEmpty())
    emit itemActivated(id);
  else
    emit dismissRequested();
}

void FanGlassView::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape)
    emit dismissRequested();
  else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
           !scene_.hub.focusedId.isEmpty())
    emit itemActivated(scene_.hub.focusedId);
  else
    QWidget::keyPressEvent(event);
}

void FanGlassView::leaveEvent(QEvent *) { setHoverTarget({}); }

} // namespace locus
