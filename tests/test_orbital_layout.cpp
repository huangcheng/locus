#include "layout/OrbitalLayoutStrategy.h"

#include <QtTest>

#include <algorithm>
#include <cmath>

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

int trackCount(const locus::SceneModel &scene) {
  int n = 0;
  for (const auto &dec : scene.decorations)
    if (dec.styleKey == QLatin1String("orbit.track"))
      ++n;
  return n;
}

qreal discWidth(const locus::SceneModel &scene) {
  for (const auto &dec : scene.decorations)
    if (dec.styleKey == QLatin1String("orbit.disc"))
      return dec.bounds.width();
  return 0;
}

} // namespace

class OrbitalTest : public QObject {
  Q_OBJECT
private slots:
  void sixPinsOneTrack() {
    locus::OrbitalLayoutStrategy s;
    locus::DensityPrefs d;
    auto pins = makePins(6);
    auto scene = s.build(pins, pins[0].id, d);
    QCOMPARE(itemCount(scene), 6);
    QCOMPARE(trackCount(scene), 1);
    QCOMPARE(scene.hub.focusedId, pins[0].id);
    QCOMPARE(scene.hub.selectedTitle, pins[0].label);
    // First pin sits at the top of the inner track (radius 88).
    const QRectF disc = scene.decorations.first().bounds;
    const QPointF c = scene.items.first().bounds.center();
    QVERIFY(qAbs(c.x() - disc.center().x()) < 0.01);
    QVERIFY(qAbs(disc.center().y() - c.y() - 88.0) < 0.01);
  }

  void ninePinsTwoTracks() {
    locus::OrbitalLayoutStrategy s;
    locus::DensityPrefs d;
    auto scene = s.build(makePins(9), {}, d);
    QCOMPARE(itemCount(scene), 9);
    QCOMPARE(trackCount(scene), 2);
  }

  void twentyPinsThreeTracks() {
    locus::OrbitalLayoutStrategy s;
    locus::DensityPrefs d;
    auto scene = s.build(makePins(20), {}, d);
    QCOMPARE(itemCount(scene), 20);
    QCOMPARE(trackCount(scene), 3);
  }

  void discGrowsWithTracks() {
    locus::OrbitalLayoutStrategy s;
    locus::DensityPrefs d;
    const qreal oneTrack = discWidth(s.build(makePins(6), {}, d));
    const qreal threeTracks = discWidth(s.build(makePins(20), {}, d));
    QVERIFY(oneTrack > 0);
    QVERIFY(threeTracks > oneTrack);
  }

  void emptyPinsHubOnly() {
    locus::OrbitalLayoutStrategy s;
    locus::DensityPrefs d;
    auto scene = s.build({}, {}, d);
    QCOMPARE(itemCount(scene), 0);
    QCOMPARE(trackCount(scene), 0);
    QVERIFY(discWidth(scene) > 0); // hub-sized disc still renders
  }

  void densityScalesGeometry() {
    locus::OrbitalLayoutStrategy s;
    locus::DensityPrefs d;
    d.cellSize = 96;
    d.cellGap = 16;
    auto scene = s.build(makePins(6), {}, d);
    // Track radius and chip size follow the shared sliders.
    const QRectF disc = scene.decorations.first().bounds;
    const QPointF c = scene.items.first().bounds.center();
    QVERIFY(qAbs(disc.center().y() - c.y() - 96.0 * 1.1) < 0.01);
    QVERIFY(qAbs(scene.items.first().bounds.width() - 96.0 * 0.55) < 0.01);
    // Wider spacing pushes the second track further out.
    const auto twoTracks = s.build(makePins(9), {}, d);
    qreal r1 = 0;
    for (const auto &dec : twoTracks.decorations)
      if (dec.styleKey == QLatin1String("orbit.track"))
        r1 = qMax(r1, dec.bounds.width() / 2.0);
    QVERIFY(qAbs(r1 - (96.0 * 1.1 + 96.0 * 0.6 + 16.0 * 2.0)) < 0.01);
  }
};

QTEST_MAIN(OrbitalTest)
#include "test_orbital_layout.moc"
