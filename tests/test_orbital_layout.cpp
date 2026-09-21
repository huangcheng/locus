#include "layout/OrbitalLayoutStrategy.h"

#include <QtTest>

#include <algorithm>
#include <cmath>

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

} // namespace

class OrbitalTest : public QObject {
  Q_OBJECT
private slots:
  void eightPinsOneRing() {
    navi::OrbitalLayoutStrategy s;
    navi::DensityPrefs d;
    auto pins = makePins(8);
    auto scene = s.build(pins, pins[0].id, d);
    int items = 0;
    for (const auto &it : scene.items)
      if (it.role == navi::ItemRole::Item)
        ++items;
    QCOMPARE(items, 8);
    QVERIFY(std::any_of(
        scene.decorations.begin(), scene.decorations.end(), [](const auto &dec) {
          return dec.styleKey == QLatin1String("orbital.inner");
        }));
    QVERIFY(std::none_of(
        scene.decorations.begin(), scene.decorations.end(), [](const auto &dec) {
          return dec.styleKey == QLatin1String("orbital.outer");
        }));
    QVERIFY(std::any_of(
        scene.decorations.begin(), scene.decorations.end(), [](const auto &dec) {
          return dec.styleKey == QLatin1String("hover.well");
        }));
    QCOMPARE(scene.hub.selectedTitle, pins[0].label);
  }

  void twentyPinsTwoRings() {
    navi::OrbitalLayoutStrategy s;
    navi::DensityPrefs d;
    auto pins = makePins(20);
    auto scene = s.build(pins, {}, d);
    int items = 0;
    for (const auto &it : scene.items)
      if (it.role == navi::ItemRole::Item)
        ++items;
    QCOMPARE(items, 20);
    QVERIFY(std::any_of(
        scene.decorations.begin(), scene.decorations.end(), [](const auto &dec) {
          return dec.styleKey == QLatin1String("orbital.outer");
        }));
  }

  void rotationChangesPositions() {
    navi::OrbitalLayoutStrategy s;
    auto pins = makePins(8);
    navi::DensityPrefs d;
    auto a = s.build(pins, {}, d);
    s.setRotationRadians(M_PI / 4);
    auto b = s.build(pins, {}, d);
    const QPointF ca = a.items[0].bounds.center();
    const QPointF cb = b.items[0].bounds.center();
    QVERIFY(std::hypot(ca.x() - cb.x(), ca.y() - cb.y()) > 1.0);
  }
};

QTEST_MAIN(OrbitalTest)
#include "test_orbital_layout.moc"
