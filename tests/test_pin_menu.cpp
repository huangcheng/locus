// Pin context menu: right-clicking a pin emits itemContextMenuRequested with
// the pin's id and a usable global position; clicking empty space emits
// nothing. OrbitalGlassView hit-tests in view coordinates directly (no origin
// offset), which keeps the test free of layout constants.
#include "core/LayoutStrategy.h"
#include "core/PinStore.h"
#include "layout/OrbitalLayoutStrategy.h"
#include "ui/OrbitalGlassView.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QtTest>

using namespace locus;

class PinMenuTest : public QObject {
  Q_OBJECT
private slots:
  void rightClickEmitsPinId() {
    QVector<Pin> pins;
    for (const char *id : {"alpha", "bravo", "charlie"}) {
      Pin p;
      p.id = QString::fromLatin1(id);
      p.label = p.id;
      pins.push_back(p);
    }
    OrbitalLayoutStrategy layout;
    const SceneModel scene =
        layout.build(pins, QStringLiteral("bravo"), DensityPrefs());
    QVERIFY(!scene.items.isEmpty());

    OrbitalGlassView view;
    view.setScene(scene);
    view.resize(view.sizeHint());

    const PlacedItem &target = scene.items.first();
    const QPoint pos = target.bounds.center().toPoint();

    QSignalSpy spy(&view, &OrbitalGlassView::itemContextMenuRequested);
    QContextMenuEvent onPin(QContextMenuEvent::Mouse, pos,
                            view.mapToGlobal(pos));
    QApplication::sendEvent(&view, &onPin);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), target.id);
    QVERIFY(!spy.first().at(1).toPoint().isNull());

    // Empty corner: no pin under the cursor, no menu.
    QContextMenuEvent onVoid(QContextMenuEvent::Mouse, QPoint(1, 1),
                             view.mapToGlobal(QPoint(1, 1)));
    QApplication::sendEvent(&view, &onVoid);
    QCOMPARE(spy.count(), 1);
  }
};

QTEST_MAIN(PinMenuTest)
#include "test_pin_menu.moc"
