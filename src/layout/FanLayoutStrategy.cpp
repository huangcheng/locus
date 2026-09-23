#include "layout/FanLayoutStrategy.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace locus {
namespace {

// A real hand of cards: every card in a row rotates around a shared pivot
// below the row, so the tops spread and the bottoms converge. Rows overlap
// deeply — the front hand covers the back hand except its top strip (the
// rank pips), like two suits held together. All metrics derive from the
// shared density sliders; defaults (cellSize 80, cellGap 8) reproduce the
// original 84×110 card / 22 peek geometry.
constexpr int kPerHand = 12;

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

  const qreal cardW = density.cellSize * 1.05;  // 84 @80
  const qreal cardH = density.cellSize * 1.22;  // 98 @80: tall enough to read
                                                // as a card, short enough to
                                                // stay dense with icons only
  const qreal peek = density.cellGap * 2.75;    // 22 @8: visible top strip
  const qreal margin = density.cellSize * 0.8;  // 64 @80
  const qreal radius = cardH * 1.35;            // pivot → card center
  // Angular step sized so the visible strip at the top edge equals peek.
  const qreal step = peek / (radius + cardH / 2.0);
  // Front hand covers the back hand's lower half — deep enough to read as
  // stacked hands, shallow enough that each back-row card keeps a usable
  // hit strip.
  const qreal rowPitch = cardH * 0.52;

  const int n = pins.size();
  const int hands = n == 0 ? 0 : (n + kPerHand - 1) / kPerHand;

  int focusIdx = -1;
  for (int i = 0; i < n; ++i) {
    if (pins[i].id == focusedId) {
      focusIdx = i;
      break;
    }
  }

  // Lay out in pivot-local space (hand 0 pivot at the origin), then measure
  // and translate into the canvas — no analytic extent guesswork.
  QVector<QRectF> handBounds;
  for (int hand = 0; hand < hands; ++hand) {
    const int begin = hand * kPerHand;
    const int count = qMin(kPerHand, n - begin);
    if (count <= 0)
      break;

    const QPointF pivot(0, -hand * rowPitch);
    QRectF handUnion;

    for (int i = 0; i < count; ++i) {
      const int pinIdx = begin + i;
      const qreal theta = (i - (count - 1) / 2.0) * step;
      const QPointF center(pivot.x() + radius * std::sin(theta),
                           pivot.y() - radius * std::cos(theta));

      PlacedItem item;
      item.id = pins[pinIdx].id;
      item.label = pins[pinIdx].label;
      item.bounds = QRectF(center.x() - cardW / 2.0, center.y() - cardH / 2.0,
                           cardW, cardH);
      item.angle = theta;
      item.shape =
          rotatedCardPolygon(center, cardW * 1.02, cardH * 1.02, theta);
      item.role = ItemRole::Item;
      // Within a hand: left→right stack (later on top).
      // Between hands: the lower (front) hand paints over the upper one.
      item.z = (hands - hand) * 1000 + i;
      if (pinIdx == focusIdx)
        item.z += 10000;
      handUnion |= item.shape->boundingRect();
      scene.items.push_back(item);
    }
    handBounds.push_back(handUnion);
  }

  QRectF content;
  for (const QRectF &r : handBounds)
    content |= r;
  const qreal extent =
      qCeil(qMax(content.width(), content.height()) + 2.0 * margin);
  const QPointF shift((extent - content.width()) / 2.0 - content.x(),
                      (extent - content.height()) / 2.0 - content.y());

  for (auto &item : scene.items) {
    item.bounds.translate(shift);
    if (item.shape)
      item.shape = item.shape->translated(shift);
  }

  Decoration canvas;
  canvas.name = QStringLiteral("Canvas");
  canvas.kind = DecorationKind::Ellipse;
  canvas.bounds = QRectF(0, 0, extent, extent);
  canvas.styleKey = QStringLiteral("fan.canvas");
  scene.decorations.push_back(canvas);

  for (int hand = 0; hand < handBounds.size(); ++hand) {
    Decoration track;
    track.name = QStringLiteral("Hand");
    track.kind = DecorationKind::Ellipse;
    track.bounds = handBounds[hand].translated(shift);
    track.styleKey = QStringLiteral("fan.track");
    scene.decorations.push_back(track);
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
