#include "layout/FanLayoutStrategy.h"

#include "core/HitTest.h"

#include <QtTest>
#include <QtMath>

#include <QLineF>
#include <QSet>

namespace {

QVector<locus::Pin> makePins(int n) {
  QVector<locus::Pin> pins;
  pins.reserve(n);
  for (int i = 0; i < n; ++i) {
    locus::Pin p;
    p.id = QStringLiteral("p%1").arg(i);
    p.label = QStringLiteral("App %1").arg(i);
    p.appPath = QStringLiteral("/tmp/app%1.app").arg(i);
    pins.push_back(p);
  }
  return pins;
}

int itemCount(const locus::SceneModel &scene) {
  int n = 0;
  for (const auto &it : scene.items)
    if (it.role == locus::ItemRole::Item)
      ++n;
  return n;
}

int handCount(const locus::SceneModel &scene) {
  int n = 0;
  for (const auto &dec : scene.decorations)
    if (dec.styleKey == QLatin1String("fan.track"))
      ++n;
  return n;
}

} // namespace

class FanLayoutTest : public QObject {
  Q_OBJECT
private slots:
  void oneHandIsHorizontalStack() {
    locus::FanLayoutStrategy s;
    locus::DensityPrefs d;
    d.widgetSize = 560;
    auto pins = makePins(8);
    auto scene = s.build(pins, pins[7].id, d);
    QCOMPARE(itemCount(scene), 8);
    QCOMPARE(handCount(scene), 1);

    // Neighbors sit close (peek overlap), left→right.
    for (int i = 1; i < scene.items.size(); ++i) {
      QVERIFY(scene.items[i].bounds.center().x() >
              scene.items[i - 1].bounds.center().x());
      QVERIFY(QLineF(scene.items[i - 1].bounds.center(),
                     scene.items[i].bounds.center())
                  .length() < 50.0);
      QVERIFY(scene.items[i].z > scene.items[i - 1].z ||
              scene.items[i].id == pins[7].id);
    }
  }

  void twoHandsOverlapLikeARealHand() {
    locus::FanLayoutStrategy s;
    locus::DensityPrefs d;
    d.widgetSize = 560;
    auto pins = makePins(20);
    auto scene = s.build(pins, {}, d);
    QCOMPARE(itemCount(scene), 20);
    QCOMPARE(handCount(scene), 2);

    // Bottom hand (first 12 pins) must paint above the upper hand, and the
    // rows overlap deeply — only the back hand's top strip stays visible.
    int minBottomZ = 100000, maxTopZ = -1;
    qreal bottomY = 0, topY = 0;
    int nb = 0, nt = 0;
    for (const auto &it : scene.items) {
      const int pinNum = it.id.mid(1).toInt();
      if (pinNum < 12) {
        minBottomZ = qMin(minBottomZ, it.z);
        bottomY += it.bounds.center().y();
        ++nb;
      } else {
        maxTopZ = qMax(maxTopZ, it.z);
        topY += it.bounds.center().y();
        ++nt;
      }
    }
    QVERIFY(nb > 0 && nt > 0);
    QVERIFY(minBottomZ > maxTopZ);
    const qreal cardH = d.cellSize * 1.375;
    const qreal separation = bottomY / nb - topY / nt;
    QVERIFY(separation > cardH * 0.25);
    QVERIFY(separation < cardH * 0.55);
  }

  void focusDoesNotRelayoutNeighbors() {
    locus::FanLayoutStrategy s;
    locus::DensityPrefs d;
    d.widgetSize = 560;
    auto pins = makePins(12);
    auto a = s.build(pins, pins[2].id, d);
    auto b = s.build(pins, pins[8].id, d);
    for (int i = 0; i < a.items.size(); ++i) {
      QCOMPARE(a.items[i].id, b.items[i].id);
      QCOMPARE(a.items[i].bounds.center(), b.items[i].bounds.center());
    }
  }

  void emptyPinsOk() {
    locus::FanLayoutStrategy s;
    locus::DensityPrefs d;
    auto scene = s.build({}, {}, d);
    QCOMPARE(itemCount(scene), 0);
  }

  void densityScalesCards() {
    locus::FanLayoutStrategy s;
    locus::DensityPrefs d;
    d.cellSize = 96;
    d.cellGap = 16;
    auto scene = s.build(makePins(4), {}, d);
    // Card metrics follow the shared sliders (96*1.05 wide).
    QVERIFY(qAbs(scene.items.first().bounds.width() - 96.0 * 1.05) < 0.01);
    QVERIFY(qAbs(scene.items.first().bounds.height() - 96.0 * 1.375) < 0.01);
    // Cards radiate from a pivot: the horizontal step at card-center height
    // is tighter than the peek strip measured at the top edge.
    const qreal peek = 16.0 * 2.75;
    const qreal dx = scene.items[1].bounds.center().x() -
                     scene.items[0].bounds.center().x();
    QVERIFY(dx < peek);
    QVERIFY(dx > peek * 0.5);
  }
};

QTEST_MAIN(FanLayoutTest)
#include "test_fan_layout.moc"
