#pragma once

#include <QPixmap>
#include <QString>

namespace locus {

/// Renders the real macOS icon for an app bundle at `size` px via
/// NSWorkspace (full-resolution representations, unlike QFileIconProvider).
QPixmap macIconForPath(const QString &path, int size);

} // namespace locus
