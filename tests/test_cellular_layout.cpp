#include "layout/CellularLayoutStrategy.h"

#include "core/HitTest.h"

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
