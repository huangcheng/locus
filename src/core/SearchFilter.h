#pragma once

#include "core/Pin.h"

#include <QVector>

namespace locus {

/// Case-insensitive label substring filter for overlay type-to-search.
/// Empty query returns the input unchanged.
inline QVector<Pin> filterPins(const QVector<Pin> &pins,
                               const QString &query) {
  if (query.isEmpty())
    return pins;
  QVector<Pin> out;
  for (const auto &p : pins)
    if (p.label.contains(query, Qt::CaseInsensitive))
      out.push_back(p);
  return out;
}

} // namespace locus
