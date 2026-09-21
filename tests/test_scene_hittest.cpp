#include "core/HitTest.h"
#include "core/SceneModel.h"

#include <QtTest>

class HitTestTest : public QObject {
  Q_OBJECT
private slots:
  void hitsTopmostItem() {
    navi::SceneModel scene;
    navi::PlacedItem a;
    a.id = QStringLiteral("a");
    a.bounds = QRectF(0, 0, 40, 40);
    a.z = 1;
    a.role = navi::ItemRole::Item;
    navi::PlacedItem b;
    b.id = QStringLiteral("b");
    b.bounds = QRectF(20, 20, 40, 40);
    b.z = 2;
    b.role = navi::ItemRole::Item;
    scene.items = {a, b};
    scene.hitOrder = {QStringLiteral("b"), QStringLiteral("a")};
    QCOMPARE(navi::hitTest(scene, QPointF(25, 25)), QStringLiteral("b"));
    QCOMPARE(navi::hitTest(scene, QPointF(5, 5)), QStringLiteral("a"));
    QVERIFY(navi::hitTest(scene, QPointF(200, 200)).isEmpty());
  }
};

QTEST_MAIN(HitTestTest)
#include "test_scene_hittest.moc"
