#include "ui/HoneycombMark.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace locus {

void paintHoneycombMark(QPainter &p, const QPointF &center, qreal r,
                        bool dark) {
  const qreal sx = 1.732 * r + 1.6; // same-row hex spacing
  const qreal sy = 1.5 * r + 1.2;   // row spacing
  const auto hexPath = [&](const QPointF &c) {
    QPainterPath path;
    for (int i = 0; i < 6; ++i) {
      const qreal a = qDegreesToRadians(90.0 + i * 60.0);
      const QPointF pt(c.x() + r * qCos(a), c.y() + r * qSin(a));
      if (i == 0)
        path.moveTo(pt);
      else
        path.lineTo(pt);
    }
    path.closeSubpath();
    return path;
  };
  p.setPen(Qt::NoPen);
  p.setBrush(dark ? QColor(255, 255, 255, 225) : QColor(38, 38, 42));
  const QPointF ring[6] = {QPointF(-sx, 0.0), QPointF(sx, 0.0),
                           QPointF(-sx / 2.0, -sy), QPointF(sx / 2.0, -sy),
                           QPointF(-sx / 2.0, sy), QPointF(sx / 2.0, sy)};
  for (const QPointF &off : ring)
    p.drawPath(hexPath(center + off));
  QLinearGradient amber(center + QPointF(0, -r), center + QPointF(0, r));
  amber.setColorAt(0.0, QColor(247, 176, 60));
  amber.setColorAt(1.0, QColor(230, 130, 15));
  p.setBrush(amber);
  p.drawPath(hexPath(center));
}

} // namespace locus
