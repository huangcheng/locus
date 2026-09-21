#include "layout/OrbitalLayoutStrategy.h"

#include <QtMath>

#include <algorithm>

namespace navi {
namespace {

QRectF iconBounds(QPointF center, qreal size) {
  return QRectF(center.x() - size / 2.0, center.y() - size / 2.0, size, size);
}

void placeRing(SceneModel &scene, const QVector<Pin> &pins, int begin, int end,
               qreal radius, qreal iconSize, qreal rotation, QPointF origin,
               int baseZ) {
  const int count = end - begin;
  if (count <= 0)
    return;
  for (int i = 0; i < count; ++i) {
    const qreal angle = -M_PI / 2.0 + rotation + (2.0 * M_PI * i) / count;
    const QPointF c(origin.x() + radius * qCos(angle),
                    origin.y() + radius * qSin(angle));
    PlacedItem item;
    item.id = pins[begin + i].id;
    item.bounds = iconBounds(c, iconSize);
    item.z = baseZ + i;
    item.role = ItemRole::Item;
    item.angle = angle;
    scene.items.push_back(item);
  }
}

} // namespace

SceneModel OrbitalLayoutStrategy::build(const QVector<Pin> &pins,
                                        const QString &focusedId,
                                        const DensityPrefs &density) {
  SceneModel scene;
  const QPointF origin(density.widgetSize / 2.0, density.widgetSize / 2.0);

  const int innerCap = qMin(8, density.maxPerRing);
  const int innerCount = qMin(pins.size(), innerCap);
  const int outerCount = qMax(0, pins.size() - innerCount);

  constexpr qreal kInnerRadius = 136.0;
  constexpr qreal kOuterRadius = 239.0;
  const qreal innerIcon = qMax(density.minIconSize, 48.0);
  const qreal outerIcon = density.minIconSize;

  if (innerCount > 0) {
    Decoration innerOrb;
    innerOrb.name = QStringLiteral("Inner Orbital");
    innerOrb.kind = DecorationKind::Ellipse;
    innerOrb.bounds = QRectF(origin.x() - kInnerRadius, origin.y() - kInnerRadius,
                             kInnerRadius * 2, kInnerRadius * 2);
    innerOrb.styleKey = QStringLiteral("orbital.inner");
    scene.decorations.push_back(innerOrb);
  }

  if (outerCount > 0) {
    Decoration outerOrb;
    outerOrb.name = QStringLiteral("Outer Orbital");
    outerOrb.kind = DecorationKind::Ellipse;
    outerOrb.bounds = QRectF(origin.x() - kOuterRadius, origin.y() - kOuterRadius,
                             kOuterRadius * 2, kOuterRadius * 2);
    outerOrb.styleKey = QStringLiteral("orbital.outer");
    scene.decorations.push_back(outerOrb);
  }

  placeRing(scene, pins, 0, innerCount, kInnerRadius, innerIcon,
            rotationRadians_, origin, 10);
  placeRing(scene, pins, innerCount, innerCount + outerCount, kOuterRadius,
            outerIcon, rotationRadians_, origin, 20);

  QString focusedLabel;
  for (const auto &pin : pins) {
    if (pin.id == focusedId) {
      focusedLabel = pin.label;
      break;
    }
  }
  scene.hub.selectedTitle = focusedLabel;
  scene.hub.brandSubtitle = QStringLiteral("NAVI");

  if (!focusedId.isEmpty()) {
    for (const auto &item : scene.items) {
      if (item.role == ItemRole::Item && item.id == focusedId) {
        Decoration well;
        well.name = QStringLiteral("Hover Well");
        well.kind = DecorationKind::Ellipse;
        well.bounds = item.bounds.adjusted(-12, -12, 12, 12);
        well.styleKey = QStringLiteral("hover.well");
        scene.decorations.push_back(well);
        break;
      }
    }
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

} // namespace navi
