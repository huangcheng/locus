#pragma once

#include <QIcon>
#include <QString>

namespace navi {

class IconProvider {
public:
  QIcon iconForPath(const QString &appPath) const;
};

} // namespace navi
