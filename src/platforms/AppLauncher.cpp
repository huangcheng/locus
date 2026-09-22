#include "platforms/AppLauncher.h"

#include <QDesktopServices>
#include <QUrl>

namespace locus {

bool AppLauncher::launch(const QString &appPath) const {
  if (appPath.isEmpty())
    return false;
  return QDesktopServices::openUrl(QUrl::fromLocalFile(appPath));
}

} // namespace locus
