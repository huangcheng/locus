#include "core/UpdateChecker.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QRegularExpression>
#include <QTimer>
#include <QXmlStreamReader>

namespace locus {
namespace {

constexpr int kAutoCheckIntervalSec = 24 * 60 * 60;
constexpr int kFetchTimeoutMs = 15000;

// sparkle namespace + attribute lookup that tolerates both namespace-aware
// and prefix forms.
QString sparkleAttr(const QXmlStreamAttributes &attrs, const char *name) {
  QString v = attrs.value(
      QStringLiteral("http://www.andymatuschak.org/xml-namespaces/sparkle"),
      QLatin1String(name)).toString();
  if (v.isEmpty())
    v = attrs.value(QLatin1String("sparkle:") + QLatin1String(name))
            .toString();
  return v;
}

} // namespace

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent),
      nam_(new QNetworkAccessManager(this)),
      throttle_(new QSettings(QStringLiteral("Locus"), QStringLiteral("Locus"),
                              this)),
      feedUrl_(QStringLiteral(
          "https://github.com/huangcheng/locus/releases/latest/download/"
          "appcast.xml")) {}

UpdateChecker::~UpdateChecker() {
  if (reply_)
    reply_->abort();
}

bool UpdateChecker::shouldAutoCheck() const {
  const QDateTime last =
      throttle_->value(QStringLiteral("updates/lastCheckUtc")).toDateTime();
  return !last.isValid() ||
         last.secsTo(QDateTime::currentDateTimeUtc()) >= kAutoCheckIntervalSec;
}

void UpdateChecker::markChecked() {
  throttle_->setValue(QStringLiteral("updates/lastCheckUtc"),
                      QDateTime::currentDateTimeUtc());
}

void UpdateChecker::check() {
  if (reply_) // a check is already in flight
    return;

  QNetworkRequest request(feedUrl_);
  // GitHub's /releases/latest/download/ is a redirect; follow it.
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setRawHeader("User-Agent",
                       QByteArray("Locus/") + currentVersion_.toUtf8());

  reply_ = nam_->get(request);

  // Feed fetches that hang (captive portal, dead DNS) must not wedge the
  // updater state machine.
  auto *timeout = new QTimer(reply_);
  timeout->setSingleShot(true);
  timeout->start(kFetchTimeoutMs);
  connect(timeout, &QTimer::timeout, reply_, [this] {
    reply_->abort();
  });

  connect(reply_, &QNetworkReply::finished, this, [this] {
    reply_->deleteLater();
    QNetworkReply *reply = reply_;
    reply_ = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
      emit checkFailed(reply->errorString());
      return;
    }
    finish(reply->readAll());
  });
}

void UpdateChecker::finish(const QByteArray &xml) {
  const QVector<UpdateInfo> releases = parseAppcast(xml, hostOs());
  if (releases.isEmpty()) {
    emit checkFailed(tr("No usable release found in the update feed."));
    return;
  }
  const UpdateInfo &latest = releases.first();
  if (isNewer(latest.version, currentVersion_))
    emit updateAvailable(latest);
  else
    emit upToDate();
}

QString UpdateChecker::hostOs() {
#if defined(Q_OS_MAC)
  return QStringLiteral("macos");
#elif defined(Q_OS_WIN)
  return QStringLiteral("windows-x64");
#else
  // Linux intentionally maps to no feed enclosure: package managers own
  // updates there, so the checker reports "no usable release" (checkFailed)
  // instead of offering a foreign installer.
  return QStringLiteral("linux");
#endif
}

bool UpdateChecker::isNewer(const QString &candidate, const QString &current) {
  auto segments = [](const QString &version) {
    // Strip pre-release/build suffixes: 1.0.0-beta+5 → 1.0.0
    QString v = version;
    const int cut = v.indexOf(QRegularExpression(QStringLiteral("[-+]")));
    if (cut >= 0)
      v = v.left(cut);
    QVector<int> out;
    for (const QString &part : v.split(QLatin1Char('.')))
      out.push_back(part.toInt());
    return out;
  };
  const QVector<int> a = segments(candidate);
  const QVector<int> b = segments(current);
  for (int i = 0; i < qMax(a.size(), b.size()); ++i) {
    const int x = i < a.size() ? a[i] : 0;
    const int y = i < b.size() ? b[i] : 0;
    if (x != y)
      return x > y;
  }
  return false;
}

QVector<UpdateInfo> UpdateChecker::parseAppcast(const QByteArray &xml,
                                                const QString &os) {
  QVector<UpdateInfo> out;
  QXmlStreamReader reader(xml);

  while (!reader.atEnd()) {
    if (reader.readNext() != QXmlStreamReader::StartElement ||
        reader.name() != QLatin1String("item"))
      continue;

    // One <item> may carry enclosures for several OSes; take this OS's.
    UpdateInfo info;
    QString title;
    bool matchedOs = false;
    while (!reader.atEnd()) {
      const auto token = reader.readNext();
      if (token == QXmlStreamReader::EndElement &&
          reader.name() == QLatin1String("item"))
        break;
      if (token != QXmlStreamReader::StartElement)
        continue;
      if (reader.name() == QLatin1String("title")) {
        title = reader.readElementText().trimmed();
      } else if (reader.name() == QLatin1String("pubDate")) {
        info.pubDate = QDateTime::fromString(reader.readElementText().trimmed(),
                                             Qt::RFC2822Date);
      } else if (reader.name() == QLatin1String("enclosure")) {
        const QXmlStreamAttributes attrs = reader.attributes();
        if (sparkleAttr(attrs, "os") != os)
          continue;
        matchedOs = true;
        info.version = sparkleAttr(attrs, "version");
        if (info.version.isEmpty())
          info.version = title;
        info.url = QUrl(attrs.value(QLatin1String("url")).toString());
        info.size = attrs.value(QLatin1String("length")).toLongLong();
        info.sha256 = sparkleAttr(attrs, "sha256");
      }
    }
    if (matchedOs && !info.url.isEmpty() && !info.version.isEmpty())
      out.push_back(info);
  }

  // Newest version first; the feed's document order is not contractual.
  std::sort(out.begin(), out.end(), [](const UpdateInfo &a,
                                       const UpdateInfo &b) {
    return isNewer(a.version, b.version);
  });
  return out;
}

} // namespace locus
