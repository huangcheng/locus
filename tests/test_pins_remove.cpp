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
    // widgets: still exactly one × per remaining pin.
    win.refreshFromModel();
    int count = 0;
    for (QToolButton *b : win.findChildren<QToolButton *>())
      if (b->text() == QStringLiteral("×"))
        ++count;
    QCOMPARE(count, 2);
  }
};

QTEST_MAIN(PinsRemoveTest)
#include "test_pins_remove.moc"
