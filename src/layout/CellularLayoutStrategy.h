#pragma once

#include "core/LayoutStrategy.h"

namespace navi {

/// Honeycomb widget: pins fill interlocked pointy-top hex rows (3,4,3,4…),
/// trailing slots in the last row render as dashed empty cells.
class CellularLayoutStrategy : public LayoutStrategy {
public:
  SceneModel build(const QVector<Pin> &pins, const QString &focusedId,
                   const DensityPrefs &density) override;
};

} // namespace navi
