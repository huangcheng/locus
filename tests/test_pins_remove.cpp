// Regression: the × button on a pin row must be clickable. The row widget
// once carried WA_TransparentForMouseEvents — which (per Qt docs) silences
// the row AND its children, so clicks on × fell through to the viewport and
// pins could never be removed. This drives a click through the real
// window-system event path (full hit-testing), not widget-direct QTest.
#include "core/PinStore.h"
#include "core/Prefs.h"
#include "platforms/IconProvider.h"
#include "ui/PrefsWindow.h"
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

#include <qpa/qwindowsysteminterface.h>

class PinsRemoveTest : public QObject {
  Q_OBJECT
private slots:
  void removeButtonDeletesPin() {
    // Global-position hit-testing needs a real windowing platform.
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
      QSKIP("no window server");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")),
                       QSettings::IniFormat);
    locus::Prefs prefs(&settings);
    prefs.load();
    locus::PinStore pins(&settings);
    for (int i = 0; i < 3; ++i) {
      locus::Pin p;
      p.id = QStringLiteral("p%1").arg(i);
      p.label = QStringLiteral("App %1").arg(i);
      p.appPath = QStringLiteral("/tmp/app%1.app").arg(i);
      pins.addPin(p);
    }
    locus::IconProvider icons;
    locus::PrefsWindow win(&prefs, &pins, &icons);
    win.show();
    win.selectPane(1); // Pins
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QList<QToolButton *> removeBtns;
    for (QToolButton *b : win.findChildren<QToolButton *>())
      if (b->text() == QStringLiteral("×"))
        removeBtns << b;
    QCOMPARE(removeBtns.size(), 3); // exactly one live row per pin

    QWindow *wh = win.windowHandle();
    QVERIFY(wh);
    const QPointF g(removeBtns.first()->mapToGlobal(QPoint(12, 12)));
    const QPointF local(wh->mapFromGlobal(g.toPoint()));
    QWindowSystemInterface::handleMouseEvent(wh, local, g, Qt::LeftButton,
                                             Qt::LeftButton,
                                             QEvent::MouseButtonPress);
    QCoreApplication::processEvents();
    QWindowSystemInterface::handleMouseEvent(wh, local, g, Qt::NoButton,
                                             Qt::LeftButton,
                                             QEvent::MouseButtonRelease);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QCOMPARE(pins.pins().size(), 2);

    // Rebuilding the list (refresh on show/retranslate) must not orphan row
    // widgets: still exactly one × per remaining pin. Deletion is deferred
    // (see removeDefersRowDeletion), so let the event loop reap the detached
    // rows before counting.
    win.refreshFromModel();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    int count = 0;
    for (QToolButton *b : win.findChildren<QToolButton *>())
      if (b->text() == QStringLiteral("×"))
        ++count;
    QCOMPARE(count, 2);
  }

  // Regression: removing a pin must not synchronously delete the row widget —
  // its × button is the object emitting clicked at that moment, so a direct
  // `delete` is use-after-free on the signal-emission stack (crashed the
  // release app intermittently). Deletion must be deferred to the event loop.
  void removeDefersRowDeletion() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")),
                       QSettings::IniFormat);
    locus::Prefs prefs(&settings);
    prefs.load();
    locus::PinStore pins(&settings);
    locus::Pin p;
    p.id = QStringLiteral("p0");
    p.label = QStringLiteral("App 0");
    p.appPath = QStringLiteral("/tmp/app0.app");
    pins.addPin(p);
    locus::IconProvider icons;
    locus::PrefsWindow win(&prefs, &pins, &icons);
    win.show();
    win.selectPane(1); // Pins
    QCoreApplication::processEvents();

    QToolButton *btn = nullptr;
    for (QToolButton *b : win.findChildren<QToolButton *>())
      if (b->text() == QStringLiteral("×")) {
        btn = b;
        break;
      }
    QVERIFY(btn);
    const QPointer<QToolButton> guard(btn);

    btn->click(); // synchronous clicked emission drives onRemove

    // The emitting button must still be alive here; deletion is deferred.
    QVERIFY2(!guard.isNull(), "row widget deleted during clicked emission");
    QCOMPARE(pins.pins().size(), 0);

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(guard.isNull());
  }
};

QTEST_MAIN(PinsRemoveTest)
#include "test_pins_remove.moc"
