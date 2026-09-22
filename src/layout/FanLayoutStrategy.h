#pragma once

#include "core/LayoutStrategy.h"

namespace locus {

/// Fan v2: pins on an upper arc around a bottom pivot; each item carries a
/// tilt angle and a rotated hit polygon.
class FanLayoutStrategy : public LayoutStrategy {
public:
  SceneModel build(const QVector<Pin> &pins, const QString &focusedId,
                   const DensityPrefs &density) override;
};

} // namespace locus
