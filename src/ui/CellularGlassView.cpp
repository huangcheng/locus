#include "ui/CellularGlassView.h"

#include "core/Hexagon.h"
#include "core/HitTest.h"
#include "ui/DockAnim.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace navi {
namespace {

constexpr qreal kMargin = 28.0;
constexpr qreal kIconSize = 40.0;
// Transparent padding around the grid so the focus glow and magnified
// cells are never clipped by the window edge.
constexpr qreal kPad = 40.0;

// Dock-style motion
constexpr qreal kHoverRadius = 160.0; // px; cursor influence range
constexpr qreal kHoverBoost = 0.35;   // max extra scale at the cursor
constexpr qreal kPressDepth = 0.12;   // click dip depth
constexpr qint64 kPressMs = 180;
constexpr qint64 kOpenMs = 240;
constexpr qint64 kStaggerMs = 26;
constexpr qreal kSettleEps = 0.002;

} // namespace

CellularGlassView::CellularGlassView(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  clock_.start();
}

void CellularGlassView::setScene(const SceneModel &scene) {
  QVector<QString> ids;
  ids.reserve(scene.items.size());
  for (const auto &item : scene.items)
    if (item.role == ItemRole::Item)
      ids.push_back(item.id);

  const bool samePins = ids == lastIds_;
  scene_ = scene;

  if (!samePins) {
    lastIds_ = ids;
    scales_.clear();
    shownAt_ = -1;

    // Open stagger runs outward from the grid center.
    const QPointF center = gridBounds().center();
    openOrder_ = ids;
    std::sort(openOrder_.begin(), openOrder_.end(),
              [&](const QString &a, const QString &b) {
                auto dist2 = [&](const QString &id) {
                  for (const auto &item : scene_.items) {
                    if (item.id == id) {
                      const QPointF d = item.bounds.center() - center;
                      return d.x() * d.x() + d.y() * d.y();
                    }
                  }
                  return 0.0;
                };
                return dist2(a) < dist2(b);
              });
  }
  updateGeometry();
  update();
}

void CellularGlassView::setAppearance(Appearance appearance) {
  appearance_ = appearance;
  update();
}

void CellularGlassView::setIcon(const QString &pinId, const QIcon &icon) {
  // Render at 4x so hover magnification downscales (crisp) instead of
  // upscaling a 40px raster (blurry).
  pixmaps_.insert(pinId, icon.pixmap(QSize(160, 160)));
  update();
}

void CellularGlassView::playOpenAnimation() {
  if (openOrder_.isEmpty())
    return;
  shownAt_ = clock_.elapsed();
  if (!animTimer_.isActive())
    animTimer_.start(16, this);
  update();
}

QRectF CellularGlassView::gridBounds() const {
  QRectF g;
  for (const auto &dec : scene_.decorations) {
    // Empty "+" slots are hidden, so they don't count toward the window size.
    if (dec.styleKey == QLatin1String("cell.empty"))
      continue;
    g = g.united(dec.bounds);
  }
  return g;
}

QSize CellularGlassView::sizeHint() const {
  const QRectF g = gridBounds();
  const qreal w = qMax<qreal>(g.width(), 200) + 2 * (kMargin + kPad);
  const qreal h = qMax<qreal>(g.height(), 200) + 2 * (kMargin + kPad);
  return QSize(int(std::ceil(w)), int(std::ceil(h)));
}

qreal CellularGlassView::cellScale(const QString &id, qint64 now) const {
  qreal s = scales_.value(id, 1.0);
  if (shownAt_ >= 0) {
    const int idx = qMax(0, int(openOrder_.indexOf(id)));
    const qreal t = qreal(now - shownAt_ - idx * kStaggerMs) / qreal(kOpenMs);
    s *= dockanim::easeOutBack(qBound(0.0, t, 1.0));
  }
  if (id == pressedId_ && pressedAt_ >= 0) {
    const qreal t = qreal(now - pressedAt_) / qreal(kPressMs);
    s *= dockanim::pressDip(qBound(0.0, t, 1.0), kPressDepth);
  }
  return s;
}

void CellularGlassView::updateScales() {
  const qint64 now = clock_.elapsed();
  bool animating = false;
  bool dirty = false;

  for (const auto &item : scene_.items) {
    if (item.role != ItemRole::Item)
      continue;
    qreal target = 1.0;
    if (cursorInside_) {
      const QPointF d = cursorPos_ - item.bounds.center();
      target += kHoverBoost * dockanim::falloff(std::hypot(d.x(), d.y()),
                                                kHoverRadius);
    }
    const qreal cur = scales_.value(item.id, 1.0);
    const qreal next = cur + (target - cur) * 0.18;
    if (qAbs(next - target) > kSettleEps) {
      scales_[item.id] = next;
      animating = true;
      dirty = true;
    } else if (cur != target) {
      scales_[item.id] = target;
      dirty = true;
    }
  }

  if (shownAt_ >= 0) {
    const qint64 openEnd =
        shownAt_ + qint64(openOrder_.size()) * kStaggerMs + kOpenMs;
    if (now < openEnd) {
      animating = true;
      dirty = true;
    } else {
      shownAt_ = -1;
    }
  }
  if (pressedAt_ >= 0) {
    if (now - pressedAt_ < kPressMs) {
      animating = true;
      dirty = true;
    } else {
      pressedAt_ = -1;
      pressedId_.clear();
    }
  }

  if (dirty)
    update();
  if (!animating)
    animTimer_.stop();
}

void CellularGlassView::timerEvent(QTimerEvent *event) {
  if (event->timerId() == animTimer_.timerId())
    updateScales();
  else
    QWidget::timerEvent(event);
}

void CellularGlassView::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  p.setRenderHint(QPainter::TextAntialiasing, true);
  const bool dark = appearance_ != Appearance::Light;
  const qint64 now = clock_.elapsed();

  p.save();
  p.translate(kPad, kPad); // scene → view coords; padding prevents clipping

  // Amber glow behind the focused cell
  for (const auto &dec : scene_.decorations) {
    if (dec.kind != DecorationKind::Path ||
        dec.styleKey != QLatin1String("cell.focused"))
      continue;
    QRadialGradient glow(dec.bounds.center(), 95);
    glow.setColorAt(0.0, QColor(245, 200, 110, dark ? 82 : 115));
    glow.setColorAt(1.0, QColor(245, 200, 110, 0));
    p.fillRect(dec.bounds.adjusted(-60, -60, 60, 60), glow);
  }

  // Pair each cell decoration with its item and animated scale
  struct Cell {
    const Decoration *dec;
    const PlacedItem *item; // nullptr for empty slots
    qreal scale;
  };
  QVector<Cell> cells;
  for (const auto &dec : scene_.decorations) {
    if (dec.kind != DecorationKind::Path)
      continue;
    Cell cell{&dec, nullptr, 1.0};
    for (const auto &item : scene_.items) {
      if (item.role == ItemRole::Item && item.bounds == dec.bounds) {
        cell.item = &item;
        cell.scale = cellScale(item.id, now);
        break;
      }
    }
    cells.push_back(cell);
  }
  // Largest on top, like the Dock
  std::stable_sort(cells.begin(), cells.end(),
                   [](const Cell &a, const Cell &b) { return a.scale < b.scale; });

  for (const auto &cell : cells) {
    const QRectF r = cell.dec->bounds;
    p.save();
    if (cell.scale != 1.0) {
      const QPointF c = r.center();
      p.translate(c);
      p.scale(cell.scale, cell.scale);
      p.translate(-c);
    }

    QPainterPath hex;
    hex.addPolygon(hexagonForRect(r));
    hex.closeSubpath();

    const bool isEmpty = cell.dec->styleKey == QLatin1String("cell.empty");
    if (!isEmpty) {
      // Fake soft shadow (QPainter has no blur): two offset low-alpha hexes.
      for (int i = 2; i >= 1; --i) {
        QPainterPath shadowPath;
        shadowPath.addPolygon(hexagonForRect(r.translated(0, i * 2.5)));
        shadowPath.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(dark ? QColor(0, 0, 0, 16 * i) : QColor(42, 39, 31, 7 * i));
        p.drawPath(shadowPath);
      }
    }

    if (cell.dec->styleKey == QLatin1String("cell.focused")) {
      QLinearGradient fill(r.topLeft(), r.bottomRight());
      fill.setColorAt(0.0, QColor(248, 221, 156)); // #F8DD9C
      fill.setColorAt(1.0, QColor(238, 200, 109)); // #EEC86D
      p.setPen(QPen(QColor(180, 130, 40, 110), 1));
      p.setBrush(fill);
      p.drawPath(hex);
    } else if (isEmpty) {
      // Hidden: empty "+" slots stay in the scene model (layout + tests
      // depend on them) but are not painted. Restore this branch to bring
      // the dashed add-cells back:
      //
      //   QPen pen(dark ? QColor(255, 255, 255, 72) : QColor(42, 39, 31, 64), 1.2);
      //   pen.setDashPattern({5, 6});
      //   p.setPen(pen);
      //   p.setBrush(Qt::NoBrush);
      //   p.drawPath(hex);
      //   p.setPen(dark ? QColor(255, 255, 255, 90) : QColor(42, 39, 31, 90));
      //   QFont f = font();
      //   f.setPixelSize(18);
      //   p.setFont(f);
      //   p.drawText(r, Qt::AlignCenter, QStringLiteral("+"));
    } else {
      QLinearGradient fill(r.topLeft(), r.bottomRight());
      if (dark) {
        fill.setColorAt(0.0, QColor(52, 49, 63));
        fill.setColorAt(1.0, QColor(38, 35, 46));
      } else {
        fill.setColorAt(0.0, QColor(255, 255, 255));
        fill.setColorAt(1.0, QColor(242, 239, 231));
      }
      p.setPen(QPen(dark ? QColor(255, 255, 255, 26) : QColor(42, 39, 31, 36),
                    1));
      p.setBrush(fill);
      p.drawPath(hex);
    }

    if (cell.item) {
      const QRectF ir(r.center().x() - kIconSize / 2.0,
                      r.center().y() - kIconSize / 2.0, kIconSize, kIconSize);
      const QPixmap pm = pixmaps_.value(cell.item->id);
      if (!pm.isNull()) {
        p.drawPixmap(ir, pm, pm.rect());
      } else {
        p.setPen(dark ? QColor(255, 255, 255, 180) : QColor(40, 40, 50));
        QFont f = font();
        f.setPixelSize(14);
        f.setBold(true);
        p.setFont(f);
        p.drawText(ir, Qt::AlignCenter, cell.item->id.left(2).toUpper());
      }
    }
    p.restore();
  }

  p.restore();
}

void CellularGlassView::mouseMoveEvent(QMouseEvent *event) {
  const QPointF scenePos = event->position() - QPointF(kPad, kPad);
  cursorPos_ = scenePos;
  cursorInside_ = true;
  if (!animTimer_.isActive())
    animTimer_.start(16, this);
  const QString id = hitTest(scene_, scenePos);
  if (!id.isEmpty() && id != lastHover_) {
    lastHover_ = id;
    emit itemHovered(id);
  }
}

void CellularGlassView::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;
  const QString id =
      hitTest(scene_, event->position() - QPointF(kPad, kPad));
  if (!id.isEmpty()) {
    pressedId_ = id;
    pressedAt_ = clock_.elapsed();
    if (!animTimer_.isActive())
      animTimer_.start(16, this);
    emit itemActivated(id);
  } else {
    emit dismissRequested();
  }
}

void CellularGlassView::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape)
    emit dismissRequested();
  else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
           !lastHover_.isEmpty())
    emit itemActivated(lastHover_);
  else
    QWidget::keyPressEvent(event);
}

void CellularGlassView::leaveEvent(QEvent *) {
  // Sticky selection: keep lastHover_ so ⏎ still activates the focused cell
  // and the amber cell / label stay put. Only the magnification relaxes.
  cursorInside_ = false;
  if (!animTimer_.isActive())
    animTimer_.start(16, this);
}

} // namespace navi
