#pragma once

#include <QtGlobal>

class QPainter;
class QPointF;

namespace locus {

/// The Locus honeycomb mark: pointy-top hexes in a 2-3-2 cluster with an
/// amber gradient center — the same shape as the tray icon, drawn
/// vector-crisp at any size/DPI. `r` is the center-hex radius; the mark
/// spans about 2*(1.732r + 1.6 + r) wide and 2*(1.5r + 1.2 + r) tall.
void paintHoneycombMark(QPainter &p, const QPointF &center, qreal r,
                        bool dark);

} // namespace locus
