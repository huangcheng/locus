#pragma once

#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>

#include <optional>

namespace navi {

enum class ItemRole { Item, Hub, Decoration };

enum class DecorationKind { Ellipse, Path };

struct PlacedItem {
  QString id;
  QRectF bounds;
  int z = 0;
  ItemRole role = ItemRole::Item;
  std::optional<qreal> angle;
  // Optional absolute-coords polygon; when present it wins over bounds in hit-testing.
  std::optional<QPolygonF> shape;
};

struct Decoration {
  QString name;
  DecorationKind kind = DecorationKind::Ellipse;
  QRectF bounds;
  QString styleKey;
};

struct HubInfo {
  QString selectedTitle;
  QString brandSubtitle = QStringLiteral("NAVI");
  QString focusedId;
};

struct SceneModel {
  QVector<PlacedItem> items;
  HubInfo hub;
  QVector<Decoration> decorations;
  QVector<QString> hitOrder;
};

} // namespace navi
