// UpdateChecker/UpdateDownloader: feed parsing, version comparison, and
// download integrity. The network path is exercised over file:// URLs, which
// QNetworkAccessManager serves locally — no fixture server needed.
#include "core/UpdateChecker.h"
#include "core/UpdateDownloader.h"

#include <QCryptographicHash>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

using namespace locus;

namespace {

// Fixture lives in a file: moc's raw-string lexer chokes on multi-line
// R"(...)" literals and then silently produces an empty .moc.
QByteArray appcastFixture() {
  QFile f(QString::fromUtf8(QT_TESTCASE_SOURCEDIR) +
          QStringLiteral("/tests/fixtures/appcast.xml"));
  if (!f.open(QIODevice::ReadOnly))
    return {};
  return f.readAll();
}

} // namespace

class UpdateTest : public QObject {
  Q_OBJECT
private slots:
  void parseSelectsOsAndSortsNewestFirst() {
    const QByteArray feed = appcastFixture();
    QVERIFY(!feed.isEmpty());
    const QVector<UpdateInfo> win =
        UpdateChecker::parseAppcast(feed, QStringLiteral("windows-x64"));
    QCOMPARE(win.size(), 2);
    QCOMPARE(win.first().version, QStringLiteral("0.2.0"));
    QCOMPARE(win.first().url,
             QUrl(QStringLiteral(
                 "https://example.com/Locus-0.2.0-windows-x64-setup.exe")));
    QCOMPARE(win.first().sha256, QStringLiteral("aabbcc"));
    QCOMPARE(win.first().size, qint64(24129834));
    QCOMPARE(win.last().version, QStringLiteral("0.1.5"));

    const QVector<UpdateInfo> mac =
        UpdateChecker::parseAppcast(feed, QStringLiteral("macos"));
    QCOMPARE(mac.size(), 1);
    QCOMPARE(mac.first().url,
             QUrl(QStringLiteral("https://example.com/Locus-0.2.0-macos.dmg")));
  }

  void versionComparison() {
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("0.2.0"),
                                   QStringLiteral("0.1.0")));
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("0.10.0"),
                                   QStringLiteral("0.9.9")));
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("1.0.0"),
                                   QStringLiteral("0.9.9")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral("0.1.0"),
                                    QStringLiteral("0.1.0")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral("0.1.0"),
                                    QStringLiteral("0.2.0")));
    // Suffixes are stripped: 1.0.0-beta beats 0.9.9, ties 1.0.0.
    QVERIFY(UpdateChecker::isNewer(QStringLiteral("1.0.0-beta"),
                                   QStringLiteral("0.9.9")));
    QVERIFY(!UpdateChecker::isNewer(QStringLiteral("1.0.0-beta"),
                                    QStringLiteral("1.0.0")));
  }

  void sha256Verification() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("file.bin"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("locus payload");
    f.close();

    const QString hex = QString::fromLatin1(
        QCryptographicHash::hash("locus payload", QCryptographicHash::Sha256)
            .toHex());
    QVERIFY(UpdateDownloader::verifySha256(path, hex));
    QVERIFY(UpdateDownloader::verifySha256(path, hex.toUpper()));
    QVERIFY(!UpdateDownloader::verifySha256(path, QStringLiteral("00")));
    QVERIFY(!UpdateDownloader::verifySha256(
        dir.filePath(QStringLiteral("missing.bin")), hex));
  }

  void checkerReportsNewerRelease() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString feedPath = dir.filePath(QStringLiteral("appcast.xml"));
    QFile f(feedPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(appcastFixture());
    f.close();

    UpdateChecker checker;
    checker.setFeedUrl(QUrl::fromLocalFile(feedPath));
    checker.setCurrentVersion(QStringLiteral("0.1.0"));
    QSignalSpy spy(&checker, &UpdateChecker::updateAvailable);
    checker.check();
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
    const UpdateInfo info =
        qvariant_cast<UpdateInfo>(spy.first().first());
    QCOMPARE(info.version, QStringLiteral("0.2.0"));
    QCOMPARE(info.sha256, QStringLiteral("aabbcc"));
  }

  void checkerReportsUpToDate() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString feedPath = dir.filePath(QStringLiteral("appcast.xml"));
    QFile f(feedPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(appcastFixture());
    f.close();

    UpdateChecker checker;
    checker.setFeedUrl(QUrl::fromLocalFile(feedPath));
    checker.setCurrentVersion(QStringLiteral("0.2.0"));
    QSignalSpy spy(&checker, &UpdateChecker::upToDate);
    checker.check();
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
  }

  void malformedFeedFails() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString feedPath = dir.filePath(QStringLiteral("appcast.xml"));
    QFile f(feedPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("<rss><channel><item>oops no enclosure");
    f.close();

    UpdateChecker checker;
    checker.setFeedUrl(QUrl::fromLocalFile(feedPath));
    checker.setCurrentVersion(QStringLiteral("0.1.0"));
    QSignalSpy spy(&checker, &UpdateChecker::checkFailed);
    checker.check();
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
  }
};

QTEST_MAIN(UpdateTest)
#include "test_update.moc"
