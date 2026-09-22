#include "ui/OrbitalGlassView.h"

#include "core/HitTest.h"
#include "platforms/macos/MacActivation.h"
#include "ui/HoneycombMark.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace locus {

namespace {

constexpr qreal kHubRadius = 44.0;
constexpr qreal kHoverScale = 1.17; // 48px chip → 56px, matching the design
constexpr qreal kIconInset = 4.0;   // chip padding around the icon

// Exponential-smoothing rates (1/s). Higher = snappier; an entrance settles
// in ~180ms at rate 16. Exits run faster than entrances so the widget never
// feels like it is lagging behind the cursor.
constexpr qreal kEmphasisInRate = 16.0;
constexpr qreal kEmphasisOutRate = 22.0;
constexpr qreal kHubRate = 18.0;
constexpr qreal kEpsilon = 0.004;
constexpr qreal kSpinRate = 10.0; // spark head chases the cursor angle
constexpr qreal kSpinEpsilonDeg = 0.2;
constexpr qreal kTailRate = 10.0;
constexpr qreal kTailMaxDeg = 110.0;
constexpr qreal kTailMinDeg = 12.0; // parked spark still reads as an arc

QColor mixColor(const QColor &a, const QColor &b, qreal t) {
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

QColor fadeColor(QColor c, qreal k) {
  c.setAlphaF(c.alphaF() * k);
  return c;
}

} // namespace

OrbitalGlassView::OrbitalGlassView(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  resize(extent_, extent_);
  animTimer_.setInterval(16);
  connect(&animTimer_, &QTimer::timeout, this,
          &OrbitalGlassView::advanceAnimation);
}

void OrbitalGlassView::setScene(const SceneModel &scene) {
  scene_ = scene;
  const QRectF disc = discBounds();
  if (disc.isValid()) {
    const int needed = int(disc.width() + 64.0 + 0.5);
    if (needed != extent_) {
      extent_ = needed;
      updateGeometry(); // OverlayWindow::resizeToContent re-reads sizeHint
    }
  }
  update();
}

void OrbitalGlassView::setAppearance(Appearance appearance) {
  appearance_ = appearance;
  update();
}

void OrbitalGlassView::setBackdrop(Backdrop backdrop) {
  backdrop_ = backdrop;
  update();
}

void OrbitalGlassView::setIcon(const QString &pinId, const QIcon &icon) {
  icons_.insert(pinId, icon);
  update();
}

QRectF OrbitalGlassView::discBounds() const {
  for (const auto &dec : scene_.decorations) {
    if (dec.styleKey == QLatin1String("orbit.disc"))
      return dec.bounds;
  }
  return QRectF();
}

qreal OrbitalGlassView::sparkRadius() const {
  qreal best = 0.0;
  for (const auto &dec : scene_.decorations) {
    if (dec.styleKey == QLatin1String("orbit.track"))
      best = qMax(best, dec.bounds.width() / 2.0);
  }
  if (best <= 0.0) {
    const QRectF disc = discBounds();
    if (disc.isValid())
      best = disc.width() * 0.39;
  }
  return best;
}

const PlacedItem *OrbitalGlassView::hoveredItem() const {
  if (lastHover_.isEmpty())
    return nullptr;
  for (const auto &item : scene_.items) {
    if (item.role == ItemRole::Item && item.id == lastHover_)
      return &item;
  }
  return nullptr;
}

void OrbitalGlassView::setHoverTarget(const QString &id) {
  if (id == lastHover_)
    return;
  lastHover_ = id;
  if (!id.isEmpty())
    hubIconId_ = id;

  if (macReduceMotion()) {
    emphasis_.clear();
    if (hoveredItem()) {
      emphasis_.insert(id, 1.0);
      hubFade_ = 1.0;
    } else {
      hubFade_ = 0.0;
    }
    animTimer_.stop();
    update();
    emit itemHovered(id);
    return;
  }

  if (!animTimer_.isActive()) {
    animClock_.restart();
    animTimer_.start();
  }
  update();
  emit itemHovered(id);
}

void OrbitalGlassView::advanceAnimation() {
  qreal dt = animClock_.restart() / 1000.0;
  dt = qBound<qreal>(0.001, dt, 0.05);
  bool settled = true;

  const auto approach = [dt, &settled](qreal current, qreal target, qreal rate) {
    const qreal next =
        current + (target - current) * (1.0 - qExp(-rate * dt));
    if (qAbs(next - target) > kEpsilon)
      settled = false;
    return next;
  };

  // Chip emphasis: hovered chip eases in, everything else eases out.
  for (auto it = emphasis_.begin(); it != emphasis_.end();) {
    const qreal target = (it.key() == lastHover_) ? 1.0 : 0.0;
    const qreal rate =
        target > it.value() ? kEmphasisInRate : kEmphasisOutRate;
    it.value() = approach(it.value(), target, rate);
    if (target == 0.0 && it.value() < kEpsilon)
      it = emphasis_.erase(it);
    else
      ++it;
  }
  if (!lastHover_.isEmpty() && !emphasis_.contains(lastHover_))
    emphasis_.insert(lastHover_, approach(0.0, 1.0, kEmphasisInRate));

  const PlacedItem *hovered = hoveredItem();
  hubFade_ = approach(hubFade_, hovered ? 1.0 : 0.0, kHubRate);

  // Spark head chases the cursor angle along the shortest arc; the tail
  // stretches with the remaining chase distance (faster sweep = longer tail).
  if (spinInside_) {
    qreal delta = spinTargetDeg_ - spinDeg_;
    while (delta > 180.0)
      delta -= 360.0;
    while (delta < -180.0)
      delta += 360.0;
    if (qAbs(delta) > 0.05)
      spinDir_ = delta > 0.0 ? 1 : -1;
    spinDeg_ += delta * (1.0 - qExp(-kSpinRate * dt));
    if (qAbs(spinTargetDeg_ - spinDeg_) > kSpinEpsilonDeg)
      settled = false;
    const qreal tailTarget =
        qBound(kTailMinDeg, qAbs(delta) * 3.5, kTailMaxDeg);
    spinTailDeg_ += (tailTarget - spinTailDeg_) * (1.0 - qExp(-kTailRate * dt));
    if (qAbs(tailTarget - spinTailDeg_) > 0.5)
      settled = false;
  } else {
    spinTailDeg_ += (0.0 - spinTailDeg_) * (1.0 - qExp(-kTailRate * dt));
  }
  const qreal spinTarget = spinInside_ ? 1.0 : 0.0;
  spinAlpha_ = approach(spinAlpha_, spinTarget, kSpinRate);

  if (settled) {
    animTimer_.stop();
    hubFade_ = hovered ? 1.0 : 0.0;
    if (hovered)
      emphasis_.insert(lastHover_, 1.0);
    spinDeg_ = spinTargetDeg_;
    spinTailDeg_ = spinInside_ ? kTailMinDeg : 0.0;
    spinAlpha_ = spinTarget;
  }
  update();
}

void OrbitalGlassView::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  const bool dark = appearance_ != Appearance::Light;
  const QRectF disc = discBounds();
  if (!disc.isValid())
    return;
  const QPointF center = disc.center();

  // Atmosphere (kept inside the widget rect so the rim never slices it)
  {
    QRadialGradient glow(center, width() * 0.46);
    glow.setColorAt(0.0,
                    dark ? QColor(90, 70, 130, 30) : QColor(190, 180, 210, 24));
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(rect(), glow);
  }

  // Soft shadow under the disc (stacked radial fade)
  {
    const QRectF shadowRect = disc.adjusted(-12, -6, 12, 20);
    QRadialGradient shadow(shadowRect.center(), shadowRect.width() / 2.0);
    shadow.setColorAt(0.0, QColor(0, 0, 0, dark ? 90 : 45));
    shadow.setColorAt(0.85, QColor(0, 0, 0, dark ? 36 : 16));
    shadow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(shadow);
    p.drawEllipse(shadowRect);
  }

  // Disc body. Over a native material (Glass/Frosted) a whisper of tint is
  // enough — the material draws its own rim and specular highlights. Under
  // Tint there is nothing behind the disc but the wallpaper, so the painted
  // fill carries the disc: nearly opaque, with a real border for definition.
  {
    QLinearGradient fill(disc.topLeft(), disc.bottomLeft());
    if (backdrop_ == Backdrop::Tint) {
      if (dark) {
        fill.setColorAt(0.0, QColor(34, 34, 40, 216));
        fill.setColorAt(1.0, QColor(24, 24, 30, 200));
      } else {
        fill.setColorAt(0.0, QColor(252, 252, 253, 226));
        fill.setColorAt(1.0, QColor(242, 242, 246, 206));
      }
      p.setBrush(fill);
      p.setPen(QPen(dark ? QColor(255, 255, 255, 34) : QColor(0, 0, 0, 26), 1));
    } else {
      if (dark) {
        fill.setColorAt(0.0, QColor(255, 255, 255, 14));
        fill.setColorAt(1.0, QColor(255, 255, 255, 7));
      } else {
        fill.setColorAt(0.0, QColor(255, 255, 255, 55));
        fill.setColorAt(1.0, QColor(255, 255, 255, 32));
      }
      p.setBrush(fill);
      p.setPen(QPen(dark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 12), 1));
    }
    p.drawEllipse(disc);
  }

  // Everything inside the widget is clipped to the disc — hover glows near
  // the rim would otherwise spill past the glass onto the desktop.
  p.save();
  QPainterPath discClip;
  discClip.addEllipse(disc);
  p.setClipPath(discClip, Qt::IntersectClip);

  // Track rings
  for (const auto &dec : scene_.decorations) {
    if (dec.styleKey != QLatin1String("orbit.track"))
      continue;
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(dark ? QColor(255, 255, 255, 30) : QColor(0, 0, 0, 22), 1.5));
    p.drawEllipse(dec.bounds);
  }

  // Spark: an arc of light riding the outermost track, circling the hub. The
  // head chases the cursor angle; the tail trails behind the direction of
  // motion, fading out along the ring in dense per-segment steps. Screen
  // blend on dark reads as real light; deeper amber on the light frosted
  // glass.
  if (spinAlpha_ > 0.002) {
    const qreal R = sparkRadius();
    const qreal headRad = qDegreesToRadians(spinDeg_);
    const QPointF headPt(center.x() + R * qCos(headRad),
                         center.y() + R * qSin(headRad));
    if (dark)
      p.setCompositionMode(QPainter::CompositionMode_Screen);
    p.setPen(Qt::NoPen);

    const qreal tail = spinTailDeg_;
    if (tail > 1.0) {
      const qreal start = spinDeg_;                 // at the head
      const qreal end = spinDeg_ - spinDir_ * tail; // at the tail tip
      const int steps = qMax(12, int(tail / 2.0));
      const QColor hot = dark ? QColor(255, 224, 155, 210)
                              : QColor(250, 186, 55, 232);
      for (int i = 0; i < steps; ++i) {
        const qreal a0 = qDegreesToRadians(start + (end - start) * i / steps);
        const qreal a1 =
            qDegreesToRadians(start + (end - start) * (i + 1) / steps);
        QPainterPath quad;
        quad.moveTo(center.x() + (R + 5.0) * qCos(a0),
                    center.y() + (R + 5.0) * qSin(a0));
        quad.lineTo(center.x() + (R + 5.0) * qCos(a1),
                    center.y() + (R + 5.0) * qSin(a1));
        quad.lineTo(center.x() + (R - 5.0) * qCos(a1),
                    center.y() + (R - 5.0) * qSin(a1));
        quad.lineTo(center.x() + (R - 5.0) * qCos(a0),
                    center.y() + (R - 5.0) * qSin(a0));
        quad.closeSubpath();
        const qreal t = qreal(i) / steps; // 0 at head → 1 at tip
        QColor c = hot;
        c.setAlphaF(hot.alphaF() * (1.0 - t) * (1.0 - t) * spinAlpha_);
        p.setBrush(c);
        p.drawPath(quad);
      }
    }

    // Head: small bloom + bright core.
    QRadialGradient bloom(headPt, 30.0);
    bloom.setColorAt(0.0, fadeColor(dark ? QColor(255, 230, 170, 95)
                                         : QColor(255, 200, 90, 105),
                                    spinAlpha_));
    bloom.setColorAt(1.0, QColor(255, 230, 170, 0));
    p.setBrush(bloom);
    p.drawEllipse(headPt, 30.0, 30.0);
    QRadialGradient core(headPt, 9.0);
    core.setColorAt(0.0, fadeColor(dark ? QColor(255, 246, 215, 230)
                                        : QColor(255, 196, 80, 235),
                                   spinAlpha_));
    core.setColorAt(1.0, fadeColor(dark ? QColor(255, 222, 150, 0)
                                        : QColor(250, 186, 55, 0),
                                   spinAlpha_));
    p.setBrush(core);
    p.drawEllipse(headPt, 9.0, 9.0);
    if (dark)
      p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  }

  // Chips. Emphasis eases scale, glow, fill and border per chip, so both the
  // arriving and the departing chip animate instead of snapping.
  for (const auto &item : scene_.items) {
    if (item.role != ItemRole::Item)
      continue;
    const qreal e = emphasis_.value(item.id, 0.0);
    QRectF chip = item.bounds;
    if (e > 0.0) {
      const qreal grow = chip.width() * (kHoverScale - 1.0) * e;
      chip.adjust(-grow / 2.0, -grow / 2.0, grow / 2.0, grow / 2.0);
      const qreal glowR = chip.width() * 0.8;
      const QRectF glowRect(chip.center().x() - glowR, chip.center().y() - glowR,
                            glowR * 2.0, glowR * 2.0);
      QRadialGradient g(glowRect.center(), glowR);
      g.setColorAt(0.0, fadeColor(dark ? QColor(245, 214, 138, 70)
                                       : QColor(245, 200, 100, 85),
                                  e));
      g.setColorAt(1.0, QColor(245, 214, 138, 0));
      p.setPen(Qt::NoPen);
      p.setBrush(g);
      p.drawEllipse(glowRect);
    }

    const QColor fillTop = mixColor(dark ? QColor(255, 255, 255, 41)
                                         : QColor(255, 255, 255, 245),
                                    QColor(255, 235, 191, dark ? 82 : 210), e);
    const QColor fillBottom = mixColor(dark ? QColor(255, 255, 255, 15)
                                            : QColor(238, 240, 246, 228),
                                       QColor(255, 217, 153, dark ? 41 : 160),
                                       e);
    const QColor border =
        mixColor(dark ? QColor(255, 255, 255, 80) : QColor(0, 0, 0, 40),
                 QColor(255, 224, 153, dark ? 153 : 220), e);

    QPainterPath chipPath;
    const qreal radius = chip.width() * 0.29;
    chipPath.addRoundedRect(chip, radius, radius);
    QLinearGradient chipFill(chip.topLeft(), chip.bottomLeft());
    chipFill.setColorAt(0.0, fillTop);
    chipFill.setColorAt(1.0, fillBottom);
    p.setBrush(chipFill);
    p.setPen(QPen(border, 1));
    p.drawPath(chipPath);

    const qreal iconSize =
        qMin(iconSize_, chip.width() - kIconInset * 2.0);
    const QRectF iconRect(chip.center().x() - iconSize / 2.0,
                          chip.center().y() - iconSize / 2.0, iconSize,
                          iconSize);
    const QIcon icon = icons_.value(item.id);
    if (!icon.isNull()) {
      icon.paint(&p, iconRect.toRect());
    } else {
      p.setPen(dark ? QColor(255, 255, 255, 180) : QColor(40, 40, 50));
      QFont f = font();
      f.setPixelSize(qMax(9, int(iconSize / 3.0)));
      p.setFont(f);
      p.drawText(iconRect, Qt::AlignCenter, item.id.left(2).toUpper());
    }
  }

  p.restore();

  // Center hub: Locus logo at idle, hovered app's icon while hovering,
  // crossfaded by hubFade_.
  {
    const QRectF hub(center.x() - kHubRadius, center.y() - kHubRadius,
                     kHubRadius * 2.0, kHubRadius * 2.0);
    QLinearGradient hubFill(hub.topLeft(), hub.bottomLeft());
    if (dark) {
      hubFill.setColorAt(0.0, QColor(255, 255, 255, 40));
      hubFill.setColorAt(1.0, QColor(255, 255, 255, 24));
    } else {
      hubFill.setColorAt(0.0, QColor(252, 253, 255, 250));
      hubFill.setColorAt(1.0, QColor(235, 237, 243, 244));
    }
    p.setBrush(hubFill);
    p.setPen(QPen(dark ? QColor(255, 255, 255, 55) : QColor(0, 0, 0, 55), 1));
    p.drawEllipse(hub);

    constexpr qreal picSize = 32.0; // icon-sized: reads as one among the apps
    const QRectF picRect(center.x() - picSize / 2.0, center.y() - picSize / 2.0,
                         picSize, picSize);

    // Vector honeycomb mark matching the tray icon: pointy-top hexes in a
    // 2-3-2 cluster, amber center — drawn at the current scale, crisp at any
    // size and DPI, which no bitmap tile can be at 32px.
    const auto drawMark = [&] { paintHoneycombMark(p, center, 5.4, dark); };

    p.save();
    QPainterPath clipPath;
    clipPath.addRoundedRect(picRect, 7, 7);
    p.setClipPath(clipPath);
    const QIcon hoverIcon = icons_.value(hubIconId_);
    if (!hoverIcon.isNull() && hubFade_ > 0.0) {
      if (hubFade_ < 1.0) {
        p.setOpacity(1.0 - hubFade_);
        drawMark();
      }
      p.setOpacity(hubFade_);
      hoverIcon.paint(&p, picRect.toRect());
    } else {
      drawMark();
    }
    p.restore();
  }
}

void OrbitalGlassView::mouseMoveEvent(QMouseEvent *event) {
  setHoverTarget(hitTest(scene_, event->position()));

  const QRectF disc = discBounds();
  const qreal r = disc.width() / 2.0;
  const QPointF d = event->position() - disc.center();
  const qreal distSq = d.x() * d.x() + d.y() * d.y();
  spinInside_ = distSq <= r * r && distSq > 100.0; // 10px dead zone at the hub
  if (spinInside_)
    spinTargetDeg_ = qRadiansToDegrees(qAtan2(d.y(), d.x()));

  if (macReduceMotion()) {
    spinDeg_ = spinTargetDeg_; // 1:1 with the cursor, no chase, no tail
    spinTailDeg_ = 0.0;
    spinAlpha_ = spinInside_ ? 1.0 : 0.0;
    update();
    return;
  }
  if (spinInside_ && spinAlpha_ < 0.05) {
    spinDeg_ = spinTargetDeg_; // appear at the cursor, no sweep from stale
    spinTailDeg_ = 0.0;
  }
  if (!animTimer_.isActive()) {
    animClock_.restart();
    animTimer_.start();
  }
}

void OrbitalGlassView::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;
  const QString id = hitTest(scene_, event->position());
  if (!id.isEmpty())
    emit itemActivated(id);
  else if (!discBounds().contains(event->position()))
    emit dismissRequested();
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
  setHoverTarget({});
  spinInside_ = false;
  if (macReduceMotion()) {
    spinAlpha_ = 0.0;
    update();
  } else if (!animTimer_.isActive()) {
    animClock_.restart();
    animTimer_.start();
  }
}

} // namespace locus
