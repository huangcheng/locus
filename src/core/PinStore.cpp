#include "core/PinStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace navi {

PinStore::PinStore(QSettings *settings) : settings_(settings) {}

void PinStore::setPins(QVector<Pin> pins) {
  pins_ = std::move(pins);
  save();
}

void PinStore::addPin(Pin pin) {
  const auto it = std::find_if(pins_.begin(), pins_.end(),
                               [&](const Pin &p) { return p.id == pin.id; });
  if (it != pins_.end())
    *it = std::move(pin);
  else
    pins_.push_back(std::move(pin));
  save();
}

void PinStore::removePin(const QString &id) {
  pins_.erase(std::remove_if(pins_.begin(), pins_.end(),
                             [&](const Pin &p) { return p.id == id; }),
              pins_.end());
  save();
}

void PinStore::movePin(int from, int to) {
  if (from < 0 || from >= pins_.size() || to < 0 || to >= pins_.size() ||
      from == to)
    return;
  const Pin pin = pins_.takeAt(from);
  pins_.insert(to, pin);
  save();
}

void PinStore::load() {
  pins_.clear();
  if (!settings_)
    return;
  const QByteArray raw =
      settings_->value(QStringLiteral("pins/json")).toByteArray();
  if (raw.isEmpty())
    return;
  const auto doc = QJsonDocument::fromJson(raw);
  if (!doc.isArray())
    return;
  for (const auto &value : doc.array()) {
    const auto obj = value.toObject();
    Pin pin;
    pin.id = obj.value(QStringLiteral("id")).toString();
    pin.appPath = obj.value(QStringLiteral("appPath")).toString();
    pin.label = obj.value(QStringLiteral("label")).toString();
    pin.iconKey = obj.value(QStringLiteral("iconKey")).toString();
    if (!pin.id.isEmpty())
      pins_.push_back(pin);
  }
}

void PinStore::save() const {
  if (!settings_)
    return;
  QJsonArray array;
  for (const auto &pin : pins_) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), pin.id);
    obj.insert(QStringLiteral("appPath"), pin.appPath);
    obj.insert(QStringLiteral("label"), pin.label);
    obj.insert(QStringLiteral("iconKey"), pin.iconKey);
    array.append(obj);
  }
  settings_->setValue(QStringLiteral("pins/json"),
                      QJsonDocument(array).toJson(QJsonDocument::Compact));
}

} // namespace navi
