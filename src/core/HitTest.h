#pragma once

#include "core/SceneModel.h"

#include <QPointF>
#include <QString>

namespace navi {

/// First Item in hitOrder whose shape (or bounds) contains point; else empty.
inline QString hitTest(const SceneModel &scene, QPointF p) {
  for (const QString &id : scene.hitOrder) {
    for (const auto &it : scene.items) {
      if (it.id != id || it.role != ItemRole::Item)
        continue;
      if (it.shape && !it.shape->isEmpty()) {
        if (it.shape->containsPoint(p, Qt::OddEvenFill))
          return id;
      } else if (it.bounds.contains(p)) {
        return id;
      }
    }
  }
  return {};
}

} // namespace navi
