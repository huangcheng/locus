#pragma once

#include "core/Pin.h"

#include <QSettings>
#include <QVector>

namespace locus {

class PinStore {
public:
  explicit PinStore(QSettings *settings);

  QVector<Pin> pins() const { return pins_; }
  void setPins(QVector<Pin> pins);
  void addPin(Pin pin);
  void removePin(const QString &id);
  void movePin(int from, int to);

  void load();
  void save() const;

private:
  QSettings *settings_ = nullptr;
  QVector<Pin> pins_;
};

} // namespace locus
