#pragma once

#include "core/Pin.h"
#include "core/SceneModel.h"

namespace locus {

struct DensityPrefs {
  qreal minIconSize = 34;
  int maxPerRing = 12;
  qreal gap = 12;
  qreal widgetSize = 560;
  // Honeycomb grid: cell height = cellSize * 1.15, vertical pitch keeps the
  // 77/80 interlock ratio; cellGap is the horizontal pitch offset.
  qreal cellSize = 80;
  qreal cellGap = 8;
  qreal iconSize = 40;
};

class LayoutStrategy {
public:
  virtual ~LayoutStrategy() = default;
  virtual SceneModel build(const QVector<Pin> &pins, const QString &focusedId,
                           const DensityPrefs &density) = 0;
};

} // namespace locus
