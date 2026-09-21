#include "core/Prefs.h"

namespace navi {

Prefs::Prefs(QSettings *settings) : settings_(settings) {}

void Prefs::setAppearance(Appearance a) {
  appearance_ = a;
  save();
}

void Prefs::setStyleId(StyleId id) {
  styleId_ = id;
  save();
}

void Prefs::setDensity(DensityPrefs d) {
  density_ = d;
  save();
}

void Prefs::setHotkey(QKeySequence seq) {
  hotkey_ = std::move(seq);
  save();
}

void Prefs::load() {
  if (!settings_)
    return;
  const int appearanceInt =
      settings_->value(QStringLiteral("appearance"), static_cast<int>(Appearance::System))
          .toInt();
  appearance_ = (appearanceInt >= 0 &&
                 appearanceInt <= static_cast<int>(Appearance::System))
                    ? static_cast<Appearance>(appearanceInt)
                    : Appearance::System;
  const int styleInt =
      settings_->value(QStringLiteral("style"), static_cast<int>(StyleId::Cellular))
          .toInt();
  styleId_ = (styleInt >= 0 && styleInt <= static_cast<int>(StyleId::Pie))
                 ? static_cast<StyleId>(styleInt)
                 : StyleId::Cellular;
  density_.minIconSize =
      settings_->value(QStringLiteral("density/minIconSize"), 34.0).toDouble();
  density_.maxPerRing =
      settings_->value(QStringLiteral("density/maxPerRing"), 12).toInt();
  density_.gap = settings_->value(QStringLiteral("density/gap"), 12.0).toDouble();
  density_.widgetSize =
      settings_->value(QStringLiteral("density/widgetSize"), 560.0).toDouble();
  hotkey_ = QKeySequence(
      settings_->value(QStringLiteral("hotkey"), QStringLiteral("Ctrl+Space"))
          .toString());
}

void Prefs::save() const {
  if (!settings_)
    return;
  settings_->setValue(QStringLiteral("appearance"), static_cast<int>(appearance_));
  settings_->setValue(QStringLiteral("style"), static_cast<int>(styleId_));
  settings_->setValue(QStringLiteral("density/minIconSize"), density_.minIconSize);
  settings_->setValue(QStringLiteral("density/maxPerRing"), density_.maxPerRing);
  settings_->setValue(QStringLiteral("density/gap"), density_.gap);
  settings_->setValue(QStringLiteral("density/widgetSize"), density_.widgetSize);
  settings_->setValue(QStringLiteral("hotkey"), hotkey_.toString());
}

} // namespace navi
