#include "layout/CellularLayoutStrategy.h"

#include "core/Hexagon.h"

#include <algorithm>

namespace locus {
namespace {

constexpr qreal kMargin = 28.0;

// All metrics derive from cellSize so the grid scales uniformly; defaults
// (80, gap 8) reproduce the original 80x92 / 88 / 77 geometry exactly.
struct GridGeo {
  qreal cellW;
  qreal cellH;
  qreal pitchX;
  qreal pitchY;
  qreal gridW; // widest row (4 cells)
};

GridGeo gridGeoFor(const DensityPrefs &d) {
  GridGeo g;
  g.cellW = d.cellSize;
  g.cellH = d.cellSize * 1.15;
  g.pitchX = d.cellSize + d.cellGap;
  g.pitchY = d.cellSize * 0.9625;
  g.gridW = 3.0 * g.pitchX + g.cellW;
  return g;
}

int rowCapacity(int row) { return row % 2 == 0 ? 3 : 4; }

int rowCountFor(int pinCount) {
  // At least one row so the empty state still shows slots.
  int rows = 0;
  int remaining = pinCount;
  do {
    remaining -= rowCapacity(rows);
    ++rows;
  } while (remaining > 0);
  return rows;
}

QRectF cellRect(const GridGeo &g, int row, int col) {
  const int n = rowCapacity(row);
  const qreal rowW = (n - 1) * g.pitchX + g.cellW;
  const qreal x0 = kMargin + (g.gridW - rowW) / 2.0;
  return QRectF(x0 + col * g.pitchX, kMargin + row * g.pitchY, g.cellW,
                g.cellH);
}

} // namespace

SceneModel CellularLayoutStrategy::build(const QVector<Pin> &pins,
                                         const QString &focusedId,
                                         const DensityPrefs &density) {
  const GridGeo geo = gridGeoFor(density);
  SceneModel scene;
  const int rows = rowCountFor(pins.size());

  int index = 0;
  for (int r = 0; r < rows; ++r) {
    const int cap = rowCapacity(r);
    for (int c = 0; c < cap; ++c) {
      const QRectF rect = cellRect(geo, r, c);
      Decoration cell;
      cell.kind = DecorationKind::Path;
      cell.bounds = rect;
      if (index < pins.size()) {
        const Pin &pin = pins[index];
        const bool focused = pin.id == focusedId;
        cell.name = QStringLiteral("cell.%1").arg(pin.id);
        cell.styleKey = focused ? QStringLiteral("cell.focused")
                                : QStringLiteral("cell");

        PlacedItem item;
        item.id = pin.id;
        item.bounds = rect;
        item.z = r * 10 + c;
        item.role = ItemRole::Item;
        item.shape = hexagonForRect(rect);
        scene.items.push_back(item);
        ++index;
      } else {
        cell.name = QStringLiteral("cell.empty.%1.%2").arg(r).arg(c);
        cell.styleKey = QStringLiteral("cell.empty");
      }
      scene.decorations.push_back(cell);
    }
  }

  for (const auto &pin : pins) {
    if (pin.id == focusedId) {
      scene.hub.selectedTitle = pin.label;
      break;
    }
  }
  scene.hub.brandSubtitle = QStringLiteral("LOCUS");
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
