#pragma once

#include "core/Pin.h"
#include "core/SceneModel.h"

namespace navi {

struct DensityPrefs {
  qreal minIconSize = 34;
  int maxPerRing = 12;
  qreal gap = 12;
  qreal widgetSize = 560;
};

class LayoutStrategy {
public:
  virtual ~LayoutStrategy() = default;
  virtual SceneModel build(const QVector<Pin> &pins, const QString &focusedId,
                           const DensityPrefs &density) = 0;
};

} // namespace navi
