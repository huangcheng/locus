#include "layout/OrbitalLayoutStrategy.h"

#include <QtMath>

#include <algorithm>

namespace locus {
namespace {

QRectF chipBounds(QPointF center, qreal size) {
  return QRectF(center.x() - size / 2.0, center.y() - size / 2.0, size, size);
}

} // namespace

SceneModel OrbitalLayoutStrategy::build(const QVector<Pin> &pins,
                                        const QString &focusedId,
                                        const DensityPrefs &) {
  SceneModel scene;

  struct Ring {
    qreal radius;
    int cap;
    qreal chip;
  };
  QVector<Ring> rings;
  int left = pins.size();
  for (int i = 0; left > 0; ++i) {
    const Ring ring{88.0 + 64.0 * i, 6 + 2 * i, i == 0 ? 44.0 : 48.0};
    rings.push_back(ring);
    left -= ring.cap;
  }

  constexpr qreal kHubRadius = 44.0;
  const qreal discRadius =
      rings.isEmpty()
          ? kHubRadius + 24.0
          : rings.last().radius + rings.last().chip / 2.0 + 10.0;
  constexpr qreal kMargin = 32.0; // room for glow/shadow around the disc
  const qreal size = (discRadius + kMargin) * 2.0;
  const QPointF origin(size / 2.0, size / 2.0);

  Decoration disc;
  disc.name = QStringLiteral("Glass Disc");
  disc.kind = DecorationKind::Ellipse;
  disc.bounds = QRectF(origin.x() - discRadius, origin.y() - discRadius,
                       discRadius * 2.0, discRadius * 2.0);
  disc.styleKey = QStringLiteral("orbit.disc");
  scene.decorations.push_back(disc);

  int begin = 0;
  int baseZ = 10;
  for (const auto &ring : rings) {
    const int count = qMin(ring.cap, pins.size() - begin);
    if (count <= 0)
      break;
    Decoration track;
    track.name = QStringLiteral("Track");
    track.kind = DecorationKind::Ellipse;
    track.bounds = QRectF(origin.x() - ring.radius, origin.y() - ring.radius,
                          ring.radius * 2.0, ring.radius * 2.0);
    track.styleKey = QStringLiteral("orbit.track");
    scene.decorations.push_back(track);

    for (int i = 0; i < count; ++i) {
      const qreal angle = -M_PI / 2.0 + (2.0 * M_PI * i) / count;
      const QPointF c(origin.x() + ring.radius * qCos(angle),
                      origin.y() + ring.radius * qSin(angle));
      PlacedItem item;
      item.id = pins[begin + i].id;
      item.bounds = chipBounds(c, ring.chip);
      item.z = baseZ + i;
      item.role = ItemRole::Item;
      item.angle = angle;
      scene.items.push_back(item);
    }
    begin += count;
    baseZ += 100;
  }

  for (const auto &pin : pins) {
    if (pin.id == focusedId) {
      scene.hub.selectedTitle = pin.label;
      break;
    }
  }
  scene.hub.focusedId = focusedId;

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
