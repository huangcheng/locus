#pragma once

#include "core/LayoutStrategy.h"

namespace locus {

/// Orbit v2: pins packed onto concentric tracks around a center hub. The
/// inner track holds 6 chips, each next track 2 more (8, 10, …) at a radius
/// step of 64 — the widget grows a ring instead of paginating.
class OrbitalLayoutStrategy : public LayoutStrategy {
public:
  SceneModel build(const QVector<Pin> &pins, const QString &focusedId,
                   const DensityPrefs &density) override;
};

} // namespace locus
