#include "platform/AppLauncher.h"

#include <QDesktopServices>
#include <QUrl>

namespace navi {

bool AppLauncher::launch(const QString &appPath) const {
  if (appPath.isEmpty())
    return false;
  return QDesktopServices::openUrl(QUrl::fromLocalFile(appPath));
}

} // namespace navi
