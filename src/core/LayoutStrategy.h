#pragma once

#include "core/Pin.h"
#include "core/SceneModel.h"

namespace locus {

struct DensityPrefs {
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
