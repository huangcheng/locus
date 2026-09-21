# Honeycomb ("Cellular") Widget Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the approved honeycomb design the default Navi launcher widget: floating pointy-top hexagonal cells (no window chrome), amber focused cell, selection pill — replacing Orbital as the default style while keeping Orbital available via the existing `style` pref.

**Architecture:** The codebase already has the seam (`docs/superpowers/specs/2026-09-20-navi-architecture-design.md`): `LayoutStrategy::build(pins, focusedId, density) -> SceneModel` and `MenuView::setScene`. We add `CellularLayoutStrategy` (layout math + cell chrome as `Decoration`s) and `CellularGlassView` (QPainter rendering + input), extend `SceneModel`/`HitTest` with polygon shapes (hex rows overlap by 15px vertically, so rect-only hit-testing misfires in overlap bands), and wire a style switch in `main.cpp`.

**Design source of truth:** Ardot file 727830845408975 frames "01b — Honeycomb · Dark" / "01c — Honeycomb · Light" / "02 — Empty Widget", and `mockups/launcher-directions.html` section C.

**Tech Stack:** Qt 6.11 Widgets (QPainter), C++20, CMake, Qt Test. Build: `cmake --build build2 -j`, test: `ctest --test-dir build2 --output-on-failure`.

**NOTE:** This repo is NOT git-initialized — skip all commit steps; a task is done when its tests pass.

**Geometry contract (do not improvise):**
- Cell rect: 80 × 92 (pointy-top hexagon inside)
- Pitch: horizontal 88, vertical 77 (rows interlock; bounding boxes overlap 15px vertically)
- Row capacities alternate 3, 4, 3, 4… starting with 3. Rows are centered; grid width is 344 (4-cell row: 3·88+80). A 3-cell row starts 44px to the right of a 4-cell row.
- Top-left margin of the grid: (28, 28)
- Pins fill rows left→right, top→bottom; remaining slots in the last row render as dashed empty "＋" cells
- Icon: 40×40 centered in the cell
- Focused cell: amber gradient #F8DD9C → #EEC86D + soft radial glow
- Focus label below grid: focused app name, 11px bold, centered plain text — no pill/chip chrome

---

### Task 1: Shape-aware hit testing

Hex rows interlock, so bounding boxes overlap. A point can sit inside a higher-z cell's rect but outside its hexagon; the hit must fall through to the lower cell. This task adds an optional polygon `shape` to `PlacedItem` and makes `hitTest` prefer it.

**Files:**
- Create: `src/core/Hexagon.h`
- Modify: `src/core/SceneModel.h`
- Modify: `src/core/HitTest.h`
- Test: `tests/test_hexhittest.cpp` (new)
- Modify: `CMakeLists.txt` (register the test)

- [ ] **Step 1: Write the failing test** — create `tests/test_hexhittest.cpp`:

```cpp
#include "core/Hexagon.h"
#include "core/HitTest.h"

#include <QtTest>

namespace {

navi::PlacedItem hexItem(const QString &id, const QRectF &rect, int z) {
  navi::PlacedItem it;
  it.id = id;
  it.bounds = rect;
  it.z = z;
  it.role = navi::ItemRole::Item;
  it.shape = navi::hexagonForRect(rect);
  return it;
}

} // namespace

class HexHitTest : public QObject {
  Q_OBJECT
private slots:
  void shapeBeatsRectOverlap() {
    // Two interlocked pointy-top hex cells: "a" is row0 col0 (lower z),
    // "b" is row1 col1 (higher z, tested first). (120,112) lies inside
    // b's bounding rect but outside b's hexagon — inside a's hexagon only.
    navi::SceneModel scene;
    scene.items = {hexItem(QStringLiteral("a"), QRectF(72, 28, 80, 92), 0),
                   hexItem(QStringLiteral("b"), QRectF(116, 105, 80, 92), 10)};
    scene.hitOrder = {QStringLiteral("b"), QStringLiteral("a")};
    QCOMPARE(navi::hitTest(scene, QPointF(120, 112)), QStringLiteral("a"));
    QCOMPARE(navi::hitTest(scene, QPointF(156, 151)), QStringLiteral("b"));
    QCOMPARE(navi::hitTest(scene, QPointF(10, 10)), QString());
  }

  void rectFallbackWithoutShape() {
    navi::PlacedItem it;
    it.id = QStringLiteral("r");
    it.bounds = QRectF(0, 0, 50, 50);
    it.z = 0;
    it.role = navi::ItemRole::Item;
    navi::SceneModel scene;
    scene.items = {it};
    scene.hitOrder = {QStringLiteral("r")};
    QCOMPARE(navi::hitTest(scene, QPointF(25, 25)), QStringLiteral("r"));
    QCOMPARE(navi::hitTest(scene, QPointF(60, 60)), QString());
  }
};

QTEST_MAIN(HexHitTest)
#include "test_hexhittest.moc"
```

Register in `CMakeLists.txt` after the existing `navi_add_test(...)` lines (line 65 area):

```cmake
navi_add_test(test_hexhittest tests/test_hexhittest.cpp)
```

- [ ] **Step 2: Run to verify it fails**

```bash
cmake -S . -B build2 -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" && cmake --build build2 -j 2>&1 | tail -20
```

Expected: compile error — `PlacedItem has no member named 'shape'` / `hexagonForRect` undefined.

- [ ] **Step 3: Implement**

Create `src/core/Hexagon.h`:

```cpp
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
```

Modify `src/core/SceneModel.h` — add the include, the `shape` field on `PlacedItem`, and `focusedId` on `HubInfo`:

```cpp
#pragma once

#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>

#include <optional>

namespace navi {

enum class ItemRole { Item, Hub, Decoration };

enum class DecorationKind { Ellipse, Path };

struct PlacedItem {
  QString id;
  QRectF bounds;
  int z = 0;
  ItemRole role = ItemRole::Item;
  std::optional<qreal> angle;
  // Optional absolute-coords polygon; when present it wins over bounds in hit-testing.
  std::optional<QPolygonF> shape;
};

struct Decoration {
  QString name;
  DecorationKind kind = DecorationKind::Ellipse;
  QRectF bounds;
  QString styleKey;
};

struct HubInfo {
  QString selectedTitle;
  QString brandSubtitle = QStringLiteral("NAVI");
  QString focusedId;
};

struct SceneModel {
  QVector<PlacedItem> items;
  HubInfo hub;
  QVector<Decoration> decorations;
  QVector<QString> hitOrder;
};

} // namespace navi
```

Replace the body of `hitTest` in `src/core/HitTest.h`:

```cpp
#pragma once

#include "core/SceneModel.h"

#include <QPointF>
#include <QString>

namespace navi {

/// First Item in hitOrder whose shape (or bounds) contains point; else empty.
inline QString hitTest(const SceneModel &scene, QPointF p) {
  for (const QString &id : scene.hitOrder) {
    for (const auto &it : scene.items) {
      if (it.id != id || it.role != ItemRole::Item)
        continue;
      if (it.shape && !it.shape->isEmpty()) {
        if (it.shape->containsPoint(p, Qt::OddEvenFill))
          return id;
      } else if (it.bounds.contains(p)) {
        return id;
      }
    }
  }
  return {};
}

} // namespace navi
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
cmake --build build2 -j && ctest --test-dir build2 --output-on-failure
```

Expected: all 6 tests pass (the new `test_hexhittest` plus the existing 5 — `test_scene_hittest` still passes because rect-only items keep rect behavior).

---

### Task 2: CellularLayoutStrategy

Turns N pins into interlocked honeycomb rows: one `PlacedItem` (with hexagon `shape`) per pin, one `Decoration` per cell (`cell` / `cell.focused` / `cell.empty`), hub info for the pill, and z-descending `hitOrder`.

**Files:**
- Create: `src/layout/CellularLayoutStrategy.h`
- Create: `src/layout/CellularLayoutStrategy.cpp`
- Test: `tests/test_cellular_layout.cpp` (new)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test** — create `tests/test_cellular_layout.cpp`:

```cpp
#include "layout/CellularLayoutStrategy.h"

#include <QtTest>

namespace {

QVector<navi::Pin> makePins(int n) {
  QVector<navi::Pin> pins;
  pins.reserve(n);
  for (int i = 0; i < n; ++i) {
    navi::Pin p;
    p.id = QStringLiteral("p%1").arg(i);
    p.label = QStringLiteral("App %1").arg(i);
    p.appPath = QStringLiteral("/tmp/app%1.app").arg(i);
    pins.push_back(p);
  }
  return pins;
}

int countStyle(const navi::SceneModel &scene, const char *key) {
  int n = 0;
  for (const auto &dec : scene.decorations)
    if (dec.styleKey == QLatin1String(key))
      ++n;
  return n;
}

} // namespace

class CellularTest : public QObject {
  Q_OBJECT
private slots:
  void eightPinsThreeRows() {
    navi::CellularLayoutStrategy s;
    navi::DensityPrefs d;
    auto pins = makePins(8);
    auto scene = s.build(pins, pins[4].id, d);
    QCOMPARE(int(scene.items.size()), 8);
    QCOMPARE(int(scene.decorations.size()), 10); // 3 + 4 + 3 cells
    QCOMPARE(countStyle(scene, "cell"), 7);
    QCOMPARE(countStyle(scene, "cell.focused"), 1);
    QCOMPARE(countStyle(scene, "cell.empty"), 2);
    QCOMPARE(scene.hub.selectedTitle, pins[4].label);
    QCOMPARE(scene.hub.focusedId, pins[4].id);
  }

  void rowsInterlock() {
    navi::CellularLayoutStrategy s;
    navi::DensityPrefs d;
    auto pins = makePins(8);
    auto scene = s.build(pins, {}, d);
    // row0 (3 cells) starts 44px right of row1 (4 cells); pitch 88 / 77
    QCOMPARE(scene.items[0].bounds, QRectF(72, 28, 80, 92));   // row0 col0
    QCOMPARE(scene.items[3].bounds, QRectF(28, 105, 80, 92));  // row1 col0
    QCOMPARE(scene.items[7].bounds, QRectF(72, 182, 80, 92));  // row2 col0
    QVERIFY(scene.items[0].shape.has_value());
    QCOMPARE(scene.items[0].shape->size(), 6);
  }

  void twentyPinsTileOnward() {
    navi::CellularLayoutStrategy s;
    navi::DensityPrefs d;
    auto pins = makePins(20);
    auto scene = s.build(pins, {}, d);
    QCOMPARE(int(scene.items.size()), 20);
    QCOMPARE(int(scene.decorations.size()), 21); // 3+4+3+4+3+4
    QCOMPARE(countStyle(scene, "cell.empty"), 1);
    // pin 19 is row5 col2: x = 28 + 2*88, y = 28 + 5*77
    QCOMPARE(scene.items[19].bounds, QRectF(204, 413, 80, 92));
  }

  void zeroPinsShowEmptySlots() {
    navi::CellularLayoutStrategy s;
    navi::DensityPrefs d;
    auto scene = s.build({}, {}, d);
    QCOMPARE(int(scene.items.size()), 0);
    QCOMPARE(int(scene.decorations.size()), 3);
    QCOMPARE(countStyle(scene, "cell.empty"), 3);
    QCOMPARE(navi::hitTest(scene, QPointF(100, 60)), QString());
  }
};

QTEST_MAIN(CellularTest)
#include "test_cellular_layout.moc"
```

Note: `zeroPinsShowEmptySlots` uses `navi::hitTest` — add `#include "core/HitTest.h"` at the top of the test file.

Register in `CMakeLists.txt`:

```cmake
navi_add_test(test_cellular_layout tests/test_cellular_layout.cpp)
```

- [ ] **Step 2: Run to verify it fails**

```bash
cmake -S . -B build2 -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" && cmake --build build2 -j 2>&1 | tail -20
```

Expected: compile error — `layout/CellularLayoutStrategy.h: No such file or directory`.

- [ ] **Step 3: Implement**

Create `src/layout/CellularLayoutStrategy.h`:

```cpp
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
```

Create `src/layout/CellularLayoutStrategy.cpp`:

```cpp
#include "layout/CellularLayoutStrategy.h"

#include "core/Hexagon.h"

#include <algorithm>

namespace navi {
namespace {

constexpr qreal kCellW = 80.0;
constexpr qreal kCellH = 92.0;
constexpr qreal kPitchX = 88.0;
constexpr qreal kPitchY = 77.0;
constexpr qreal kMargin = 28.0;
constexpr qreal kGridW = 344.0; // widest row: 3*88 + 80

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

QRectF cellRect(int row, int col) {
  const int n = rowCapacity(row);
  const qreal rowW = (n - 1) * kPitchX + kCellW;
  const qreal x0 = kMargin + (kGridW - rowW) / 2.0;
  return QRectF(x0 + col * kPitchX, kMargin + row * kPitchY, kCellW, kCellH);
}

} // namespace

SceneModel CellularLayoutStrategy::build(const QVector<Pin> &pins,
                                         const QString &focusedId,
                                         const DensityPrefs &) {
  SceneModel scene;
  const int rows = rowCountFor(pins.size());

  int index = 0;
  for (int r = 0; r < rows; ++r) {
    const int cap = rowCapacity(r);
    for (int c = 0; c < cap; ++c) {
      const QRectF rect = cellRect(r, c);
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
  scene.hub.brandSubtitle = QStringLiteral("NAVI");
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

} // namespace navi
```

Add `src/layout/CellularLayoutStrategy.cpp` to `NAVI_CORE_SOURCES` in `CMakeLists.txt`, right after the orbital line:

```cmake
  src/layout/OrbitalLayoutStrategy.cpp
  src/layout/CellularLayoutStrategy.cpp
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
cmake -S . -B build2 -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" && cmake --build build2 -j && ctest --test-dir build2 --output-on-failure
```

Expected: 7 tests pass, including `test_cellular_layout`. `test_orbital_layout` must still pass (orbital strategy untouched).

---

### Task 3: CellularGlassView

QPainter view rendering the honeycomb: focused-cell glow, hex cell chrome per `styleKey`, 40px icons, and the selection pill under the grid. Input mirrors the orbital view (hover → `itemHovered`, click → `itemActivated`, outside-click / Esc → `dismissRequested`, ⏎ activates hover) minus the wheel/rotation signal. `sizeHint` is content-driven so the window can fit the grid.

This is painting code; the repo has no view tests (tests cover model/layout only) — verification is that it compiles, plus the Task 5 manual run.

**Files:**
- Create: `src/ui/CellularGlassView.h`
- Create: `src/ui/CellularGlassView.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Implement**

Create `src/ui/CellularGlassView.h`:

```cpp
#pragma once

#include "core/MenuView.h"
#include "core/Types.h"

#include <QHash>
#include <QIcon>
#include <QWidget>

class QPainter;

namespace navi {

class CellularGlassView : public QWidget, public MenuView {
  Q_OBJECT
public:
  explicit CellularGlassView(QWidget *parent = nullptr);

  void setScene(const SceneModel &scene) override;
  QWidget *widget() override { return this; }
  QSize sizeHint() const override;

  void setAppearance(Appearance appearance);
  void setIcon(const QString &pinId, const QIcon &icon);

signals:
  void itemHovered(const QString &id);
  void itemActivated(const QString &id);
  void dismissRequested();

protected:
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  QRectF gridBounds() const;
  void paintPill(QPainter &p, bool dark);

  SceneModel scene_;
  Appearance appearance_ = Appearance::Dark;
  QHash<QString, QIcon> icons_;
  QString lastHover_;
};

} // namespace navi
```

Create `src/ui/CellularGlassView.cpp`:

```cpp
#include "ui/CellularGlassView.h"

#include "core/Hexagon.h"
#include "core/HitTest.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace navi {
namespace {

constexpr qreal kMargin = 28.0;
constexpr qreal kPillGap = 16.0;
constexpr qreal kPillW = 180.0;
constexpr qreal kPillH = 48.0;
constexpr qreal kIconSize = 40.0;

} // namespace

CellularGlassView::CellularGlassView(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
}

void CellularGlassView::setScene(const SceneModel &scene) {
  scene_ = scene;
  updateGeometry();
  update();
}

void CellularGlassView::setAppearance(Appearance appearance) {
  appearance_ = appearance;
  update();
}

void CellularGlassView::setIcon(const QString &pinId, const QIcon &icon) {
  icons_.insert(pinId, icon);
  update();
}

QRectF CellularGlassView::gridBounds() const {
  QRectF g;
  for (const auto &dec : scene_.decorations)
    g = g.united(dec.bounds);
  return g;
}

QSize CellularGlassView::sizeHint() const {
  const QRectF g = gridBounds();
  const qreal w = qMax<qreal>(g.width(), 200) + 2 * kMargin;
  const qreal h = g.height() + 2 * kMargin + kPillGap + kPillH;
  return QSize(int(std::ceil(w)), int(std::ceil(h)));
}

void CellularGlassView::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  const bool dark = appearance_ != Appearance::Light;

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

  // Cell chrome
  for (const auto &dec : scene_.decorations) {
    if (dec.kind != DecorationKind::Path)
      continue;
    QPainterPath hex;
    hex.addPolygon(hexagonForRect(dec.bounds));
    hex.closeSubpath();

    if (dec.styleKey == QLatin1String("cell.focused")) {
      QLinearGradient fill(dec.bounds.topLeft(), dec.bounds.bottomRight());
      fill.setColorAt(0.0, QColor(248, 221, 156)); // #F8DD9C
      fill.setColorAt(1.0, QColor(238, 200, 109)); // #EEC86D
      p.setPen(Qt::NoPen);
      p.setBrush(fill);
      p.drawPath(hex);
    } else if (dec.styleKey == QLatin1String("cell.empty")) {
      QPen pen(dark ? QColor(255, 255, 255, 72) : QColor(42, 39, 31, 64), 1.2);
      pen.setDashPattern({5, 6});
      p.setPen(pen);
      p.setBrush(Qt::NoBrush);
      p.drawPath(hex);
      p.setPen(dark ? QColor(255, 255, 255, 90) : QColor(42, 39, 31, 90));
      QFont f = font();
      f.setPixelSize(18);
      p.setFont(f);
      p.drawText(dec.bounds, Qt::AlignCenter, QStringLiteral("+"));
    } else {
      p.setPen(QPen(dark ? QColor(255, 255, 255, 34) : QColor(42, 39, 31, 26),
                    1));
      p.setBrush(dark ? QColor(255, 255, 255, 18) : QColor(252, 251, 247));
      p.drawPath(hex);
    }
  }

  // Icons
  for (const auto &item : scene_.items) {
    if (item.role != ItemRole::Item)
      continue;
    const QRectF r(item.bounds.center().x() - kIconSize / 2.0,
                   item.bounds.center().y() - kIconSize / 2.0, kIconSize,
                   kIconSize);
    const QIcon icon = icons_.value(item.id);
    if (!icon.isNull()) {
      icon.paint(&p, r.toRect());
    } else {
      p.setPen(dark ? QColor(255, 255, 255, 180) : QColor(40, 40, 50));
      QFont f = font();
      f.setPixelSize(14);
      f.setBold(true);
      p.setFont(f);
      p.drawText(r, Qt::AlignCenter, item.id.left(2).toUpper());
    }
  }

  paintPill(p, dark);
}

void CellularGlassView::paintPill(QPainter &p, bool dark) {
  const QRectF g = gridBounds();
  const QRectF pill((width() - kPillW) / 2.0, g.bottom() + kPillGap, kPillW,
                    kPillH);

  QPainterPath path;
  path.addRoundedRect(pill, kPillH / 2.0, kPillH / 2.0);
  p.setPen(QPen(dark ? QColor(255, 255, 255, 30) : QColor(42, 39, 31, 26), 1));
  p.setBrush(dark ? QColor(18, 16, 24, 200) : QColor(255, 255, 255, 220));
  p.drawPath(path);

  const bool hasFocus = !scene_.hub.focusedId.isEmpty();
  const QIcon icon = icons_.value(scene_.hub.focusedId);
  if (!icon.isNull())
    icon.paint(&p, QRect(int(pill.left()) + 11, int(pill.top()) + 11, 26, 26));

  const QRectF textRect(pill.left() + 47, pill.top(), pill.width() - 55,
                        pill.height());
  p.setPen(dark ? QColor(242, 238, 229) : QColor(42, 39, 31));
  QFont title = font();
  title.setPixelSize(13);
  title.setBold(true);
  p.setFont(title);
  p.drawText(QRectF(textRect.left(), textRect.top() + 7, textRect.width(), 16),
             Qt::AlignLeft | Qt::AlignVCenter,
             hasFocus ? scene_.hub.selectedTitle : QStringLiteral("Navi"));

  p.setPen(dark ? QColor(138, 133, 120) : QColor(120, 114, 100));
  QFont sub = font();
  sub.setPixelSize(9);
  p.setFont(sub);
  p.drawText(
      QRectF(textRect.left(), textRect.top() + 26, textRect.width(), 12),
      Qt::AlignLeft | Qt::AlignVCenter,
      hasFocus ? QStringLiteral("PRESS RETURN TO OPEN")
               : QStringLiteral("HOVER TO SELECT"));
}

void CellularGlassView::mouseMoveEvent(QMouseEvent *event) {
  const QString id = hitTest(scene_, event->position());
  if (id != lastHover_) {
    lastHover_ = id;
    emit itemHovered(id);
  }
}

void CellularGlassView::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;
  const QString id = hitTest(scene_, event->position());
  if (!id.isEmpty())
    emit itemActivated(id);
  else
    emit dismissRequested();
}

void CellularGlassView::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape)
    emit dismissRequested();
  else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    emit itemActivated(lastHover_);
  else
    QWidget::keyPressEvent(event);
}

void CellularGlassView::leaveEvent(QEvent *) {
  if (!lastHover_.isEmpty()) {
    lastHover_.clear();
    emit itemHovered({});
  }
}

} // namespace navi
```

Add to `NAVI_CORE_SOURCES` in `CMakeLists.txt`, after the orbital view line:

```cmake
  src/ui/OrbitalGlassView.cpp
  src/ui/CellularGlassView.cpp
```

- [ ] **Step 2: Build**

```bash
cmake -S . -B build2 -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" && cmake --build build2 -j
```

Expected: compiles clean; `ctest --test-dir build2 --output-on-failure` still 7/7.

---

### Task 4: Wire it up — style switch, content-sized window, Cellular default

`Prefs` already persists `style`; flip its default to Cellular, pick strategy/view in `main.cpp`, and make `OverlayWindow` size itself from the view's `sizeHint` (honeycomb is 400×~366, not 560×560).

**Files:**
- Modify: `src/core/Prefs.cpp:33-35` (default style)
- Modify: `src/ui/OverlayWindow.h` and `src/ui/OverlayWindow.cpp` (add `resizeToContent`)
- Modify: `src/ui/OrbitalGlassView.h` (add `sizeHint`)
- Modify: `src/main.cpp` (factory + wiring)

- [ ] **Step 1: Prefs default** — in `src/core/Prefs.cpp` change the style default:

```cpp
  styleId_ = static_cast<StyleId>(
      settings_->value(QStringLiteral("style"), static_cast<int>(StyleId::Cellular))
          .toInt());
```

(The `style` key is only ever written by `setStyleId`, which nothing calls yet, so existing installs have no saved value and pick up the new default.)

- [ ] **Step 2: OverlayWindow::resizeToContent** — add to `src/ui/OverlayWindow.h` under `setContent`:

```cpp
  void resizeToContent();
```

and to `src/ui/OverlayWindow.cpp`:

```cpp
void OverlayWindow::resizeToContent() {
  if (!content_)
    return;
  const QSize hint = content_->sizeHint();
  if (hint.isValid())
    resize(hint);
}
```

- [ ] **Step 3: OrbitalGlassView::sizeHint** — in `src/ui/OrbitalGlassView.h`, add under `widget()`:

```cpp
  QSize sizeHint() const override { return QSize(560, 560); }
```

- [ ] **Step 4: Rewrite `src/main.cpp`** with the style switch (complete file):

```cpp
#include "core/PinStore.h"
#include "core/Prefs.h"
#include "core/SessionController.h"
#include "layout/CellularLayoutStrategy.h"
#include "layout/OrbitalLayoutStrategy.h"
#include "platform/AppLauncher.h"
#include "platform/IconProvider.h"
#include "platform/MacActivation.h"
#include "platform/TrayController.h"
#include "ui/CellularGlassView.h"
#include "ui/OrbitalGlassView.h"
#include "ui/OverlayWindow.h"

#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QUuid>

#include <type_traits>

namespace {

QVector<navi::Pin> seedMacApps() {
  const QStringList candidates = {
      QStringLiteral("/Applications/Safari.app"),
      QStringLiteral("/System/Applications/Utilities/Terminal.app"),
      QStringLiteral("/Applications/Visual Studio Code.app"),
      QStringLiteral("/Applications/Slack.app"),
      QStringLiteral("/Applications/Figma.app"),
      QStringLiteral("/System/Applications/Music.app"),
      QStringLiteral("/System/Applications/Finder.app"),
      QStringLiteral("/System/Applications/Notes.app"),
      QStringLiteral("/Applications/Google Chrome.app"),
      QStringLiteral("/Applications/Spotify.app"),
      QStringLiteral("/Applications/Notion.app"),
      QStringLiteral("/Applications/Discord.app"),
  };
  QVector<navi::Pin> pins;
  for (const QString &path : candidates) {
    if (!QFileInfo::exists(path))
      continue;
    navi::Pin pin;
    pin.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    pin.appPath = path;
    pin.label = QFileInfo(path).completeBaseName();
    pin.iconKey = path;
    pins.push_back(pin);
    if (pins.size() >= 12)
      break;
  }
  return pins;
}

} // namespace

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("Navi"));
  QCoreApplication::setApplicationName(QStringLiteral("Navi"));
  app.setQuitOnLastWindowClosed(false);
  navi::macActivateApplication();

  QSettings settings;
  navi::Prefs prefs(&settings);
  prefs.load();
  navi::PinStore pinStore(&settings);
  pinStore.load();
  if (pinStore.pins().isEmpty())
    pinStore.setPins(seedMacApps());

  navi::SessionController session;
  navi::IconProvider icons;
  navi::AppLauncher launcher;

  const bool cellular = prefs.styleId() == navi::StyleId::Cellular;
  const navi::Appearance appearance = prefs.appearance() == navi::Appearance::Light
                                          ? navi::Appearance::Light
                                          : navi::Appearance::Dark;

  navi::CellularLayoutStrategy cellularStrategy;
  navi::OrbitalLayoutStrategy orbitalStrategy;
  navi::LayoutStrategy *strategy =
      cellular ? static_cast<navi::LayoutStrategy *>(&cellularStrategy)
               : static_cast<navi::LayoutStrategy *>(&orbitalStrategy);

  navi::CellularGlassView *cellularView = nullptr;
  navi::OrbitalGlassView *orbitalView = nullptr;
  navi::MenuView *view = nullptr;
  if (cellular) {
    cellularView = new navi::CellularGlassView;
    cellularView->setAppearance(appearance);
    for (const auto &pin : pinStore.pins())
      cellularView->setIcon(pin.id, icons.iconForPath(pin.appPath));
    view = cellularView;
  } else {
    orbitalView = new navi::OrbitalGlassView;
    orbitalView->setAppearance(appearance);
    for (const auto &pin : pinStore.pins())
      orbitalView->setIcon(pin.id, icons.iconForPath(pin.appPath));
    view = orbitalView;
  }

  navi::OverlayWindow overlay(view->widget());

  auto rebuild = [&] {
    const auto scene =
        strategy->build(pinStore.pins(), session.focusedId(), prefs.density());
    view->setScene(scene);
    overlay.resizeToContent();
  };

  auto showMenu = [&] {
    session.open();
    if (session.focusedId().isEmpty() && !pinStore.pins().isEmpty())
      session.setFocus(pinStore.pins().first().id);
    rebuild();
    QPoint anchor = QCursor::pos();
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
      // Prefer screen center on first show so the widget isn't lost
      // under the IDE / off a secondary display edge.
      anchor = screen->availableGeometry().center();
    }
    overlay.showAt(anchor);
    navi::macActivateApplication();
  };

  auto hideMenu = [&] {
    session.close();
    overlay.hide();
  };

  auto wireView = [&](auto *v) {
    using V = std::remove_pointer_t<decltype(v)>;
    QObject::connect(v, &V::itemHovered, &session, [&](const QString &id) {
      session.setFocus(id);
      rebuild();
    });
    QObject::connect(v, &V::itemActivated, &app, [&](const QString &id) {
      session.setFocus(id);
      const QString activated = session.activate();
      if (activated.isEmpty())
        return;
      for (const auto &pin : pinStore.pins()) {
        if (pin.id == activated) {
          launcher.launch(pin.appPath);
          break;
        }
      }
      overlay.hide();
    });
    QObject::connect(v, &V::dismissRequested, &app, hideMenu);
  };
  if (cellularView)
    wireView(cellularView);
  else
    wireView(orbitalView);

  if (orbitalView) {
    QObject::connect(orbitalView, &navi::OrbitalGlassView::rotationDelta, &app,
                     [&](qreal delta) {
                       orbitalStrategy.setRotationRadians(
                           orbitalStrategy.rotationRadians() + delta);
                       rebuild();
                     });
  }

  navi::TrayController tray;
  QObject::connect(&tray, &navi::TrayController::showRequested, &app, showMenu);
  QObject::connect(&tray, &navi::TrayController::quitRequested, &app,
                   &QApplication::quit);
  QObject::connect(&tray, &navi::TrayController::prefsRequested, &app, [] {
    // PrefsDialog arrives in a later task.
  });

  showMenu();
  return app.exec();
}
```

- [ ] **Step 5: Build and run full suite**

```bash
cmake --build build2 -j && ctest --test-dir build2 --output-on-failure
```

Expected: clean build, 7/7 tests pass.

---

### Task 5: End-to-end verification + README

- [ ] **Step 1: Full clean-ish verification**

```bash
cmake --build build2 -j && ctest --test-dir build2 --output-on-failure
```

Expected: 7/7 pass (test_smoke, test_session, test_orbital_layout, test_scene_hittest, test_prefs, test_hexhittest, test_cellular_layout).

- [ ] **Step 2: Run the app and eyeball the widget**

```bash
open build2/Navi.app
```

Verify against the Ardot frames: hex cells in 3/4/3 rows, no window chrome, amber focused cell with glow, icons centered, pill below showing the focused app, Esc dismisses, clicking a cell launches the app, clicking empty space dismisses. Move the mouse across row boundaries — hover must land on the visually-correct cell (polygon hit test). Then quit and re-run with orbital to confirm the switch still works:

```bash
defaults write Navi.Navi style -int 0 && open build2/Navi.app
# verify orbital wheel renders, quit, then restore:
defaults delete Navi.Navi style
```

(The QSettings domain is `<OrganizationName>.<ApplicationName>` = `Navi.Navi`; if `defaults` errors, check `~/Library/Preferences` for the actual plist name.)

- [ ] **Step 3: README** — two edits in `README.md`:

Replace line 3:

```markdown
Cross-platform radial desktop launcher — Qt Widgets, hotkey + tray, pluggable UI skins (Orbital v1; Cellular / Pie later).
```

with:

```markdown
Cross-platform desktop launcher summoned at the cursor — Qt Widgets, hotkey + tray, pluggable UI skins (Cellular honeycomb by default; set `style=0` in QSettings for Orbital, Pie later).
```

In the Docs section, add after the existing Plan line:

```markdown
- Plan: `docs/superpowers/plans/2026-09-21-honeycomb-widget.md`
```

- [ ] **Step 4: Final test run**

```bash
ctest --test-dir build2 --output-on-failure
```

Expected: 7/7 pass.

---

## Addendum (2026-09-21, post-implementation refinements)

- **Pill removed → focus label**: the 180×48 pill became a plain 11px bold centered app-name line under the grid (`paintFocusLabel`). The selection is already carried by the amber hex; the bar was redundant weight.
- **Sticky selection**: hovering off cells no longer clears focus — the view only emits non-empty `itemHovered`, `leaveEvent` keeps `lastHover_`, and main.cpp ignores empty hover ids. ⏎ always activates the visible selection.
- **System appearance resolves the real OS theme** (`QStyleHints::colorScheme()`), with live updates via `colorSchemeChanged` — dark glass no longer floats over light desktops.
- **Dock-style animation** (`src/ui/DockAnim.h` + animator in `CellularGlassView`):
  - **Magnification**: cosine falloff (radius 150px), max +22% scale at cursor, cells painted largest-on-top; 60fps exponential settle.
  - **Open pop**: easeOutBack scale-in, staggered 26ms outward from grid center, replayed on every summon via `playOpenAnimation()`.
  - **Click dip**: 12% press sink over 180ms; `main.cpp` delays `overlay.hide()` by 180ms on activation so the dip is visible.
  - Math covered by `tests/test_dockanim.cpp` (8/8 suite green).
