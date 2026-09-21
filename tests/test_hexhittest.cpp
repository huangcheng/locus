#include "core/Hexagon.h"
#include "core/HitTest.h"

#include <QtTest>

namespace {

locus::PlacedItem hexItem(const QString &id, const QRectF &rect, int z) {
  locus::PlacedItem it;
  it.id = id;
  it.bounds = rect;
  it.z = z;
  it.role = locus::ItemRole::Item;
  it.shape = locus::hexagonForRect(rect);
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
    locus::SceneModel scene;
    scene.items = {hexItem(QStringLiteral("a"), QRectF(72, 28, 80, 92), 0),
                   hexItem(QStringLiteral("b"), QRectF(116, 105, 80, 92), 10)};
    scene.hitOrder = {QStringLiteral("b"), QStringLiteral("a")};
    QCOMPARE(locus::hitTest(scene, QPointF(120, 112)), QStringLiteral("a"));
    QCOMPARE(locus::hitTest(scene, QPointF(156, 151)), QStringLiteral("b"));
    QCOMPARE(locus::hitTest(scene, QPointF(10, 10)), QString());
  }

  void rectFallbackWithoutShape() {
    locus::PlacedItem it;
    it.id = QStringLiteral("r");
    it.bounds = QRectF(0, 0, 50, 50);
    it.z = 0;
    it.role = locus::ItemRole::Item;
    locus::SceneModel scene;
    scene.items = {it};
    scene.hitOrder = {QStringLiteral("r")};
    QCOMPARE(locus::hitTest(scene, QPointF(25, 25)), QStringLiteral("r"));
    QCOMPARE(locus::hitTest(scene, QPointF(60, 60)), QString());
  }
};

QTEST_MAIN(HexHitTest)
#include "test_hexhittest.moc"
