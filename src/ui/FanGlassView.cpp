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

// Pull-out spring: k/damp ≈ ζ 0.65 — quick draw with a small overshoot pop.
constexpr qreal kSpringK = 260.0;
constexpr qreal kSpringDamp = 21.0;
constexpr qreal kEpsilon = 0.004;
// Raised-card geometry, mirrored by the hover hit-test priority region.
constexpr qreal kPullOut = 26.0;
constexpr qreal kRaiseScale = 1.07;

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
  const qreal prev = p.opacity();
  p.setOpacity(prev * opacity);
  p.drawImage(QPointF((card.width() - img.width()) / 2.0,
                      (card.height() - img.height()) / 2.0 + yOff),
              img);
  p.setOpacity(prev);
}

// macOS app icons paint their artwork into only ~80% of the canvas; overscale
// so the artwork — not its transparent margin — fills the box.
constexpr qreal kIconBleed = 1.24;

void paintAppIcon(QPainter &p, const QIcon &icon, const QRectF &box,
                  qreal radius, const QColor &placeholder) {
  QPainterPath clip;
  clip.addRoundedRect(box, radius, radius);
  p.save();
  p.setClipPath(clip, Qt::IntersectClip);
  if (icon.isNull()) {
    p.fillRect(box, placeholder);
  } else {
    const qreal s = box.width() * kIconBleed;
    icon.paint(&p, QRectF(box.center().x() - s / 2.0,
                          box.center().y() - s / 2.0, s, s)
                       .toRect());
  }
  p.restore();
}

// Where a card PAINTS at full emphasis (paintEvent): pulled upright (tilt →
// 0), morphed to a square tile (h → w), lifted by kPullOut, scaled by
// kRaiseScale about its center. Hover AND clicks must resolve against this
// geometry — the rest-shape hit-test would hand the top of the visible card
// to the back hand behind it.
QPolygonF raisedCardShape(const PlacedItem &item) {
  const QPointF c = item.bounds.center();
  const qreal w = item.bounds.width() * kRaiseScale;
  return QPolygonF(
      QRectF(c.x() - w / 2.0, c.y() - kPullOut - w / 2.0, w, w));
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
  // Cards whose face is fully exposed (top of their hand) get a center
  // emblem below the corner pip, like the ace on a real hand.
  exposed_.clear();
  for (const auto &item : scene_.items) {
    if (item.role == ItemRole::Item &&
        hitTest(scene_, item.bounds.center()) == item.id)
      exposed_.insert(item.id);
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
    emphasisVel_.clear();
    if (!id.isEmpty())
      emphasis_[id] = 1.0;
    else if (!scene_.hub.focusedId.isEmpty())
      emphasis_[scene_.hub.focusedId] = 1.0;
    animTimer_.stop(); // values are final — no ticks left to settle
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
    // Underdamped spring: the card pops out of the hand with a slight
    // overshoot instead of gliding exponentially.
    qreal cur = emphasis_.value(item.id, 0.0);
    qreal vel = emphasisVel_.value(item.id, 0.0);
    const qreal acc = kSpringK * (want - cur) - kSpringDamp * vel;
    vel += acc * dt;
    cur += vel * dt;
    if (qAbs(cur - want) > kEpsilon || qAbs(vel) > 0.02) {
      emphasis_[item.id] = cur;
      emphasisVel_[item.id] = vel;
      animating = true;
      dirty = true;
    } else if (!qFuzzyCompare(1.0 + emphasis_.value(item.id), 1.0 + want)) {
      emphasis_[item.id] = want;
      emphasisVel_[item.id] = 0.0;
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
    const qreal ev = qBound(0.0, e, 1.0);   // spring may overshoot
    const QPointF center = item->bounds.center();
    const qreal scale = 1.0 + (kRaiseScale - 1.0) * e;
    // Drawing a card out of a hand: it slides along its own axis and
    // straightens upright as it comes out.
    const qreal tilt = item->angle.value_or(0.0) * (1.0 - ev);

    const qreal w = item->bounds.width();
    const qreal hRest = item->bounds.height();
    // The raised card morphs into a square icon tile: a portrait card fully
    // uncovered with just a centered icon reads as mostly empty space.
    const qreal h = hRest - (hRest - w) * ev;

    p.save();
    p.translate(center);
    p.rotate(qRadiansToDegrees(tilt));
    p.translate(0, -kPullOut * e);
    p.scale(scale, scale);
    p.translate(-w / 2.0, -h / 2.0);

    const QRectF card(0, 0, w, h);
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
      // Icon-only face: fill the square tile, leave just a breathing margin.
      const qreal iconBox = qMin(iconSize_ * 1.7, card.width() * 0.82);
      const QRectF iconRect((card.width() - iconBox) / 2.0,
                            (card.height() - iconBox) / 2.0, iconBox, iconBox);
      p.setOpacity(qMin(1.0, (e - 0.4) * 2.4));
      paintAppIcon(p, icon, iconRect, iconBox * 0.225,
                   dark ? QColor(255, 255, 255, 28) : QColor(0, 0, 0, 14));
      p.setOpacity(1.0);
    } else {
      p.setOpacity(1.0 - e * 2.0);
      if (exposed_.contains(item->id)) {
        // Fully visible face (top of a hand): one centered emblem, no pip —
        // the same icon twice on one card reads as a bug, not an ace.
        const qreal em = card.width() * 0.52;
        paintAppIcon(p, icon,
                     QRectF((card.width() - em) / 2.0,
                            (card.height() - em) / 2.0, em, em),
                     em * 0.225,
                     dark ? QColor(255, 255, 255, 24) : QColor(0, 0, 0, 12));
      } else {
        // Compact index corner — matches a playing-card rank pip.
        paintAppIcon(p, icon, QRectF(7, 8, 30, 30), 8,
                     dark ? QColor(255, 255, 255, 36) : QColor(0, 0, 0, 16));
      }
      p.setOpacity(1.0);
    }
    p.restore();
  }
}

QString FanGlassView::hitTestView(const QPointF &pos) const {
  // The raised card (hovered, or focused while not hovering anything) paints
  // on top of its neighbours and is pulled out and straightened, so give it
  // first claim on the cursor — the plain z-ordered hit-test would keep
  // handing hover and clicks to a card that is visually underneath.
  const QString raisedId =
      !lastHover_.isEmpty() ? lastHover_ : scene_.hub.focusedId;
  if (!raisedId.isEmpty()) {
    for (const auto &item : scene_.items) {
      if (item.id != raisedId || item.role != ItemRole::Item)
        continue;
      if (raisedCardShape(item).containsPoint(pos, Qt::OddEvenFill))
        return raisedId;
      break;
    }
  }
  return hitTest(scene_, pos);
}

void FanGlassView::mouseMoveEvent(QMouseEvent *event) {
  setHoverTarget(hitTestView(event->position()));
}

void FanGlassView::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;
  const QString id = hitTestView(event->position());
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
