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
    navi::PinStore store(&settings);
    navi::Pin pin;
    pin.id = QStringLiteral("safari");
    pin.label = QStringLiteral("Safari");
    pin.appPath = QStringLiteral("/Applications/Safari.app");
    store.addPin(pin);
    settings.sync();

    QSettings reader(dir.filePath(QStringLiteral("prefs.ini")),
                     QSettings::IniFormat);
    navi::PinStore loaded(&reader);
    loaded.load();
    QCOMPARE(loaded.pins().size(), 1);
    QCOMPARE(loaded.pins().at(0).label, QStringLiteral("Safari"));
  }

  void densityRoundTrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("prefs.ini")),
                       QSettings::IniFormat);
    navi::Prefs prefs(&settings);
    navi::DensityPrefs d;
    d.minIconSize = 40;
    d.maxPerRing = 10;
    prefs.setDensity(d);
    settings.sync();

    QSettings reader(dir.filePath(QStringLiteral("prefs.ini")),
                     QSettings::IniFormat);
    navi::Prefs loaded(&reader);
    loaded.load();
    QCOMPARE(loaded.density().minIconSize, 40.0);
    QCOMPARE(loaded.density().maxPerRing, 10);
  }
};

QTEST_MAIN(PrefsTest)
#include "test_prefs.moc"
