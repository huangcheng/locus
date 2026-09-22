#include "layout/FanLayoutStrategy.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace locus {
namespace {

// Playing-card hands: each row is a tight left→right stack.
constexpr qreal kCardW = 84.0;
constexpr qreal kCardH = 110.0;
constexpr qreal kPeek = 22.0;       // readable index strip without looking sparse
constexpr qreal kArch = 40.0;
constexpr qreal kMaxTiltDeg = 34.0;
constexpr int kPerHand = 12;
constexpr qreal kLanePitch = 168.0; // keep stacked hands from colliding
constexpr qreal kMargin = 64.0;

QPolygonF rotatedCardPolygon(QPointF center, qreal w, qreal h, qreal tiltRad) {
  const qreal hw = w / 2.0;
  const qreal hh = h / 2.0;
  const QPointF corners[4] = {{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}};
  const qreal c = std::cos(tiltRad);
  const qreal s = std::sin(tiltRad);
  QPolygonF poly;
  poly.reserve(4);
  for (const QPointF &pt : corners) {
    poly << QPointF(center.x() + pt.x() * c - pt.y() * s,
                    center.y() + pt.x() * s + pt.y() * c);
  }
  return poly;
}

} // namespace

SceneModel FanLayoutStrategy::build(const QVector<Pin> &pins,
                                    const QString &focusedId,
                                    const DensityPrefs &density) {
  SceneModel scene;

  const qreal scale =
      (density.widgetSize > 0 ? density.widgetSize : 560.0) / 560.0;
  const qreal cardW = kCardW * scale;
  const qreal cardH = kCardH * scale;
  const qreal peek = kPeek * scale;
  const qreal arch = kArch * scale;
  const qreal lanePitch = kLanePitch * scale;
  const qreal margin = kMargin * scale;

  const int n = pins.size();
  const int hands = n == 0 ? 0 : (n + kPerHand - 1) / kPerHand;

  const int widest = n == 0 ? 1 : qMin(kPerHand, n);
  const qreal handWidth = cardW + qMax(0, widest - 1) * peek;
  const qreal extentW = handWidth + 2.0 * margin;
  const qreal extentH =
      cardH + arch + qMax(0, hands - 1) * lanePitch + 2.0 * margin;
  const qreal extent = qMax(extentW, extentH);

  const QPointF origin(extent / 2.0, extent - margin - cardH * 0.35);

  Decoration canvas;
  canvas.name = QStringLiteral("Canvas");
  canvas.kind = DecorationKind::Ellipse;
  canvas.bounds = QRectF(0, 0, extent, extent);
  canvas.styleKey = QStringLiteral("fan.canvas");
  scene.decorations.push_back(canvas);

  // No decorative pivot — it competed with the hands.

  int focusIdx = -1;
  for (int i = 0; i < n; ++i) {
    if (pins[i].id == focusedId) {
      focusIdx = i;
      break;
    }
  }

  for (int hand = 0; hand < hands; ++hand) {
    const int begin = hand * kPerHand;
    const int count = qMin(kPerHand, n - begin);
    if (count <= 0)
      break;

    // Higher hands sit above (stacked rows, like two suits in image 2).
    const qreal laneY = origin.y() - hand * lanePitch;
    const qreal span = qMax(0, count - 1) * peek;
    const qreal startX = origin.x() - span / 2.0;

    Decoration track;
    track.name = QStringLiteral("Hand");
    track.kind = DecorationKind::Ellipse;
    track.bounds = QRectF(startX - cardW * 0.2, laneY - arch - cardH * 0.55,
                          span + cardW * 1.4, cardH + arch + 24.0 * scale);
    track.styleKey = QStringLiteral("fan.track");
    scene.decorations.push_back(track);

    for (int i = 0; i < count; ++i) {
      const int pinIdx = begin + i;
      const qreal t = count == 1 ? 0.5 : qreal(i) / qreal(count - 1);
      // Mild arch so the hand reads as a fan, not a flat strip.
      const qreal archY = arch * (1.0 - 4.0 * (t - 0.5) * (t - 0.5));
      const qreal tiltDeg = -kMaxTiltDeg + t * (2.0 * kMaxTiltDeg);
      const qreal tiltRad = qDegreesToRadians(tiltDeg);
      const QPointF center(startX + i * peek, laneY - archY);

      PlacedItem item;
      item.id = pins[pinIdx].id;
      item.label = pins[pinIdx].label;
      item.bounds = QRectF(center.x() - cardW / 2.0, center.y() - cardH / 2.0,
                           cardW, cardH);
      item.angle = tiltRad;
      item.shape =
          rotatedCardPolygon(center, cardW * 1.02, cardH * 1.02, tiltRad);
      item.role = ItemRole::Item;
      // Within a hand: left→right stack (later on top).
      // Between hands: lower row sits in front of the upper row (hearts over spades).
      item.z = (hands - hand) * 1000 + i;
      if (pinIdx == focusIdx)
        item.z += 10000;
      scene.items.push_back(item);
    }
  }

  if (focusIdx >= 0) {
    scene.hub.focusedId = pins[focusIdx].id;
    scene.hub.selectedTitle = pins[focusIdx].label;
  }

  QVector<PlacedItem> ordered = scene.items;
  std::sort(ordered.begin(), ordered.end(),
            [](const PlacedItem &a, const PlacedItem &b) { return a.z > b.z; });
  for (const auto &item : ordered) {
    if (item.role == ItemRole::Item)
      scene.hitOrder.push_back(item.id);
  }
  return scene;
}

} // namespace locus
