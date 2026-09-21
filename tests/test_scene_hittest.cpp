#include "core/HitTest.h"
#include "core/SceneModel.h"

#include <QtTest>

class HitTestTest : public QObject {
  Q_OBJECT
private slots:
  void hitsTopmostItem() {
    locus::SceneModel scene;
    locus::PlacedItem a;
    a.id = QStringLiteral("a");
    a.bounds = QRectF(0, 0, 40, 40);
    a.z = 1;
    a.role = locus::ItemRole::Item;
    locus::PlacedItem b;
    b.id = QStringLiteral("b");
    b.bounds = QRectF(20, 20, 40, 40);
    b.z = 2;
    b.role = locus::ItemRole::Item;
    scene.items = {a, b};
    scene.hitOrder = {QStringLiteral("b"), QStringLiteral("a")};
    QCOMPARE(locus::hitTest(scene, QPointF(25, 25)), QStringLiteral("b"));
    QCOMPARE(locus::hitTest(scene, QPointF(5, 5)), QStringLiteral("a"));
    QVERIFY(locus::hitTest(scene, QPointF(200, 200)).isEmpty());
  }
};

QTEST_MAIN(HitTestTest)
#include "test_scene_hittest.moc"
