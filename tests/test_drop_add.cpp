// Drag-and-drop app adding: dropping app files onto the Pins list pins them
// (duplicates deduped by path), and non-app drags are rejected so the cursor
// never promises a drop that no-ops. Delivery goes through the real
// window-system event path — synthetic sendEvent drags never reach the
// widget's drag handlers.
#include "core/PinStore.h"
#include "core/Prefs.h"
#include "platforms/IconProvider.h"
#include "ui/PrefsWindow.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QListWidget>
#include <QMimeData>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
#include <QToolButton>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformdrag.h>

class DropAddTest : public QObject {
  Q_OBJECT

#if defined(Q_OS_MAC)
  const QString appPath_ = QStringLiteral("/Applications/Foo.app");
  const QString shortcutPath_ = QStringLiteral("/Applications/Bar.app");
  const QString junkPath_ = QStringLiteral("/tmp/notes.txt");
#elif defined(Q_OS_WIN)
  const QString appPath_ = QStringLiteral("C:/Apps/Foo.exe");
  const QString shortcutPath_ = QStringLiteral("C:/Users/x/Desktop/Bar.lnk");
  const QString junkPath_ = QStringLiteral("C:/Apps/notes.txt");
#else
  const QString appPath_ = QStringLiteral("/opt/foo/bin/foo");
  const QString shortcutPath_ = QStringLiteral("/opt/bar/bin/bar");
  const QString junkPath_;
#endif

  struct Fixture {
    QTemporaryDir dir;
    QSettings settings;
    locus::Prefs prefs;
    locus::PinStore pins;
    locus::IconProvider icons;
    locus::PrefsWindow win;
    QListWidget *list = nullptr;

    Fixture()
        : settings(dir.filePath(QStringLiteral("prefs.ini")),
                   QSettings::IniFormat),
          prefs(&settings), pins(&settings), win(&prefs, &pins, &icons) {
      prefs.load();
      locus::Pin existing;
      existing.id = QStringLiteral("p0");
      existing.label = QStringLiteral("Existing");
      existing.appPath = QStringLiteral("/tmp/existing.app");
      pins.addPin(existing);
      const auto lists = win.findChildren<QListWidget *>();
      if (!lists.isEmpty())
        list = lists.first();
      win.show();
      win.selectPane(1); // Pins
      QCoreApplication::processEvents();
      QCoreApplication::processEvents();
    }
  };

private slots:
  void dropAddsAppFiles() {
    // Global-position delivery needs a real windowing platform.
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
      QSKIP("no window server");
    Fixture f;
    QVERIFY(f.list);

    QMimeData mime;
    mime.setUrls(
        {QUrl::fromLocalFile(appPath_), QUrl::fromLocalFile(shortcutPath_)});
    QWindow *wh = f.win.windowHandle();
    QVERIFY(wh);
    // WSI drag/drop take window-local coordinates and deliver through the
    // platform drag manager.
    const QPoint local = f.list->mapToGlobal(QPoint(15, 15)) -
                         wh->geometry().topLeft();

    const QPlatformDragQtResponse dragResp = QWindowSystemInterface::handleDrag(
        wh, &mime, local, Qt::CopyAction, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::processEvents();
    const QPlatformDropQtResponse dropResp = QWindowSystemInterface::handleDrop(
        wh, &mime, local, Qt::CopyAction, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QVERIFY2(dropResp.isAccepted(), "app-file drop was rejected");
    QCOMPARE(f.pins.pins().size(), 3);
    QCOMPARE(f.pins.pins().at(1).appPath, QDir::cleanPath(appPath_));
    QCOMPARE(f.pins.pins().at(1).label, QFileInfo(appPath_).completeBaseName());

    // Rows rebuilt: one × per pin.
    int rows = 0;
    for (QToolButton *b : f.win.findChildren<QToolButton *>())
      if (b->text() == QStringLiteral("×"))
        ++rows;
    QCOMPARE(rows, 3);

    // Dropping the same app again dedups by path.
    QWindowSystemInterface::handleDrop(wh, &mime, local, Qt::CopyAction,
                                       Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCOMPARE(f.pins.pins().size(), 3);
  }

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
  void junkDragRejected() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
      QSKIP("no window server");
    Fixture f;
    QVERIFY(f.list);

    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(junkPath_)});
    QWindow *wh = f.win.windowHandle();
    QVERIFY(wh);
    const QPoint local = f.list->mapToGlobal(QPoint(15, 15)) -
                         wh->geometry().topLeft();
    const QPlatformDragQtResponse resp = QWindowSystemInterface::handleDrag(
        wh, &mime, local, Qt::CopyAction, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::processEvents();
    QVERIFY(!resp.isAccepted());
  }
#endif
};

QTEST_MAIN(DropAddTest)
#include "test_drop_add.moc"
