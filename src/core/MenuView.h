#pragma once

#include "core/SceneModel.h"

class QWidget;

namespace navi {

class MenuView {
public:
  virtual ~MenuView() = default;
  virtual void setScene(const SceneModel &scene) = 0;
  virtual QWidget *widget() = 0;
};

} // namespace navi
