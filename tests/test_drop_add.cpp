// Drag-and-drop pinning: any existing local file or folder can be dropped
// onto the Pins list (apps, shortcuts, documents, folders — the OS handler
// launches them all). Web URLs and nonexistent paths are rejected so the
// cursor never promises a drop that no-ops. Delivery goes through the real
// window-system event path — synthetic sendEvent drags never reach the
// widget's drag handlers.
#include "core/PinStore.h"
#include "core/Prefs.h"
#include "platforms/IconProvider.h"
#include "ui/PrefsWindow.h"

#include <QDir>
#include <QFile>
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

  struct Fixture {
    QTemporaryDir dir;
    QSettings settings;
    locus::Prefs prefs;
    locus::PinStore pins;
    locus::IconProvider icons;
    locus::PrefsWindow win;
    QListWidget *list = nullptr;
    QString filePath;   // a real file that exists
    QString folderPath; // a real folder
    QString appPath;    // a real "app" (.exe on Windows)

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

      filePath = dir.filePath(QStringLiteral("notes.txt"));
      folderPath = dir.filePath(QStringLiteral("Tools"));
#if defined(Q_OS_WIN)
      appPath = dir.filePath(QStringLiteral("Foo.exe"));
#else
      appPath = dir.filePath(QStringLiteral("Foo.app"));
#endif
      for (const QString &p : {filePath, appPath}) {
        QFile f(p);
        f.open(QIODevice::WriteOnly);
        f.write("x");
        f.close();
      }
      QDir().mkpath(folderPath);

      const auto lists = win.findChildren<QListWidget *>();
      if (!lists.isEmpty())
        list = lists.first();
      win.show();
      win.selectPane(1); // Pins
      QCoreApplication::processEvents();
      QCoreApplication::processEvents();
    }
  };

  // Delivers drag + drop at the list's top-left. Returns the drop response.
  static QPlatformDropQtResponse dragDrop(Fixture &f, QMimeData &mime) {
    QWindow *wh = f.win.windowHandle();
    const QPoint local = f.list->mapToGlobal(QPoint(15, 15)) -
                         wh->geometry().topLeft();
    QWindowSystemInterface::handleDrag(wh, &mime, local, Qt::CopyAction,
                                       Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::processEvents();
    const auto resp = QWindowSystemInterface::handleDrop(
        wh, &mime, local, Qt::CopyAction, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    return resp;
  }

private slots:
  void dropPinsFilesAndFolders() {
    // Global-position delivery needs a real windowing platform.
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
      QSKIP("no window server");
    Fixture f;
    QVERIFY(f.list);

    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(f.appPath),
                  QUrl::fromLocalFile(f.filePath),
                  QUrl::fromLocalFile(f.folderPath)});
    const auto resp = dragDrop(f, mime);
    QVERIFY2(resp.isAccepted(), "file/folder drop was rejected");

    QCOMPARE(f.pins.pins().size(), 4); // 1 existing + 3 dropped
    QCOMPARE(f.pins.pins().at(1).label,
             QFileInfo(f.appPath).completeBaseName());
    QCOMPARE(f.pins.pins().at(2).label, QStringLiteral("notes"));
    // Folders take their directory name as the label.
    QCOMPARE(f.pins.pins().at(3).label, QStringLiteral("Tools"));
    QVERIFY(QFileInfo(f.pins.pins().at(3).appPath).isDir());

    // Rows rebuilt: one × per pin.
    int rows = 0;
    for (QToolButton *b : f.win.findChildren<QToolButton *>())
      if (b->text() == QStringLiteral("×"))
        ++rows;
    QCOMPARE(rows, 4);

    // Re-dropping the same paths dedups.
    dragDrop(f, mime);
    QCOMPARE(f.pins.pins().size(), 4);
  }

  void webUrlsAndMissingPathsRejected() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
      QSKIP("no window server");
    Fixture f;
    QVERIFY(f.list);

    QMimeData mime;
    mime.setUrls(
        {QUrl(QStringLiteral("https://example.com/app")),
         QUrl::fromLocalFile(f.dir.filePath(QStringLiteral("ghost.exe")))});
    const auto resp = dragDrop(f, mime);
    QVERIFY(!resp.isAccepted());
    QCOMPARE(f.pins.pins().size(), 1); // only the pre-existing pin
  }
};

QTEST_MAIN(DropAddTest)
#include "test_drop_add.moc"
