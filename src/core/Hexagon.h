#pragma once

#include <QPolygonF>
#include <QRectF>

namespace navi {

/// Pointy-top hexagon inscribed in rect (vertices at top/bottom midpoints
/// and at ±25%/75% height on the left/right edges).
inline QPolygonF hexagonForRect(const QRectF &r) {
  const qreal cx = r.center().x();
  return QPolygonF{QPointF(cx, r.top()),
                   QPointF(r.right(), r.top() + r.height() * 0.25),
                   QPointF(r.right(), r.top() + r.height() * 0.75),
                   QPointF(cx, r.bottom()),
                   QPointF(r.left(), r.top() + r.height() * 0.75),
                   QPointF(r.left(), r.top() + r.height() * 0.25)};
}

} // namespace navi
