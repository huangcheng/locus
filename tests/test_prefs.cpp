#include "core/PinStore.h"
#include "core/Prefs.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QtTest>

class PrefsTest : public QObject {
  Q_OBJECT
private slots:
  void pinsRoundTrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")),
                       QSettings::IniFormat);
    locus::PinStore store(&settings);
    locus::Pin pin;
    pin.id = QStringLiteral("safari");
    pin.label = QStringLiteral("Safari");
    pin.appPath = QStringLiteral("/Applications/Safari.app");
    store.addPin(pin);
    settings.sync();

    QSettings reader(dir.filePath(QStringLiteral("prefs.ini")),
                     QSettings::IniFormat);
    locus::PinStore loaded(&reader);
    loaded.load();
    QCOMPARE(loaded.pins().size(), 1);
    QCOMPARE(loaded.pins().at(0).label, QStringLiteral("Safari"));
  }

  void addPinIgnoresDuplicatePath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")),
                       QSettings::IniFormat);
    locus::PinStore store(&settings);
    locus::Pin first;
    first.id = QStringLiteral("id-1");
    first.label = QStringLiteral("Safari");
    first.appPath = QStringLiteral("/Applications/Safari.app");
    store.addPin(first);

    locus::Pin dup;
    dup.id = QStringLiteral("id-2");
    dup.label = QStringLiteral("Safari again");
    dup.appPath = QStringLiteral("/Applications/Safari.app");
    store.addPin(dup);

    // The second add must not create a second slot for the same app, and
    // must leave the existing entry (id, position, label) untouched.
    QCOMPARE(store.pins().size(), 1);
    QCOMPARE(store.pins().first().id, QStringLiteral("id-1"));
    QCOMPARE(store.pins().first().label, QStringLiteral("Safari"));
  }

  void densityRoundTrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")),
                       QSettings::IniFormat);
    locus::Prefs prefs(&settings);
    locus::DensityPrefs d;
    d.cellSize = 96;
    d.cellGap = 14;
    d.iconSize = 48;
    prefs.setDensity(d);
    settings.sync();

    QSettings reader(dir.filePath(QStringLiteral("prefs.ini")),
                     QSettings::IniFormat);
    locus::Prefs loaded(&reader);
    loaded.load();
    QCOMPARE(loaded.density().cellSize, 96.0);
    QCOMPARE(loaded.density().cellGap, 14.0);
    QCOMPARE(loaded.density().iconSize, 48.0);
  }

  void languageRoundTrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")),
                       QSettings::IniFormat);
    locus::Prefs prefs(&settings);
    prefs.setLanguage(2);
    settings.sync();

    QSettings reader(dir.filePath(QStringLiteral("prefs.ini")),
                     QSettings::IniFormat);
    locus::Prefs loaded(&reader);
    loaded.load();
    QCOMPARE(loaded.language(), 2);
  }

  void languageOutOfRangeFallsBackToSystem() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")),
                       QSettings::IniFormat);
    settings.setValue(QStringLiteral("language"), 7);
    settings.sync();

    locus::Prefs prefs(&settings);
    prefs.load();
    QCOMPARE(prefs.language(), 0);
  }

  void hotkeyMigration() {
#ifdef Q_OS_WIN
    const QString platformDefault = QStringLiteral("Ctrl+Alt+Space");
#else
    const QString platformDefault = QStringLiteral("Meta+Space");
#endif
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // The pre-macOS-aware default migrates to the platform default…
    QSettings legacy(dir.filePath(QStringLiteral("prefs.ini")),
                     QSettings::IniFormat);
    legacy.setValue(QStringLiteral("hotkey"), QStringLiteral("Ctrl+Space"));
    legacy.sync();
    locus::Prefs migrated(&legacy);
    migrated.load();
    QCOMPARE(migrated.hotkey().toString(), platformDefault);

    // …while a user-recorded shortcut passes through untouched.
    QSettings custom(dir.filePath(QStringLiteral("custom.ini")),
                     QSettings::IniFormat);
    custom.setValue(QStringLiteral("hotkey"), QStringLiteral("Shift+F3"));
    custom.sync();
    locus::Prefs kept(&custom);
    kept.load();
    QCOMPARE(kept.hotkey().toString(), QStringLiteral("Shift+F3"));
  }

  void pinRemoveAndMovePersist() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")),
                       QSettings::IniFormat);
    locus::PinStore store(&settings);
    for (int i = 0; i < 3; ++i) {
      locus::Pin p;
      p.id = QString::fromLatin1("p%1").arg(i);
      p.label = QStringLiteral("App %1").arg(i);
      p.appPath = QStringLiteral("/tmp/app%1.app").arg(i);
      store.addPin(p);
    }
    store.removePin(QStringLiteral("p1"));
    store.movePin(0, 1); // [p0, p2] -> [p2, p0]
    settings.sync();

    QSettings reader(dir.filePath(QStringLiteral("prefs.ini")),
                     QSettings::IniFormat);
    locus::PinStore loaded(&reader);
    loaded.load();
    QCOMPARE(loaded.pins().size(), 2);
    QCOMPARE(loaded.pins().at(0).id, QStringLiteral("p2"));
    QCOMPARE(loaded.pins().at(1).id, QStringLiteral("p0"));
  }

  void pinUpsertById() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")),
                       QSettings::IniFormat);
    locus::PinStore store(&settings);
    locus::Pin a;
    a.id = QStringLiteral("same-id");
    a.label = QStringLiteral("Old");
    a.appPath = QStringLiteral("/Applications/Old.app");
    store.addPin(a);
    locus::Pin b;
    b.id = QStringLiteral("same-id");
    b.label = QStringLiteral("New");
    b.appPath = QStringLiteral("/Applications/New.app");
    store.addPin(b);

    QCOMPARE(store.pins().size(), 1);
    QCOMPARE(store.pins().first().label, QStringLiteral("New"));
    QCOMPARE(store.pins().first().appPath, QStringLiteral("/Applications/New.app"));
  }

  void pinLoadRepairsTrailingSlashAndBackfillsLabel() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")),
                       QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("pins/json"),
        QByteArray("[{\"id\":\"r1\",\"appPath\":\"/Applications/X.app/\","
                   "\"label\":\"\",\"iconKey\":\"/Applications/X.app/\"}]"));
    settings.sync();

    locus::PinStore store(&settings);
    store.load();
    QCOMPARE(store.pins().size(), 1);
    QCOMPARE(store.pins().first().appPath, QStringLiteral("/Applications/X.app"));
    QCOMPARE(store.pins().first().label, QStringLiteral("X"));

    // The repair must be persisted, not just applied in memory.
    QSettings reader(dir.filePath(QStringLiteral("prefs.ini")),
                     QSettings::IniFormat);
    const QByteArray persisted =
        reader.value(QStringLiteral("pins/json")).toByteArray();
    QVERIFY(!persisted.contains("/Applications/X.app/\""));
    QVERIFY(persisted.contains("/Applications/X.app\""));
  }
};

QTEST_MAIN(PrefsTest)
#include "test_prefs.moc"
