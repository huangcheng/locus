#pragma once

#include "core/LayoutStrategy.h"

namespace locus {

class OrbitalLayoutStrategy : public LayoutStrategy {
public:
  void setRotationRadians(qreal r) { rotationRadians_ = r; }
  qreal rotationRadians() const { return rotationRadians_; }

  SceneModel build(const QVector<Pin> &pins, const QString &focusedId,
                   const DensityPrefs &density) override;

private:
  qreal rotationRadians_ = 0;
};

} // namespace locus
