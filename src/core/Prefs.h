#pragma once

#include "core/LayoutStrategy.h"
#include "core/Types.h"

#include <QKeySequence>
#include <QSettings>

namespace locus {

class Prefs {
public:
  explicit Prefs(QSettings *settings);

  Appearance appearance() const { return appearance_; }
  void setAppearance(Appearance a);

  StyleId styleId() const { return styleId_; }
  void setStyleId(StyleId id);

  DensityPrefs density() const { return density_; }
  void setDensity(DensityPrefs d);

  QKeySequence hotkey() const { return hotkey_; }
  void setHotkey(QKeySequence seq);

  void load();
  void save() const;

private:
  QSettings *settings_ = nullptr;
  Appearance appearance_ = Appearance::System;
  StyleId styleId_ = StyleId::Cellular;
  DensityPrefs density_;
  // Meta on macOS = the real Control key (Qt::CTRL maps to Command there).
  QKeySequence hotkey_ = QKeySequence(Qt::META | Qt::Key_Space);
};

} // namespace locus
