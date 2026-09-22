#pragma once

#include <QIcon>
#include <QString>

namespace locus {

class IconProvider {
public:
  QIcon iconForPath(const QString &appPath) const;
};

} // namespace locus
