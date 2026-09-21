#include "core/SceneModel.h"
#include "core/Pin.h"

#include <QtTest>

class SmokeTest : public QObject {
  Q_OBJECT
private slots:
  void sceneModelDefaults() {
    navi::SceneModel scene;
    QCOMPARE(scene.hub.brandSubtitle, QStringLiteral("NAVI"));
    QVERIFY(scene.items.isEmpty());
  }

  void pinFields() {
    navi::Pin pin;
    pin.id = QStringLiteral("1");
    pin.label = QStringLiteral("Safari");
    pin.appPath = QStringLiteral("/Applications/Safari.app");
    QCOMPARE(pin.label, QStringLiteral("Safari"));
  }
};

QTEST_MAIN(SmokeTest)
#include "test_smoke.moc"
