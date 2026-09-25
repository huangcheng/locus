#pragma once

#include <QDateTime>
#include <QObject>
#include <QUrl>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;
class QSettings;

namespace locus {

/// One release entry for one OS, parsed from the appcast feed.
struct UpdateInfo {
  QString version;
  QUrl url;
  qint64 size = 0;    // bytes, from enclosure @length
  QString sha256;     // hex, our sparkle:sha256 extension
  QDateTime pubDate;
};

/// Fetches the appcast feed and reports whether a newer release exists.
///
/// The pure parsing/comparison helpers are static so tests exercise them
/// without a network. check() itself supports file:// URLs, which the tests
/// use as a fixture server.
class UpdateChecker : public QObject {
  Q_OBJECT
public:
  explicit UpdateChecker(QObject *parent = nullptr);
  ~UpdateChecker() override;

  void setFeedUrl(const QUrl &url) { feedUrl_ = url; }
  void setCurrentVersion(const QString &version) { currentVersion_ = version; }

  /// True when the last check is older than 24h (or never ran). Throttle
  /// state lives in QSettings so it survives restarts.
  bool shouldAutoCheck() const;
  void markChecked();

  /// Fetch + parse + compare. Exactly one signal follows: updateAvailable,
  /// upToDate, or checkFailed.
  void check();

  /// Releases matching `os` (e.g. "windows-x64"), newest version first.
  static QVector<UpdateInfo> parseAppcast(const QByteArray &xml,
                                          const QString &os);
  /// Numeric semver-ish compare: 0.10.0 > 0.9.9; suffixes (-beta, +meta)
  /// stripped; missing segments count as 0.
  static bool isNewer(const QString &candidate, const QString &current);
  /// The enclosure selector for this build: "windows-x64" or "macos".
  static QString hostOs();

signals:
  void updateAvailable(const locus::UpdateInfo &info);
  void upToDate();
  void checkFailed(const QString &error);

private:
  void finish(const QByteArray &xml);

  QNetworkAccessManager *nam_; // owned
  QNetworkReply *reply_ = nullptr;
  QSettings *throttle_; // owned
  QUrl feedUrl_;
  QString currentVersion_;
};
} // namespace locus

Q_DECLARE_METATYPE(locus::UpdateInfo)
