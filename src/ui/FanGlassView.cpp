#include "ui/FanGlassView.h"

#include "core/HitTest.h"
#include "platform/MacActivation.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace locus {
namespace {

constexpr qreal kEmphasisInRate = 18.0;
constexpr qreal kEmphasisOutRate = 24.0;
constexpr qreal kEpsilon = 0.004;

QString elideLabel(const QFontMetrics &fm, const QString &text, qreal width) {
  return fm.elidedText(text, Qt::ElideRight, int(width));
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

QString FanGlassView::labelFor(const QString &id) const {
  for (const auto &item : scene_.items)
    if (item.id == id && !item.label.isEmpty())
      return item.label;
  if (id == scene_.hub.focusedId && !scene_.hub.selectedTitle.isEmpty())
    return scene_.hub.selectedTitle;
  return id;
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
    p.translate(0, -12.0 * e);
    p.scale(scale, scale);
    p.translate(-item->bounds.width() / 2.0, -item->bounds.height() / 2.0);

    const QRectF card(0, 0, item->bounds.width(), item->bounds.height());
    const qreal radius = 18.0;

    // Soft contact shadow only (no fat gold blob).
    {
      p.save();
      p.translate(0, 3 + 2 * e);
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(0, 0, 0, faceUp ? (dark ? 90 : 48) : (dark ? 55 : 26)));
      QPainterPath shadow;
      shadow.addRoundedRect(card.adjusted(1, 2, -1, 2), radius, radius);
      p.setOpacity(dark ? 0.7 : 0.55);
      p.drawPath(shadow);
      p.restore();
    }

    // Dark matches Ardot Fan: frosted glass + muted label; selected = gold rim.
    QColor fill;
    QColor border;
    if (faceUp) {
      fill = dark ? QColor(245, 214, 138, 48) : QColor(255, 250, 236);
      border = dark ? QColor(245, 214, 138, 210) : QColor(232, 186, 72);
    } else if (dark) {
      fill = QColor(255, 255, 255, 22);   // ~8% glass
      border = QColor(255, 255, 255, 32); // ~12% rim
    } else {
      fill = QColor(255, 255, 255);
      border = QColor(0, 0, 0, 20);
    }
    QPainterPath path;
    path.addRoundedRect(card, radius, radius);
    p.setBrush(fill);
    p.setPen(QPen(border, faceUp ? 1.75 : 0.9));
    p.drawPath(path);
    p.setClipPath(path);

    const QIcon icon = icons_.value(item->id);
    if (faceUp) {
      const qreal iconBox = 46.0;
      const QRectF iconRect((card.width() - iconBox) / 2.0, 18.0, iconBox,
                            iconBox);
      QPainterPath iconClip;
      iconClip.addRoundedRect(iconRect, 11.0, 11.0);
      p.save();
      p.setClipPath(iconClip, Qt::IntersectClip);
      if (!icon.isNull())
        icon.paint(&p, iconRect.toRect());
      else
        p.fillRect(iconRect, dark ? QColor(255, 255, 255, 28) : QColor(0, 0, 0, 16));
      p.restore();

      QFont font = p.font();
      font.setPixelSize(11);
      font.setWeight(QFont::DemiBold);
      p.setFont(font);
      p.setPen(dark ? QColor(245, 214, 138) : QColor(92, 64, 14));
      const QRectF labelRect(8, 72, card.width() - 16, 20);
      p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignVCenter,
                 elideLabel(QFontMetrics(font), labelFor(item->id),
                            labelRect.width()));
    } else {
      // Compact index corner — matches a playing-card rank pip.
      const QRectF iconRect(7, 9, 26, 26);
      QPainterPath iconClip;
      iconClip.addRoundedRect(iconRect, 7, 7);
      p.save();
      p.setClipPath(iconClip, Qt::IntersectClip);
      if (!icon.isNull())
        icon.paint(&p, iconRect.toRect());
      else
        p.fillRect(iconRect,
                   dark ? QColor(255, 255, 255, 36) : QColor(0, 0, 0, 18));
      p.restore();
    }
    p.restore();
  }
}

void FanGlassView::mouseMoveEvent(QMouseEvent *event) {
  setHoverTarget(hitTest(scene_, event->position()));
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
