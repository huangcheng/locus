#include "platform/IconProvider.h"

#include <QFileIconProvider>
#include <QFileInfo>

#ifdef Q_OS_MAC
#include "platform/MacIcon.h"
#endif

namespace locus {

QIcon IconProvider::iconForPath(const QString &appPath) const {
  if (appPath.isEmpty())
    return {};
#ifdef Q_OS_MAC
  // NSWorkspace gives full-resolution app icons (up to 1024px);
  // QFileIconProvider only offers small generic representations.
  const QPixmap pm = macIconForPath(appPath, 256);
  if (!pm.isNull())
    return QIcon(pm);
#endif
  QFileIconProvider provider;
  return provider.icon(QFileInfo(appPath));
}

} // namespace locus
