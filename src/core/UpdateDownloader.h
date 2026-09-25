#pragma once

#include <QObject>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

namespace locus {

struct UpdateInfo;

/// Downloads the installer referenced by an UpdateInfo to the temp dir and
/// verifies its SHA256 against the feed. A file that fails verification is
/// deleted and NEVER reported via finished() — the install path only ever
/// sees a verified file.
class UpdateDownloader : public QObject {
  Q_OBJECT
public:
  explicit UpdateDownloader(QObject *parent = nullptr);
  ~UpdateDownloader() override;

  void download(const UpdateInfo &info);
  void cancel();

  /// Pure helper, unit-tested directly. Constant string compare is overkill
  /// here (the threat model is a tampered download, not a timing side
  /// channel on the local machine).
  static bool verifySha256(const QString &path, const QString &expectedHex);

signals:
  /// total <= 0 means the server sent no Content-Length.
  void progress(qint64 received, qint64 total);
  void finished(const QString &filePath);
  void failed(const QString &error);

private:
  void cleanup(bool deleteFile);

  QNetworkAccessManager *nam_; // owned
  QNetworkReply *reply_ = nullptr;
  QFile *file_ = nullptr;
  QString targetPath_;
  QString expectedSha256_;
};

} // namespace locus
