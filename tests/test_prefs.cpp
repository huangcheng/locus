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

  void densityRoundTrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")),
                       QSettings::IniFormat);
    locus::Prefs prefs(&settings);
    locus::DensityPrefs d;
    d.minIconSize = 40;
    d.maxPerRing = 10;
    d.cellSize = 96;
    d.cellGap = 14;
    d.iconSize = 48;
    prefs.setDensity(d);
    settings.sync();

    QSettings reader(dir.filePath(QStringLiteral("prefs.ini")),
                     QSettings::IniFormat);
    locus::Prefs loaded(&reader);
    loaded.load();
    QCOMPARE(loaded.density().minIconSize, 40.0);
    QCOMPARE(loaded.density().maxPerRing, 10);
    QCOMPARE(loaded.density().cellSize, 96.0);
    QCOMPARE(loaded.density().cellGap, 14.0);
    QCOMPARE(loaded.density().iconSize, 48.0);
  }
};

QTEST_MAIN(PrefsTest)
#include "test_prefs.moc"
