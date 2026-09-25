#include "core/UpdateDownloader.h"

#include "core/UpdateChecker.h"

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>

namespace locus {

UpdateDownloader::UpdateDownloader(QObject *parent)
    : QObject(parent), nam_(new QNetworkAccessManager(this)) {}

UpdateDownloader::~UpdateDownloader() { cancel(); }

void UpdateDownloader::download(const UpdateInfo &info) {
  if (reply_)
    cancel();

  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  targetPath_ =
      QStringLiteral("%1/locus-update-%2.exe").arg(dir, info.version);
  expectedSha256_ = info.sha256;

  QFile::remove(targetPath_); // discard any stale partial download
  file_ = new QFile(targetPath_, this);
  if (!file_->open(QIODevice::WriteOnly)) {
    const QString error = file_->errorString();
    cleanup(true);
    emit failed(error);
    return;
  }

  QNetworkRequest request(info.url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setRawHeader("User-Agent", QByteArray("Locus updater"));
  reply_ = nam_->get(request);

  // Stalled-transfer guard, reset by activity: slow networks are fine, dead
  // ones must not wedge the state machine forever.
  auto *watchdog = new QTimer(reply_);
  watchdog->setSingleShot(true);
  watchdog->start(60000);
  connect(reply_, &QNetworkReply::downloadProgress, watchdog,
          [watchdog] { watchdog->start(60000); });
  connect(watchdog, &QTimer::timeout, reply_,
          [this] { reply_->abort(); });

  connect(reply_, &QNetworkReply::readyRead, this, [this] {
    file_->write(reply_->readAll());
  });
  connect(reply_, &QNetworkReply::downloadProgress, this,
          [this](qint64 received, qint64 total) {
            emit progress(received, total);
          });
  connect(reply_, &QNetworkReply::finished, this, [this] {
    QNetworkReply *reply = reply_;
    reply_ = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      const QString error = reply->errorString();
      cleanup(true);
      emit failed(error);
      return;
    }
    file_->flush();
    file_->close();

    if (!verifySha256(targetPath_, expectedSha256_)) {
      const QString error =
          tr("Downloaded file failed integrity verification.");
      cleanup(true); // never leave a tampered/corrupt installer on disk
      emit failed(error);
      return;
    }
    const QString path = targetPath_;
    cleanup(false);
    emit finished(path);
  });
}

void UpdateDownloader::cancel() {
  if (reply_) {
    reply_->abort();
    reply_->deleteLater();
    reply_ = nullptr;
  }
  cleanup(true);
}

void UpdateDownloader::cleanup(bool deleteFile) {
  if (file_) {
    if (file_->isOpen())
      file_->close();
    if (deleteFile && !targetPath_.isEmpty() && QFile::exists(targetPath_))
      QFile::remove(targetPath_);
    file_->deleteLater();
    file_ = nullptr;
  }
  if (deleteFile)
    targetPath_.clear();
}

bool UpdateDownloader::verifySha256(const QString &path,
                                    const QString &expectedHex) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&f))
    return false;
  return hash.result().toHex().compare(expectedHex.trimmed().toLatin1(),
                                       Qt::CaseInsensitive) == 0;
}

} // namespace locus
